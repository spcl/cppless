#include <argparse/argparse.hpp>

#include "./common.hpp"
#include "./dispatcher.hpp"
//#include "./graph.hpp"
#include "./serial.hpp"
#include "./threads.hpp"

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("fib_bench");

  program.add_argument("--dispatcher")
      .help("Use dispatcher")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--dispatcher-prefix-length")
      .help("Prefix length value when using the dispatcher implementation")
      .default_value(2)
      .scan<'i', unsigned int>();
  program.add_argument("--graph")
      .help("Use graph")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--graph-prefix-length")
      .help("Prefix length value when using the dispatcher implementation")
      .default_value(2)
      .scan<'i', unsigned int>();
  program.add_argument("--serial")
      .help("Use dispatcher")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--threads")
      .help("Use dispatcher")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--threads-number")
      .help("Number of threads")
      .default_value(1)
      .scan<'i', int>();
  program.add_argument("--threads-prefix-length")
      .help("Prefix length value when using the dispatcher implementation")
      .default_value(2)
      .scan<'i', unsigned int>();
  program.add_argument("input_size")
      .help("display the square of a given integer")
      .scan<'i', unsigned int>();
  program.add_argument("-r")
      .help("number of repetitions")
      .default_value(1)
      .scan<'i', int>();
  program.add_argument("-o")
      .default_value(std::string(""))
      .help("location to write output statistics");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  auto size = program.get<unsigned int>("input_size");
  int repetitions = program.get<int>("-r");
  std::string output_location = program.get<std::string>("-o");
  auto threads = program.get<int>("--threads-number");

  if (program["--serial"] == true) {
    unsigned int res = nqueens(serial_args {.size = size});
    std::cout << res << std::endl;
  } else if (program["--dispatcher"] == true) {
    auto prefix_length =
        program.get<unsigned int>("--dispatcher-prefix-length");
    unsigned int res =
        nqueens(dispatcher_args {
          .size = size,
          .prefix_length = prefix_length,
          .threads = threads,
          .repetitions = repetitions,
          .output_location = output_location
        });
    std::cout << res << std::endl;
  } else if (program["--graph"] == true) {
    //auto prefix_length = program.get<unsigned int>("--graph-prefix-length");
    //unsigned int res =
    //    nqueens(graph_args {.size = size, .prefix_length = prefix_length});
    //std::cout << res << std::endl;
  } else if (program["--threads"] == true) {
    auto prefix_length = program.get<unsigned int>("--threads-prefix-length");
    unsigned int res = nqueens(threads_args {
        .size = size,
        .prefix_length = prefix_length,
        .threads = threads,
        .repetitions = repetitions,
        .output_location = output_location
    });
    std::cout << res << std::endl;
  }

  return 0;
}
