#include <atomic>
#include <cstddef>
#include <numeric>
#include <span>
#include <thread>

#include "./dispatcher.hpp"

#include "../../include/measurement.hpp"

#include <argparse/argparse.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/utils/tracing.hpp>
#include <nlohmann/json.hpp>

#include "./common.hpp"

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;
namespace lambda = cppless::aws;
constexpr unsigned int timeout = 30;
constexpr unsigned int memory_limit = 2048;
constexpr unsigned int ephemeral_storage = 64;
using cpu_intensive =
    lambda::config<lambda::with_timeout<timeout>,
                   lambda::with_memory<memory_limit>,
                   lambda::with_ephemeral_storage<ephemeral_storage>>;

auto nqueens(dispatcher_args args) -> unsigned long
{
  auto size = args.size;
  std::size_t prefix_length = args.prefix_length;

  dispatcher aws;
  auto instance = aws.create_instance();
  unsigned long res;

  serverless_measurements benchmarker;
  for(int rep = 0; rep < args.repetitions; ++rep) {

    benchmarker.start_repetition(rep);

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<unsigned char> prefixes;
    prefixes.reserve(pow(size, prefix_length));
    std::vector<unsigned char> scratchpad(size);

    nqueens_prefixes(0,
                     prefix_length,
                     0,
                     size,
                     std::span<unsigned char> {scratchpad},
                     prefixes);

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

    std::size_t num_prefixes = prefixes.size() / prefix_length;
    std::vector<unsigned long> results(args.threads);

    auto dispatch_start = std::chrono::high_resolution_clock::now();
    for (unsigned int t = 0; t < args.threads; t++) {

      int start = indices[t], end = indices[t+1];
      std::vector<unsigned char> prefix(
          &prefixes[start],
          &prefixes[end]);

      auto task = [prefix_length, size](std::vector<unsigned char> prefix)
      {
        unsigned long res = 0;
        for (unsigned int i = 0; i < prefix.size(); i += prefix_length) {
          std::vector<unsigned char> subprefix(prefix.begin() + i,
                                            prefix.begin() + i + prefix_length);
          res += nqueens_serial_prefix(size, subprefix); 
        }
        return res;
      };

      auto start_func = std::chrono::high_resolution_clock::now();
      auto id = cppless::dispatch<cpu_intensive>(
        instance, task, results[t], {prefix}
      );

      benchmarker.add_function_start(id, start_func);
    }
    auto dispatch_end = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < args.threads; i++) {
      auto f = instance.wait_one();
      benchmarker.add_function_result(f);
    }

    res = std::accumulate(results.begin(), results.end(), 0);
    auto end = std::chrono::high_resolution_clock::now();

    std::clog << "prefixes: " << prefixes.size() / prefix_length << " result: " << res << std::endl;

    benchmarker.add_result(start, end, "total");
    benchmarker.add_result(dispatch_start, dispatch_end, "dispatch");
    benchmarker.add_result(dispatch_end, end, "wait");
    benchmarker.add_result(start, dispatch_start, "prep");

  }

  benchmarker.write(args.output_location);

  return res;
}
