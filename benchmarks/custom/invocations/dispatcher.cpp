#include <fstream>
#include <random>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>

auto no_op(int dummy) -> int
{
  return 43;
}

template<typename Dispatcher>
void benchmark(Dispatcher && instance, int repetitions, int np, const std::string& output_location)
{
  // repetition, sample id, time, request id, is_cold
  std::vector<std::tuple<int, int, uint64_t, std::string, bool>> time_results;

  for(int rep = 0; rep < repetitions + 1; ++rep) {

    int start_position_vec = time_results.size();
    int first_id = -1;

    std::vector<int> results(np);
    
    auto fn = [=](int dummy) { return no_op(dummy); };
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < np; i++) {
      auto start_func = std::chrono::high_resolution_clock::now();
      auto id = cppless::dispatch(instance,
                        fn,
                        results[i],
                        {42});
      if(first_id == -1)
        first_id = id;
      auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(start_func).time_since_epoch().count();
      time_results.emplace_back(rep, id, ts, "", false);
    }

    for(int j = 0; j < np; ++j) {
      auto res = instance.wait_one();
      auto end = std::chrono::high_resolution_clock::now(); 
      auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch().count();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

      int pos = std::get<0>(res);
      int pos_shift = pos - first_id;
      int pos_in_vector = start_position_vec + pos_shift;

      std::get<2>(time_results[pos_in_vector]) = ts - std::get<2>(time_results[pos_in_vector]);
      std::get<3>(time_results[pos_in_vector]) = std::get<1>(res).invocation_id;
      std::get<4>(time_results[pos_in_vector]) = std::get<1>(res).is_cold;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
    time_results.emplace_back(rep, -1, duration, "total", false);

    std::cout << "Total time " << rep << " " << duration / 1000.0 << std::endl;

  }

  {
    std::ofstream output_file{output_location, std::ios::out};
    output_file << "repetition,sample,time,request_id,is_cold" << std::endl;
    for(int i = 0; i < time_results.size(); ++i) {
      auto& res = time_results[i];
      output_file << std::get<0>(res) << "," << std::get<1>(res) << "," << std::get<2>(res) << "," << std::get<3>(res) << "," << std::get<4>(res) << std::endl;
    }
  }

}

using dispatcher_nghttp2 = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;
using dispatcher_beast  = cppless::aws_lambda_beast_dispatcher<>::from_env;

auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("pi_bench_dispatcher");

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
  program.add_argument("-d")
      .default_value(std::string(""))
      .help("location to write output statistics");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  int np = program.get<int>("-p");
  int repetitions = program.get<int>("-r");
  std::string output_location = program.get("-o");
  std::string dispatcher = program.get("-d");

  if(dispatcher == "nghttp2") {
    dispatcher_nghttp2 aws;
    benchmark(aws.create_instance(), repetitions, np, output_location);
  } else if(dispatcher == "beast") {
    dispatcher_beast aws;
    benchmark(aws.create_instance(), repetitions, np, output_location);
  } else {
    exit(1);
  }

  return 0;
}
