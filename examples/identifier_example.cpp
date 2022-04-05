#include <iostream>

#include <cppless/utils/fixed_string.hpp>

auto main(int argc, char* argv[]) -> int
{
  auto r = [=]() { return 12; };
  auto m = [=]() { return 1; };

  std::cout << __builtin_unique_stable_name(decltype(r)) << std::endl;
  std::cout << __builtin_unique_stable_name(decltype(m)) << std::endl;
}