#include <fstream>
#include <random>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;

auto pi_estimate(long iterations) -> double
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
  program.add_argument("-p")
      .help("number of processes")
      .default_value(1)
      .scan<'i', int>();
  program.add_argument("-t")
      .default_value(std::string(""))
      .help("location to write trace file to");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  long n = program.get<long>("n");
  int np = program.get<int>("-p");
  std::string trace_location = program.get("-t");

  dispatcher aws;
  auto instance = aws.create_instance();
  //cppless::tracing_span_container spans;
  //auto root = spans.create_root("root");

  std::vector<double> results(np);
  for (int i = 0; i < np; i++) {
    auto fn = [=](long iterations) { return pi_estimate(iterations); };
    cppless::dispatch(instance,
                      fn,
                      results[i],
                      {n / np});
                      //root.create_child("lambda_invocation"));
  }
  cppless::wait(instance, np);

  double pi = std::reduce(results.begin(), results.end(), 0.0) / np;
  std::cout << pi << std::endl;
  //if (!trace_location.empty()) {
  //  std::ofstream trace_file(trace_location);
  //  nlohmann::json j = spans;
  //  trace_file << j.dump(2);
  //}
}
