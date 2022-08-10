#include <argparse/argparse.hpp>

#include "./common.hpp"
#include "./dispatcher.hpp"
#include "./serial.hpp"
#include "threads.hpp"

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("fib_bench");

  program.add_argument("--dispatcher")
      .help("Use dispatcher")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--dispatcher-prefix-length")
      .help("Split value when using the dispatcher implementation")
      .default_value(2)
      .scan<'i', int>();
  program.add_argument("--threads")
      .help("Use threads")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--threads-prefix-length")
      .help("Split value when using the threads implementation")
      .default_value(2)
      .scan<'i', int>();
  program.add_argument("--serial")
      .help("Use serial implementation")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("input_size")
      .help("display the square of a given integer")
      .scan<'i', int>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  auto input_size = program.get<int>("input_size");

  auto [items, capacity] =
      load_input("benchmarks/bots/knapsack/inputs/knapsack-"
                 + input_file_number(input_size) + ".input");

  if (program["--serial"] == true) {
    int res = knapsack(serial_args {.items = items, .capacity = capacity});
    std::cout << res << std::endl;
  } else if (program["--dispatcher"] == true) {
    auto prefix_length = program.get<int>("--dispatcher-prefix-length");
    int res = knapsack(dispatcher_args {
        .items = items,
        .capacity = capacity,
        .split = static_cast<int>(items.size() - prefix_length)});
    std::cout << res << std::endl;
  } else if (program["--threads"] == true) {
    auto prefix_length = program.get<int>("--threads-prefix-length");
    int res = knapsack(
        threads_args {.items = items,
                      .capacity = capacity,
                      .split = static_cast<int>(items.size() - prefix_length)});
    std::cout << res << std::endl;
  }

  return 0;
}
