#include <atomic>
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

using dispatcher =
    cppless::dispatcher::aws_lambda_nghttp2_dispatcher<>::from_env;
namespace lambda = cppless::dispatcher::aws;
constexpr unsigned int memory_limit = 2048;
constexpr unsigned int ephemeral_storage = 64;
using cpu_intensive_task =
    dispatcher::task<lambda::with_memory<memory_limit>,
                     lambda::with_ephemeral_storage<ephemeral_storage>>;

auto nqueens(dispatcher_args args) -> unsigned int
{
  auto size = args.size;
  auto prefix_length = args.prefix_length;

  dispatcher aws;
  dispatcher::instance instance = aws.create_instance();

  auto prefixes = std::vector<unsigned char>();
  prefixes.reserve(pow(size, prefix_length));
  auto scratchpad = std::vector<unsigned char>(size);

  nqueens_prefixes(0,
                   prefix_length,
                   0,
                   size,
                   std::span<unsigned char> {scratchpad},
                   prefixes);

  std::vector<cppless::shared_future<unsigned int>> futures(prefixes.size());

  for (unsigned int i = 0; i < prefixes.size(); i += prefix_length) {
    std::vector<unsigned int> prefix(prefixes.begin() + i,
                                     prefixes.begin() + i + prefix_length);
    cpu_intensive_task::sendable task = [prefix, size]
    {
      auto scratchpad = std::vector<unsigned char>(size);
      std::copy(prefix.begin(), prefix.end(), scratchpad.begin());
      return nqueens_serial(prefix.size(), scratchpad);
    };
    instance.dispatch(
        task, futures.emplace_back(), std::make_tuple(), std::nullopt);
  }
  for ([[maybe_unused]] auto& f : futures) {
    instance.wait_one();
  }
  unsigned int res = 0;
  for (auto& f : futures) {
    res += f.value();
  }

  return res;
}
