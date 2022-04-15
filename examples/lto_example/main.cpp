#include <iostream>

#include "lib.hpp"

void foo4()
{
  std::cout << "Hello" << std::endl;
}

auto __attribute((entry)) alt_main() -> int
{
  foo2();
  return 0;
}

int main()
{
  std::cout << foo1();
  return 0;
}