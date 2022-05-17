#pragma once

#include <iostream>
#include <span>
#include <vector>

#include <argparse/argparse.hpp>

#include "./common.hpp"

class serial_args
{
public:
  unsigned int size;
};

inline auto nqueens(serial_args args) -> unsigned int
{
  auto scratchpad = std::vector<unsigned char>(args.size);
  auto res = nqueens_serial(0, scratchpad);
  return res;
}