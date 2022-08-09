#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
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



inline auto load_input(const std::string& name)
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

inline auto input_file_number(int x)
{
  std::ostringstream ss;
  ss << std::setw(3) << std::setfill('0') << x;
  return ss.str();
}


