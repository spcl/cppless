#include <optional>
#include <string>

#include "./dispatcher.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

using dispatcher = cppless::dispatcher::aws_lambda_beast_dispatcher<>::from_env;
using task = cppless::task<dispatcher>;

auto dispatcher_fib(int i) -> int
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

      task::sendable t0 = [=](int i) { return dispatcher_fib(i); };
      instance.dispatch(t0, a, {i - 1});
      instance.dispatch(t0, b, {i - 2});
      instance.wait_one();
      instance.wait_one();
    }

    return a.value() + b.value();
  }
}

auto fib(dispatcher_args args) -> int
{
  return dispatcher_fib(args.n);
}