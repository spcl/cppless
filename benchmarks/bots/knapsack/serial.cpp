#include <limits>
#include <span>

#include "./serial.hpp"

#include "./common.hpp"

auto knapsack(serial_args args) -> int
{
  int best_so_far {std::numeric_limits<int>::min()};
  return knapsack_serial(
      best_so_far, std::span<knapsack_item> {args.items}, args.capacity, 0);
}