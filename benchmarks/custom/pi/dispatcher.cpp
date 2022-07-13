#include <random>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>

using dispatcher =
    cppless::dispatcher::aws_lambda_nghttp2_dispatcher<>::from_env;
using task = cppless::task<dispatcher>;

auto is_inside(long iterations) -> double
{
  std::random_device r;

  std::default_random_engine e(r());
  std::uniform_real_distribution<double> dist(0, 1);

  int hit = 0;
  for (long i = 0; i < iterations; i++) {
    double x = dist(e);
    double y = dist(e);
    if (x * x + y * y <= 1)
      hit++;
  }

  constexpr const int multiplier = 4;
  return multiplier * static_cast<double>(hit) / iterations;
}

auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("pi_bench_dispatcher");

  program.add_argument("n").help("number of iterations").scan<'i', long>();

  program.add_argument("np").help("number of processes").scan<'i', int>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  long n = program.get<long>("n");
  int np = program.get<int>("np");

  dispatcher aws;
  auto instance = aws.create_instance();

  std::vector<cppless::shared_future<double>> results(np);

  task::sendable t0 = [=](long iterations) { return is_inside(iterations); };
  for (int i = 0; i < np; i++) {
    instance.dispatch(t0, results[i], {n / np});
  }

  double pi = 0;
  for (int i = 0; i < np; i++) {
    int m = instance.wait_one();
    pi += results[m].value() / np;
  }
  std::cout << pi << std::endl;
}