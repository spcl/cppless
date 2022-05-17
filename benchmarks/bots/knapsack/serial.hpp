#pragma once

#include <limits>
#include <span>

#include "./common.hpp"

class serial_args
{
public:
  std::vector<knapsack_item> items;
  int capacity;
};

inline auto knapsack(serial_args args)
{
  int best_so_far {std::numeric_limits<int>::min()};
  return knapsack_serial(
      best_so_far, std::span<knapsack_item> {args.items}, args.capacity, 0);
}