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

#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <tuple>
#include <vector>

#include "./threads.hpp"

#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

#include "./common.hpp"
#include "threads.hpp"

inline auto knapsack_threads(unsigned int split,
                             std::vector<std::thread>& threads,
                             std::span<knapsack_item> items,
                             std::vector<std::unique_ptr<int>>& futures,
                             int c,
                             int v) -> void
{
  if (items.size() == split) {
    auto child_items = items.subspan(1);
    std::vector<knapsack_item> items_vector(child_items.begin(),
                                            child_items.end());
    auto task = [](std::vector<knapsack_item> items, int c, int v)
    {
      int best_so_far = std::numeric_limits<int>::min();
      return knapsack_serial(best_so_far, items, c, v);
    };

    auto& without_future = *futures.emplace_back(std::make_unique<int>());
    threads.emplace_back([=, &without_future]()
                         { without_future = task(items_vector, c, v); });

    auto& with_future = *futures.emplace_back(std::make_unique<int>());
    threads.emplace_back(
        [=, &with_future]() {
          with_future =
              task(items_vector, c - items[0].weight, v + items[0].value);
        });
  } else {
    knapsack_threads(split, threads, items.subspan(1), futures, c, v);

    knapsack_threads(split,
                     threads,
                     items.subspan(1),
                     futures,
                     c - items[0].weight,
                     v + items[0].value);
  }
}

auto knapsack(threads_args args) -> int
{
  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<int>> futures;

  knapsack_threads(args.split,
                   threads,
                   std::span<knapsack_item> {args.items},
                   futures,
                   args.capacity,
                   0);
  std::cout << "number_of_tasks: " << futures.size() << std::endl;
  for (auto& thread : threads) {
    thread.join();
  }

  int res = std::numeric_limits<int>::min();
  for (auto& f : futures) {
    std::cout << "partial: " << *f << std::endl;
    res = std::max(*f, res);
  }
  return res;
}