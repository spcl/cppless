#include "cppless/cppless.hpp"

auto main() -> int
{
  auto result = name();
  return result == "cppless" ? 0 : 1;
}
