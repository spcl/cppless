#pragma once
#include <vector>

#include "./common.hpp"
class threads_args
{
public:
  std::vector<knapsack_item> items;
  int capacity;
  int split;
};

auto knapsack(threads_args args) -> int;