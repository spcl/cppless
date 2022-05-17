#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

#include "../../include/benchmark.hpp"

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>

#include "./common.hpp"
#include "./dispatcher.hpp"
#include "./serial.hpp"

auto load_input(const std::string& name)
    -> std::tuple<std::vector<knapsack_item>, int>
{
  int capacity {0};
  std::vector<knapsack_item> items;
  std::ifstream input(name);
  std::tie(items, capacity) = read_input(input);

  constexpr boost::lambda2::lambda2_arg<1> a {};
  constexpr boost::lambda2::lambda2_arg<2> b {};
  std::sort(items.begin(),
            items.end(),
            a->*&knapsack_item::unit_value > b->*&knapsack_item::unit_value);
  return {items, capacity};
}

auto input_file_number(int x)
{
  std::ostringstream ss;
  ss << std::setw(3) << std::setfill('0') << x;
  return ss.str();
}

const auto inputs = {12, 16, 20, 24, 32, 36, 40, 44, 48, 64, 96, 128};

auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("knapsack_bench");
  benchmark::parse_args(program, argc, argv);

  for (auto input_size : inputs) {
    benchmark::benchmark("serial / " + std::to_string(input_size)) =
        [&](auto body)
    {
      auto input = load_input("benchmarks/bots/knapsack/inputs/knapsack-"
                              + input_file_number(input_size) + ".input");
      body = [&]
      {
        auto res = knapsack(serial_args {
            .items = std::get<0>(input),
            .capacity = std::get<1>(input),
        });
        benchmark::do_not_optimize(res);
      };
    };

    benchmark::benchmark("dispatcher / " + std::to_string(input_size)) =
        [&](auto body)
    {
      auto input = load_input("benchmarks/bots/knapsack/inputs/knapsack-"
                              + input_file_number(input_size) + ".input");
      body = [&]
      {
        auto res = knapsack(dispatcher_args {
            .items = std::get<0>(input),
            .capacity = std::get<1>(input),
            .split = static_cast<int>(std::get<0>(input).size() - 4),
        });
        benchmark::do_not_optimize(res);
      };
    };
  }
}