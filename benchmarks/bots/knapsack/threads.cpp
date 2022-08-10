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
    auto task = [](std::vector<knapsack_item> items, int c, int v) mutable
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
    res = std::max(*f, res);
  }
  return res;
}