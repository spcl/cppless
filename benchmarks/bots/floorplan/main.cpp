#include <argparse/argparse.hpp>

#include "./dispatcher.hpp"
#include "./serial.hpp"
#include "./threads.hpp"

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("fib_bench");

  program.add_argument("--dispatcher")
      .help("Use dispatcher")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--dispatcher-cutoff")
      .help("Cutoff value when using the dispatcher implementation")
      .default_value(2)
      .scan<'i', int>();
  program.add_argument("--serial")
      .help("Use serial implementation")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--threads")
      .help("Use multi-threaded implementation")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--threads-cutoff")
      .help("Cutoff value when using the thread implementation")
      .default_value(2)
      .scan<'i', int>();
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

  auto fp = floorplan_init("benchmarks/bots/floorplan/inputs/floorplan."
                           + std::to_string(input_size));

  if (program["--serial"] == true) {
    auto [n_tasks, res] = floorplan(serial_args {.fp = fp});
    std::cout << "n_tasks: " << n_tasks << std::endl;
    std::cout << "min_area: " << res.min_area << std::endl;
  } else if (program["--dispatcher"] == true) {
    auto cutoff = program.get<int>("--dispatcher-cutoff");
    auto [n_tasks, res] =
        floorplan(dispatcher_args {.fp = fp, .cutoff = cutoff});
    std::cout << "n_tasks: " << n_tasks << std::endl;
    std::cout << "min_area: " << res.min_area << std::endl;
  } else if (program["--threads"] == true) {
    auto cutoff = program.get<int>("--threads-cutoff");
    auto [n_tasks, res] = floorplan(threads_args {.fp = fp, .cutoff = cutoff});
    std::cout << "n_tasks: " << n_tasks << std::endl;
    std::cout << "min_area: " << res.min_area << std::endl;
  }

  return 0;
}