#pragma once

#include <tuple>

class dispatcher_args
{
public:
  int n;
};

auto fib(dispatcher_args args) -> int;