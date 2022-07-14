#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cppless/dispatcher/local.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/executor.hpp>

template<class T>
auto operator<<(std::ostream& os, const std::vector<T>& v) -> std::ostream&
{
  os << "[";
  auto ii = v.begin();
  if (ii != v.end()) {
    os << *ii;
    ++ii;
  }
  for (; ii != v.end(); ++ii) {
    os << ", " << *ii;
  }
  os << "]";
  return os;
}

template<class T>
void merge(std::vector<T>& res,
           const std::vector<T>& left,
           const std::vector<T>& right)
{
  size_t left_index = 0;
  size_t right_index = 0;
  while (left_index < left.size() && right_index < right.size()) {
    if (left[left_index] < right[right_index]) {
      res.push_back(left[left_index]);
      left_index++;
    } else {
      res.push_back(right[right_index]);
      right_index++;
    }
  }
  while (left_index < left.size()) {
    res.push_back(left[left_index]);
    left_index++;
  }
  while (right_index < right.size()) {
    res.push_back(right[right_index]);
    right_index++;
  }
}

/*
 *        / O \
 *  data <     > return_type
 *        \ O /
 */
template<class Dispatcher, class T>
auto merge_sort(
    size_t total_size,
    const cppless::executor::shared_sender<Dispatcher, std::vector<T>>& data)
    -> cppless::executor::shared_sender<Dispatcher, std::vector<T>>
{
  if (total_size <= 1) {
    return data;
  }

  size_t mid = vec.size() / 2;

  std::vector<T> left(vec.begin(), vec.begin() + static_cast<int64_t>(mid));
  std::vector<T> right(vec.begin() + static_cast<int64_t>(mid), vec.end());

  std::vector<T> sorted_left =
      then([](std::vector<T> part) { return merge_sort(part); });
  std::vector<T> sorted_right =
      then([](std::vector<T> part) { return merge_sort(part); });

  std::vector<T> res;
  res.reserve(vec.size());
  merge(res, sorted_left, sorted_right);

  return res;
}

using dispatcher = cpplesss::local_dispatcher<cereal::BinaryInputArchive,
                                              cereal::BinaryOutputArchive>;

using namespace std;
int main(int argc, char* argv[])
{
  std::vector<int> t = {10, 4, 5, 54, 3, 232, 3, 6, 23, 325, 23};
  std::vector<int> sorted = merge_sort(t);

  std::cout << sorted << std::endl;
}