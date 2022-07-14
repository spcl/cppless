#include <optional>
#include <string>

#include "./dispatcher.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

using dispatcher = cppless::aws_lambda_beast_dispatcher<>::from_env;

auto dispatcher_fib(int i) -> int
{
  if (i <= 1) {
    return i;
  }
  {
    dispatcher aws;

    int a, b;

    {
      auto instance = aws.create_instance();

      auto t = [=](int i) { return dispatcher_fib(i); };
      cppless::dispatch(instance, t, a, {i - 1});
      cppless::dispatch(instance, t, b, {i - 2});
      cppless::wait(instance, 2);
    }

    return a + b;
  }
}

auto fib(dispatcher_args args) -> int
{
  return dispatcher_fib(args.n);
}