#include <limits>
#include <span>
#include <tuple>

#include "./common.hpp"

[[nodiscard]] auto knapsack_item::unit_value() const -> double
{
  return static_cast<double>(value) / static_cast<double>(weight);
}
/*
 * return the optimal solution for n items (first is e) and
 * capacity c. Value so far is v.
 */
auto knapsack_serial(int& best_so_far,  // NOLINT
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