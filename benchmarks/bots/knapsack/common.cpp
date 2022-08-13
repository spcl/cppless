/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify                      */
/*  it under the terms of the GNU General Public License as published by                      */
/*  the Free Software Foundation; either version 2 of the License, or                         */
/*  (at your option) any later version.                                                       */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful,                           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                             */
/*  GNU General Public License for more details.                                              */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License                         */
/*  along with this program; if not, write to the Free Software                               */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA            */
/**********************************************************************************************/

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