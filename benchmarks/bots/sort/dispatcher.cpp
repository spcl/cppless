#include <algorithm>
#include <span>
#include <thread>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

using dispatcher = cppless::aws_lambda_beast_dispatcher<>::from_env;
using task = cppless::task<dispatcher>;

constexpr auto cutoff = 1024;

static auto seq_merge(const std::vector<int>& a,
                      const std::vector<int>& b,
                      std::vector<int>& dest)
{
  auto low1 = a.begin();
  auto high1 = a.end();
  auto low2 = b.begin();
  auto high2 = b.end();
  auto lowdest = dest.begin();

  int a1 = 0;
  int a2 = 0;

  if (low1 < high1 && low2 < high2) {
    a1 = *low1;
    a2 = *low2;
    for (;;) {
      if (a1 < a2) {
        *lowdest++ = a1;
        a1 = *++low1;
        if (low1 >= high1) {
          break;
        }
      } else {
        *lowdest++ = a2;
        a2 = *++low2;
        if (low2 >= high2) {
          break;
        }
      }
    }
  }
  if (low1 <= high1 && low2 <= high2) {
    a1 = *low1;
    a2 = *low2;
    for (;;) {
      if (a1 < a2) {
        *lowdest++ = a1;
        ++low1;
        if (low1 > high1) {
          break;
        }
        a1 = *low1;
      } else {
        *lowdest++ = a2;
        ++low2;
        if (low2 > high2) {
          break;
        }
        a2 = *low2;
      }
    }
  }
  if (low1 > high1) {
    dest.insert(dest.end(), low2, high2);
  } else {
    dest.insert(dest.end(), low1, high1);
  }
}

static auto cilk_sort(std::vector<int>& v) -> void
{
  unsigned long quarter = v.size() / 4;

  if (v.size() < cutoff) {
    /* quicksort when less than 1024 elements */
    std::sort(v.begin(), v.end());
    return;
  }
  std::vector<int> a = {&v[0], &v[quarter]};
  std::vector<int> b = {&v[quarter], &v[quarter * 2]};
  std::vector<int> c = {&v[quarter * 2], &v[quarter * 3]};
  std::vector<int> d = {&v[quarter * 3], &v[v.size()]};

  cppless::shared_future<std::vector<int>> a_future;
  cppless::shared_future<std::vector<int>> b_future;
  cppless::shared_future<std::vector<int>> c_future;

  task::sendable t = [](std::vector<int> v)
  {
    cilk_sort(v);
    return v;
  };

  dispatcher aws;
  auto instance = aws.create_instance();
  instance.dispatch(t, a_future, {a});
  instance.dispatch(t, b_future, {b});
  instance.dispatch(t, c_future, {c});
  // WIP: Wait only for the tasks we need?
  // WIP: Handle network overhead in separate thread automatically?
  auto thread = std::thread(
      [&]()
      {
        for (int i = 0; i < 3; ++i) {
          instance.wait_one();
        }
      });
  cilk_sort(d);
  thread.join();

  std::vector<int> left_half(quarter * 2);
  seq_merge(a_future.value(), b_future.value(), left_half);
  std::vector<int> right_half(v.size() - quarter * 2);
  seq_merge(c_future.value(), d, right_half);
  seq_merge(left_half, right_half, v);
}