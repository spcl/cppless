#include <chrono>
#include <future>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

using dispatcher = cppless::dispatcher::aws_lambda_dispatcher;
using task = cppless::task<dispatcher>;

__attribute((weak)) auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  cppless::aws::lambda::client lambda_client;
  auto key = lambda_client.create_derived_key_from_env();
  dispatcher aws {"", lambda_client, key};

  {
    auto instance = aws.create_instance();

    int a = -1;

    const int count = 3;

    std::vector<cppless::shared_future<int>> results(count);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < count; i++) {
      task::sendable t0 = [=](int i) { return a + 3 + i; };
      instance.dispatch(t0, results[i], std::make_tuple(i));
    }

    for (int i = 0; i < count; i++) {
      instance.wait_one();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto diff = end - start;

    std::cout
        << "Time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
        << " ms" << std::endl;
    for (int i = 0; i < count; i++) {
      std::cout << i << ": " << results[i].get_value() << std::endl;
    }
    std::cout << "Completed " << count << " lambda calls" << std::endl;
  }
}
