#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <span>
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

  [[nodiscard]] auto unit_value() const -> double
  {
    return static_cast<double>(value) / static_cast<double>(weight);
  }

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
auto knapsack_serial(int& best_so_far,
                     std::span<knapsack_item> items,
                     int c,
                     int v) -> int
{
  /* base case: full knapsack or no items */
  if (c < 0) {
    return std::numeric_limits<int>::min();
  }

  /* feasible solution, with value v */
  if (items.empty() || c == 0) {
    return v;
  }

  double ub = static_cast<double>(v) + c * items[0].value / items[0].weight;

  if (ub < best_so_far) {
    /* prune ! */
    return std::numeric_limits<int>::min();
  }
  /*
   * compute the best solution without the current item in the knapsack
   */
  auto without = knapsack_serial(best_so_far, items.subspan(1), c, v);

  /* compute the best solution with the current item in the knapsack */
  auto with = knapsack_serial(
      best_so_far, items.subspan(1), c - items[0].weight, v + items[0].value);

  auto best = with > without ? with : without;
  if (best > best_so_far) {
    best_so_far = best;
  }
  return best;
}

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
