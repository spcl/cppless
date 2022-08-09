#include <span>
#include <vector>

#include "./serial.hpp"

#include "./common.hpp"

auto nqueens(serial_args args) -> unsigned int
{
  auto res = nqueens_serial(args.size, 0, 0, 0);
  return res;
}