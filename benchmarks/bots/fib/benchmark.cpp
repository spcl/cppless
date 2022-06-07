
#include "../../include/benchmark.hpp"

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>

#include "./dispatcher.hpp"
#include "./serial.hpp"

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("fib_bench");
  benchmark::parse_args(program, argc, argv);

  const auto n_max = 20;
  for (int i = 0; i < n_max; i++) {
    benchmark::benchmark("serial / " + std::to_string(i)) = [&](auto body)
    {
      body = [&]
      {
        auto res = fib(serial_args {.n = i});
        benchmark::do_not_optimize(res);
      };
    };

    benchmark::benchmark("dispatcher / " + std::to_string(i)) = [&](auto body)
    {
      body = [&]
      {
        auto res = fib(dispatcher_args {.n = i});
        benchmark::do_not_optimize(res);
      };
    };
  }
}