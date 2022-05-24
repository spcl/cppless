#pragma once

#include <vector>

#include "./common.hpp"

class serial_args
{
public:
  std::vector<knapsack_item> items;
  int capacity;
};

auto knapsack(serial_args args) -> int;