#include <atomic>
#include <span>
#include <thread>
#include <vector>

#include "./threads.hpp"

#include "../../include/measurement.hpp"

#include "./common.hpp"

auto nqueens(threads_args args) -> unsigned int
{
  measurements benchmarker;

  auto size = args.size;
  auto prefix_length = args.prefix_length;
  std::atomic<unsigned long> res;
  //unsigned long res;

  for(int rep = 0; rep < args.repetitions; ++rep) {

    res = 0;
    benchmarker.start_repetition(rep);

    auto start = std::chrono::high_resolution_clock::now();

    auto prefixes = std::vector<unsigned char>();
    prefixes.reserve(pow(size, prefix_length));
    auto scratchpad = std::vector<unsigned char>(size);

    nqueens_prefixes(0,
                     prefix_length,
                     0,
                     size,
                     std::span<unsigned char> {scratchpad},
                     prefixes);

    std::vector<std::thread> threads;

    auto dispatch_start = std::chrono::high_resolution_clock::now();
    int total_items = prefixes.size() / prefix_length;
    int work_size = total_items / args.threads;
    int work_leftover = total_items % args.threads;
    std::vector<int> indices;
    int idx = 0;
    indices.emplace_back(0);
    for (unsigned int t = 0; t < args.threads; t++) {
      int new_idx = idx + (t < work_leftover ? work_size + 1 : work_size) * prefix_length;
      indices.emplace_back(new_idx);
      idx = new_idx;
    }
    indices.emplace_back(idx);

    for (unsigned int t = 0; t < args.threads; t++) {

      int start = indices[t], end = indices[t+1];
      threads.emplace_back([&prefixes, size, start, end, prefix_length, &res]() mutable {

        for (unsigned int i = start; i < end; i += prefix_length) {
          std::vector<unsigned char> prefix(prefixes.begin() + i,
                                            prefixes.begin() + i + prefix_length);

          res += nqueens_serial_prefix(size, prefix); 
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::clog << "prefixes: " << prefixes.size() / prefix_length << " result: " << res << std::endl;

    benchmarker.add_result(start, end, "total");
    benchmarker.add_result(start, dispatch_start, "prep");
  }

  benchmarker.write(args.output_location);

  return res;
}
