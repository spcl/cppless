#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/utils/tracing.hpp>
#include <nlohmann/json.hpp>

using dispatcher = cppless::dispatcher::aws_lambda_beast_dispatcher<>;
using task = cppless::task<dispatcher>;

auto do_task(int i) -> int
{
  if (i == 0) {
    return 4;
  }
  {
    cppless::aws::lambda::client lambda_client;
    auto key = lambda_client.create_derived_key_from_env();
    dispatcher aws {lambda_client, key};

    cppless::shared_future<int> result;

    {
      auto instance = aws.create_instance();

      task::sendable t0 = [=]() { return do_task(i - 1); };
      instance.dispatch(t0, result, {}, std::nullopt);
      instance.wait_one();
    }

    return result.get_value() + 1;
  }
}

__attribute((weak)) auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  std::cout << do_task(12) << std::endl;
}
