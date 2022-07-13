#include <atomic>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/graph.hpp>
#include <cppless/graph/host_controller_executor.hpp>

#include "./common.hpp"

using dispatcher = cppless::dispatcher::aws_lambda_nghttp2_dispatcher<>;
using executor = cppless::executor::host_controller_executor<dispatcher>;
namespace lambda = cppless::dispatcher::aws;
constexpr unsigned int memory_limit = 2048;
constexpr unsigned int ephemeral_storage = 64;
using cpu_intensive_task =
    dispatcher::task<lambda::with_memory<memory_limit>,
                     lambda::with_ephemeral_storage<ephemeral_storage>>;

class graph_args
{
public:
  unsigned int size;
  unsigned int prefix_length;
};

auto nqueens(graph_args args) -> unsigned int
{
  using cppless::execution::schedule, cppless::execution::then;

  auto size = args.size;
  auto prefix_length = args.prefix_length;

  cppless::aws::lambda::client lambda_client;
  auto key = lambda_client.create_derived_key_from_env();
  auto aws = std::make_shared<dispatcher>(lambda_client, key);
  cppless::graph::builder<executor> builder {std::nullopt, aws};

  auto prefixes = std::vector<unsigned char>();
  prefixes.reserve(pow(size, prefix_length));
  auto scratchpad = std::vector<unsigned char>(size);

  nqueens_prefixes(0,
                   prefix_length,
                   0,
                   size,
                   std::span<unsigned char> {scratchpad},
                   prefixes);

  std::vector<cppless::shared_future<unsigned int>> futures;

  auto start = schedule(builder);
  for (unsigned int i = 0; i < prefixes.size(); i += prefix_length) {
    std::vector<unsigned int> prefix(prefixes.begin() + i,
                                     prefixes.begin() + i + prefix_length);

    auto task = [prefix, size, prefix_length]
    {
      auto scratchpad = std::vector<unsigned char>(size);
      std::copy(prefix.begin(), prefix.end(), scratchpad.begin());
      return nqueens_serial(prefix_length, scratchpad);
    };
    futures.push_back(then<cpu_intensive_task>(start, task)->future());
  }
  builder.await_all();
  unsigned int res = 0;
  for (auto& f : futures) {
    res += f.value();
  }

  return res;
}