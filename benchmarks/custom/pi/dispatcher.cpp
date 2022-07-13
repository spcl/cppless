#include <random>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>

using dispatcher =
    cppless::dispatcher::aws_lambda_nghttp2_dispatcher<>::from_env;
namespace lambda = cppless::dispatcher::aws;
using config = lambda::config<lambda::with_memory<42>>;

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

  std::vector<double> results(np);

  for (int i = 0; i < np; i++) {
    auto fn = [=](long iterations) { return is_inside(iterations); };
    cppless::dispatch<config>(instance, fn, results[i], {n / np});
  }
  cppless::wait(instance, np);

  double pi = std::accumulate(results.begin(), results.end(), 0) / n;
  std::cout << pi << std::endl;
}