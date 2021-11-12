set -xeuo pipefail
min_threads=1
max_threads=160
sample_time=4

# stdbuf -o0 ./bench-before --max-threads=$max_threads --sample-time-seconds=$sample_time --min-threads=$min_threads > before.json
stdbuf -o0 ./bench --max-threads=$max_threads --sample-time-seconds=$sample_time --min-threads=$min_threads > ${1}
