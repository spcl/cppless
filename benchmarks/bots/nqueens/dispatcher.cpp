#include <atomic>
#include <cstddef>
#include <numeric>
#include <span>
#include <thread>
#include <tuple>
#include <vector>

#include "./dispatcher.hpp"

#include <argparse/argparse.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/utils/tracing.hpp>
#include <nlohmann/json.hpp>

#include "./common.hpp"

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;
namespace lambda = cppless::aws;
constexpr unsigned int memory_limit = 2048;
constexpr unsigned int ephemeral_storage = 64;
using cpu_intensive =
    lambda::config<lambda::with_memory<memory_limit>,
                   lambda::with_ephemeral_storage<ephemeral_storage>>;

auto nqueens(dispatcher_args args) -> unsigned long
{
  auto size = args.size;
  std::size_t prefix_length = args.prefix_length;

  dispatcher aws;
  auto instance = aws.create_instance();

  std::vector<unsigned char> prefixes;
  prefixes.reserve(pow(size, prefix_length));
  std::vector<unsigned char> scratchpad(size);

  nqueens_prefixes(0,
                   prefix_length,
                   0,
                   size,
                   std::span<unsigned char> {scratchpad},
                   prefixes);
  std::size_t num_prefixes = prefixes.size() / prefix_length;
  std::vector<unsigned long> results(num_prefixes);

  for (unsigned int i = 0; i < num_prefixes; i++) {
    std::vector<unsigned char> prefix(
        &prefixes[prefix_length * i],
        &prefixes[prefix_length * i + prefix_length]);

    auto task = [size](std::vector<unsigned char> prefix)
    { return nqueens_serial_prefix(size, prefix); };
    cppless::dispatch<cpu_intensive>(instance, task, results[i], {prefix});
  }
  cppless::wait(instance, num_prefixes);
  unsigned long res = std::accumulate(results.begin(), results.end(), 0);

  return res;
}
