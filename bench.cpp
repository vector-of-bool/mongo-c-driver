#include <mongoc/mongoc.h>

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <cassert>
#include <fstream>
#include <filesystem>

using namespace std;

namespace fs = std::filesystem;

struct operation_executor {
   mongoc_client_pool_t *pool;
   mongoc_client_t *client = mongoc_client_pool_pop (pool);
   bson_t opts;
   bson_t filter;

   operation_executor (mongoc_client_pool_t *pool_) : pool{pool_}
   {
      bson_init (&opts);
      bson_init (&filter);
      BSON_APPEND_INT32 (&filter, "_id", 0);
   }

   ~operation_executor ()
   {
      mongoc_client_pool_push (pool, client);
      bson_destroy (&filter);
      bson_destroy (&opts);
   }

   void
   run_once ()
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "test", "coll");
      mongoc_cursor_t *cursor =
         mongoc_collection_find_with_opts (coll, &filter, &opts, nullptr);
      assert (!mongoc_cursor_error (cursor, nullptr));

      const bson_t *doc;
      mongoc_cursor_next (cursor, &doc);
      assert (!mongoc_cursor_error (cursor, nullptr));

      mongoc_cursor_destroy (cursor);
      mongoc_collection_destroy (coll);
   }

   void
   run_n_times (int n)
   {
      while (n--) {
         run_once ();
      }
   }
};

struct bench_state {
   atomic_bool stop{false};
   atomic_int64_t n_operations{0};

   mongoc_uri_t *uri = mongoc_uri_new (nullptr);
   mongoc_client_pool_t *pool = mongoc_client_pool_new (uri);

   ~bench_state ()
   {
      mongoc_uri_destroy (uri);
      mongoc_client_pool_destroy (pool);
      mongoc_cleanup ();
   }
};

void
run_thread (bench_state &state)
{
   operation_executor op{state.pool};
   while (!state.stop) {
      const int op_group_size = 73;
      op.run_n_times (op_group_size);
      state.n_operations.fetch_add (op_group_size);
   }
}

void
from_str (int &out, std::string s)
{
   out = std::stoi (s);
}

void
from_str (double &out, std::string s)
{
   out = std::stod (s);
}

void
from_str (std::string &out, std::string s)
{
   out = std::move (s);
}

template <typename T>
static T
parse_arg (vector<string> &args, std::string prefix, T def_val)
{
   T ret = def_val;
   const std::string flag = "--" + prefix + "=";
   auto it = std::find_if (args.cbegin (), args.cend (), [&] (auto &&arg) {
      return arg.find (flag) == 0;
   });
   if (it == args.cend ()) {
      return ret;
   }
   auto valstr = it->substr (flag.size ());
   from_str (ret, valstr);
   args.erase (it);
   return ret;
}

static void
print_help (std::string program)
{
   cerr << "Usage: " << program << "\n"
        << "  [--max-threads=10]\n"
        << "  [--min-threads=1]\n"
        << "  [--sample-time-seconds=5]\n"
        << "  [--step-scale=1.1]\n"
        << "  [--help]\n";
}

void
write_file (fs::path file, std::string content)
{
   std::ofstream outfile{file, std::ios::binary};
   assert (outfile.good ());
   outfile << content;
}

void
setup_cgroup (fs::path root, int period, int quota)
{
   if (root == "auto") {
      root = "/sys/fs/cgroup/unified/user.slice/";
      int uid = ::getuid ();
      auto substr1 = "user-" + std::to_string (uid) + ".slice";
      auto substr2 = "user@" + std::to_string (uid) + ".service";
   }
   auto threads_cg = root / "mc-bench2-threads";
   fs::create_directories (threads_cg);
   write_file (threads_cg / "cgroup.type", "threaded");
   write_file (threads_cg / "cgroup.procs", std::to_string (getpid ()));
   std::ifstream infile{"/proc/self/cgroup", std::ios::binary};
   std::cerr << "I am in: " << infile.rdbuf () << '\n';
}

int
do_main (vector<string> argv)
{
   const auto program = argv[0];
   argv.erase (argv.begin ());
   const auto max_threads = parse_arg<int> (argv, "max-threads", 10);
   const auto min_threads = parse_arg<int> (argv, "min-threads", 1);
   const auto sample_time_sec = parse_arg<int> (argv, "sample-time-seconds", 5);
   const auto scaling_factor = parse_arg<double> (argv, "step-scale", 1.1);

   if (std::find (argv.cbegin (), argv.cend (), "--help") != argv.cend ()) {
      print_help (program);
      return 0;
   }

   for (auto &arg : argv) {
      cerr << "Unknown argument: " << arg << '\n';
   }
   if (!argv.empty ()) {
      print_help (program);
      return 2;
   }

   if (max_threads < min_threads) {
      cerr << "max-threads must be greater or equal to min-threads\n";
      return 2;
   }
   if (scaling_factor <= 1) {
      cerr << "--step-sze must be greater than 1\n";
      return 2;
   }
   if (sample_time_sec <= 0) {
      cerr << "--sample-time-seconds must be greater than zero\n";
      return 2;
   }

   cerr << "/* Running up to " << max_threads << " threads, giving "
        << sample_time_sec << " second(s) per sampling */\n"
        << flush;

   cout << "{\n";
   cout << "  \"interval\": " << sample_time_sec << ",\n";
   cout << "  \"samples\": [\n";

   string indent = "    ";
   vector<thread> threads;
   bench_state state;
   double threads_this_round = min_threads;
   while (threads.size () < static_cast<size_t> (max_threads)) {
      do {
         threads.emplace_back ([&state] { run_thread (state); });
      } while (threads.size () < threads_this_round &&
               threads.size () < static_cast<size_t> (max_threads));
      threads_this_round = threads.size () * 1.1;

      cerr << indent << "/* Running with " << threads.size () << " threads */"
           << endl;

      // Give the thread some time to warm up
      this_thread::sleep_for (chrono::milliseconds (500));

      // Go:
      auto start_time = chrono::high_resolution_clock::now ();
      auto stop_time =
         start_time + chrono::milliseconds (int (sample_time_sec * 1'000));
      state.n_operations.store (0);
      std::stringstream strm;
      while (chrono::high_resolution_clock::now () < stop_time) {
         strm << '\r' << indent << "/* " << state.n_operations << " ops */ ";
         cerr << strm.str ();
         strm.str ("");
         this_thread::sleep_for (chrono::milliseconds (100));
      }
      cerr << '\n';

      auto end_time = chrono::high_resolution_clock::now ();
      int64_t n_ops = state.n_operations;
      auto duration = end_time - start_time;
      double n_usec =
         chrono::duration_cast<chrono::microseconds> (duration).count ();

      auto time_sec = n_usec / 1'000'000;
      auto ops_per_sec = n_ops / time_sec;
      cerr << indent << "/* " << n_ops << " ops executed in " << time_sec
           << "sec */" << std::endl;
      cout << indent << "{\"n_threads\": " << threads.size ()
           << ", \"rate\": " << ops_per_sec << "}";
      if (threads.size () < static_cast<size_t> (max_threads)) {
         cout << ',';
      }
      cout << '\n';
   }
   state.stop = true;
   for (auto &t : threads) {
      t.join ();
   }
   cout << "  ]\n}\n";
   return 0;
}

int
main (int argc, char **argv)
{
   try {
      return do_main (std::vector<std::string>{argv, argv + argc});
   } catch (const std::exception &e) {
      cerr << e.what () << '\n';
      return 2;
   }
}