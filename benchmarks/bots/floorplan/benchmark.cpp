#include <cassert>
#include <iostream>

#include "../../include/benchmark.hpp"

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>

#include "./common.hpp"
#include "./serial.hpp"

const std::vector<unsigned int> inputs = {5, 15, 20};

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("floorplan_bench");
  benchmark::parse_args(program, argc, argv);
  using namespace boost::ut;  // NOLINT
  for (auto input_size : inputs) {
    benchmark::benchmark("serial / " + std::to_string(input_size)) =
        [&](auto body)
    {
      auto fp = floorplan_init("benchmarks/bots/floorplan/inputs/floorplan."
                               + std::to_string(input_size));
      expect(fp.cells.size() == input_size + 1);

      int tasks = 0;
      result_data d {};

      body = [&]
      {
        std::tie(tasks, d) = floorplan({fp});
        benchmark::do_not_optimize(tasks);
        benchmark::do_not_optimize(d);
      };

      expect(d.min_area == fp.solution);
    };
  }
}
