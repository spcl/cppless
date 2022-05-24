#include "./json_serialization.hpp"
#include "./tail_apply.hpp"

auto main() -> int
{
  json_serialization_tests();
  tail_apply_tests();

  return 0;
}