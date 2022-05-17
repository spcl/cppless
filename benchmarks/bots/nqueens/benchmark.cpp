
#include "../../include/benchmark.hpp"

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>

#include "./common.hpp"
#include "./dispatcher.hpp"
#include "./graph.hpp"
#include "./serial.hpp"
#include "./threads.hpp"

auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("nqueens_bench");
  benchmark::parse_args(program, argc, argv);

  const auto n_max = 20;
  for (unsigned int i = 0; i < n_max; i++) {
    benchmark::benchmark("serial / " + std::to_string(i)) =
        [&, size = i](auto body)
    {
      body = [&]
      {
        auto res = nqueens(serial_args {.size = size});
        benchmark::do_not_optimize(res);
      };
    };

    benchmark::benchmark("dispatcher / " + std::to_string(i)) =
        [&, size = i](auto body)
    {
      body = [&]
      {
        auto res = nqueens(dispatcher_args {.size = size, .prefix_length = 2});
        benchmark::do_not_optimize(res);
      };
    };

    benchmark::benchmark("threads / " + std::to_string(i)) =
        [&, size = i](auto body)
    {
      body = [&]
      {
        auto res = nqueens(threads_args {.size = size, .prefix_length = 2});
        benchmark::do_not_optimize(res);
      };
    };

    benchmark::benchmark("graph / " + std::to_string(i)) =
        [&, size = i](auto body)
    {
      body = [&]
      {
        auto res = nqueens(graph_args {.size = size, .prefix_length = 2});
        benchmark::do_not_optimize(res);
      };
    };
  }
}