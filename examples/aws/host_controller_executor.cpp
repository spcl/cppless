#include <memory>
#include <string>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/graph.hpp>
#include <cppless/graph/host_controller_executor.hpp>
#include <nlohmann/json.hpp>

using dispatcher = cppless::dispatcher::aws_lambda_dispatcher;
using executor = cppless::executor::host_controller_executor<dispatcher>;

__attribute((weak)) auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  using cppless::execution::schedule, cppless::execution::then;

  cppless::aws::lambda::client lambda_client;
  auto key = lambda_client.create_derived_key_from_env();
  auto local = std::make_shared<dispatcher>("", lambda_client, key);

  // 100 tasks executed serially
  cppless::graph::builder<executor> builder {local};

  auto q = schedule(builder);
  auto asd = then(q, []() { return 12; });
  cppless::shared_future<int> m =
      then(asd, [](int m) { return m + 1; })->future();

  builder.await_all();

  std::cout << m.value() << std::endl;
}