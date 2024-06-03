#include <future>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

using dispatcher = cppless::aws_lambda_beast_dispatcher<>::from_env;

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  std::vector<std::string> args {argv, argv + argc};
  dispatcher aws;

  {
    auto instance = aws.create_instance();

    int a = -1;

    auto t0 = [=]() { return a + 3; };
    auto t1 = [=]() { return 1 - a; };
    int t0_result, t1_result;
    cppless::dispatch(instance, t0, t0_result, {});

    auto x = instance.wait_one();
    std::cout << "x = " << std::get<0>(x) << " " << std::get<1>(x).invocation_id << " is cold? " << std::get<1>(x).is_cold << std::endl;

    cppless::dispatch(instance, t1, t1_result, {});
    auto y = instance.wait_one();
    std::cout << "y = " << std::get<0>(y) << " " << std::get<1>(y).invocation_id << " is cold? " << std::get<1>(y).is_cold << std::endl;

    std::cout << t0_result << std::endl;
    std::cout << t1_result << std::endl;
  }

  return 0;
}
