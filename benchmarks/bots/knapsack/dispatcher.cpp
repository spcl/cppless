#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <vector>
#include <tuple>

#include "./dispatcher.hpp"

#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

#include "./common.hpp"

template<class Dispatcher>
inline auto knapsack_dispatcher(unsigned int split,
                                typename Dispatcher::instance& instance,
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
    cppless::dispatch(instance, task, without_future, {items_vector, c, v});

    auto& with_future = *futures.emplace_back(std::make_unique<int>());
    cppless::dispatch(instance,
                      task,
                      with_future,
                      {items_vector, c - items[0].weight, v + items[0].value});
  } else {
    knapsack_dispatcher<Dispatcher>(
        split, instance, items.subspan(1), futures, c, v);

    knapsack_dispatcher<Dispatcher>(split,
                                    instance,
                                    items.subspan(1),
                                    futures,
                                    c - items[0].weight,
                                    v + items[0].value);
  }
}

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;

auto knapsack(dispatcher_args args) -> int
{
  dispatcher aws;
  dispatcher::instance instance = aws.create_instance();

  std::vector<std::unique_ptr<int>> futures;

  knapsack_dispatcher<dispatcher>(args.split,
                                  instance,
                                  std::span<knapsack_item> {args.items},
                                  futures,
                                  args.capacity,
                                  0);
  std::cout << "number_of_tasks: " << futures.size() << std::endl;
  for ([[maybe_unused]] auto& f : futures) {
    instance.wait_one();
  }

  int res = std::numeric_limits<int>::min();
  for (auto& f : futures) {
    res = std::max(*f, res);
  }
  return res;
}