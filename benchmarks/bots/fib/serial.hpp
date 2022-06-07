#pragma once

#include <tuple>

class serial_args
{
public:
  int n;
};

auto fib(serial_args args) -> int;