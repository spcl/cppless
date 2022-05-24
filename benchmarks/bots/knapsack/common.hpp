#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <argparse/argparse.hpp>
#include <boost/lambda2.hpp>
#include <cereal/cereal.hpp>

class knapsack_item
{
public:
  int weight;
  int value;

  [[nodiscard]] auto unit_value() const -> double;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::make_nvp("weight", weight), cereal::make_nvp("value", value));
  }
};

/*
 * return the optimal solution for n items (first is e) and
 * capacity c. Value so far is v.
 */
auto knapsack_serial(int& best_so_far,  // NOLINT
                     std::span<knapsack_item> items,
                     int c,
                     int v) -> int;

template<class T>
auto read_input(T& stream) -> std::tuple<std::vector<knapsack_item>, int>
{
  unsigned int num_items {0};
  int capacity {0};
  stream >> num_items >> capacity;
  std::vector<knapsack_item> items(num_items);
  for (unsigned int i = 0; i < num_items; i++) {
    stream >> items[i].value >> items[i].weight;
  }

  return {items, capacity};
}
