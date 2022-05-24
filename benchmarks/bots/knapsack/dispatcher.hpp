#pragma once
#include <vector>

#include "./common.hpp"
class dispatcher_args
{
public:
  std::vector<knapsack_item> items;
  int capacity;
  int split;
};

auto knapsack(dispatcher_args args) -> int;