#include <argparse/argparse.hpp>

#include "./dispatcher.hpp"
#include "./serial.hpp"

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("fib_bench");

  program.add_argument("--dispatcher")
      .help("Use dispatcher")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--serial")
      .help("Use serial implementation")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("n")
      .help("display the square of a given integer")
      .scan<'i', int>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  auto n = program.get<int>("n");

  if (program["--serial"] == true) {
    int r = fib(serial_args {.n = n});
    std::cout << r << std::endl;
  } else if (program["--dispatcher"] == true) {
    int r = fib(dispatcher_args {.n = n});
    std::cout << r << std::endl;
  }

  return 0;
}