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

using dispatcher = cppless::dispatcher::aws_lambda_beast_dispatcher<>::from_env;
using task = cppless::task<dispatcher>;

auto do_task(int i) -> int
{
  if (i <= 1) {
    return i;
  }
  {
    dispatcher aws;

    cppless::shared_future<int> a;
    cppless::shared_future<int> b;

    {
      auto instance = aws.create_instance();

      task::sendable t0 = [=](int i) { return do_task(i); };
      instance.dispatch(t0, a, std::make_tuple(i - 1), std::nullopt);
      instance.dispatch(t0, b, std::make_tuple(i - 2), std::nullopt);
      instance.wait_one();
      instance.wait_one();
    }

    return a.value() + b.value();
  }
}

__attribute((weak)) auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  std::cout << do_task(12) << std::endl;
}
