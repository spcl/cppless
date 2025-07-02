#include <fstream>
#include <random>
#include <vector>

#include <cppless/dispatcher/aws-lambda.hpp>
#include <cereal/types/vector.hpp>

#include "function.hpp"

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;

auto main(int argc, char* argv[]) -> int
{
  int size = atoi(argv[1]);
  std::cerr << "Input size of " << size << std::endl;

  dispatcher aws;
  auto instance = aws.create_instance();

  double result;
  auto fn = [](int size) { 
      double result = graph_pagerank(size);

      return result;
    };
  auto id = cppless::dispatch(
    instance,
    fn,
    result,
    size
  );
  instance.wait_one();
  
  std::cout << "Received result " << result << std::endl;

  return 0;
}
