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

  std::cerr << iterations << std::endl;
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
  program.add_argument("-r")
      .help("number of repetitions")
      .default_value(1)
      .scan<'i', int>();
  program.add_argument("-o")
      .default_value(std::string(""))
      .help("location to write output statistics");
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
  int repetitions = program.get<int>("-r");
  std::string trace_location = program.get("-t");
  std::string output_location = program.get("-o");

  std::cout << "Problem size " << n << " processors " << np << std::endl;

  dispatcher aws;
  auto instance = aws.create_instance();
  //cppless::tracing_span_container spans;
  //auto root = spans.create_root("root");

  // repetition, sample id, time, request id, is_cold
  std::vector<std::tuple<int, int, uint64_t, std::string, bool>> time_results;

  for(int rep = 0; rep < repetitions; ++rep) {

    int start_position_vec = time_results.size();
    int first_id = -1;

    std::vector<double> results(np);

    // This will also work
    // auto fn = [=]() { return pi_estimate(n / np); };
    // auto id = cppless::dispatch(instance, fn, results[i]);

    auto startc = clock();
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < np; i++) {
      auto fn = [=](long iterations) { return pi_estimate(iterations); };
      auto start_func = std::chrono::high_resolution_clock::now();
      auto id = cppless::dispatch(instance,
                        fn,
                        results[i],
                        {n / np});
                        //root.create_child("lambda_invocation")); 
      if(first_id == -1)
        first_id = id;
      auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(start_func).time_since_epoch().count();
      time_results.emplace_back(rep, id, ts, "", false);
      //std::cerr << rep << " " << i << " " << id << " " << first_id << " " << start_position_vec << " " << time_results.size() << std::endl;
    }

    for(int j = 0; j < np; ++j) {
      auto res = instance.wait_one();
      auto end = std::chrono::high_resolution_clock::now(); 
      auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch().count();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

      int pos = std::get<0>(res);
      int pos_shift = pos - first_id;
      int pos_in_vector = start_position_vec + pos_shift;
 
      //std::cerr << rep << " " << j << " " << pos << " " << pos_in_vector  << " " << duration << " " << (ts - std::get<2>(time_results[pos_in_vector])) << std::endl;

      std::get<2>(time_results[pos_in_vector]) = ts - std::get<2>(time_results[pos_in_vector]);
      std::get<3>(time_results[pos_in_vector]) = std::get<1>(res).invocation_id;
      std::get<4>(time_results[pos_in_vector]) = std::get<1>(res).is_cold;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
    time_results.emplace_back(rep, -1, duration, "total", false);

    auto endc = clock();
    std::cout << "Total time " << duration / 1000.0 << std::endl;

    double pi = std::reduce(results.begin(), results.end(), 0.0) / np;
    std::cout << "Rep, PI " << rep << " " << pi << std::endl;

  }
  //if (!trace_location.empty()) {
  //  std::ofstream trace_file(trace_location);
  //  nlohmann::json j = spans;
  //  trace_file << j.dump(2);
  //}

  {
    std::ofstream output_file{output_location, std::ios::out};
    output_file << "repetition,sample,time,request_id,is_cold" << std::endl;
    for(auto & res : time_results)
      output_file << std::get<0>(res) << "," << std::get<1>(res) << "," << std::get<2>(res) << "," << std::get<3>(res) << "," << std::get<4>(res) << std::endl;
  }

  return 0;
}
