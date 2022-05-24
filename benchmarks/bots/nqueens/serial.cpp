#include <span>
#include <vector>

#include "./serial.hpp"

#include "./common.hpp"

auto nqueens(serial_args args) -> unsigned int
{
  auto scratchpad = std::vector<unsigned char>(args.size);
  auto res = nqueens_serial(0, scratchpad);
  return res;
}