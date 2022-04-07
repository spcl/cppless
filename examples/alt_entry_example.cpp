#include <iostream>

__attribute((entry)) auto alt_entry(int /*argc*/, char* /*argv*/[]) -> int
{
  std::cout << "Hello alt_entry" << std::endl;
  return 0;
}

__attribute((weak)) auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  std::cout << "Hello main" << std::endl;
  return 0;
}