#include <catch2/catch.hpp>

#include <bson/bson.h>
#include <bson2/view.h>

#include <fstream>
#include <sstream>

std::string
read_file (std::string filepath)
{
   std::ifstream infile;
   infile.open (filepath, std::ios::binary);
   REQUIRE (infile.is_open ());
   std::stringstream strm;
   strm << infile.rdbuf ();
   return strm.str ();
}

int
walk (bson_view v)
{
   int acc = 0;
   for (auto it = bson_begin (v); not bson_iterator_done (it);
        it = bson_next (it)) {
      auto subdoc = bson_iterator_document (it);
      if (subdoc.data) {
         acc += walk (subdoc);
      }
      ++acc;
   }
   return acc;
}

int
walk_old (bson_iter_t *iter)
{
   int acc = 0;
   while (bson_iter_next (iter)) {
      bson_iter_t child;
      if (bson_iter_recurse (iter, &child)) {
         acc += walk_old (&child);
      }
      acc++;
   }
   return acc;
}


TEST_CASE ("Benchmarks")
{
   auto buf = read_file ("data.bson");
   bson_t bson;
   REQUIRE (
      bson_init_static (&bson, (const uint8_t *) buf.data (), buf.size ()));

   bson_view view = bson_view_from_bson_t (&bson);

   BENCHMARK ("bson_view walk")
   {
      auto n = walk (view);
      CHECK (n == 4558);
      return n;
   };

   BENCHMARK ("bson_iter_t()")
   {
      bson_iter_t it;
      bson_iter_init (&it, &bson);
      return walk_old (&it);
   };
}
