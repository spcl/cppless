#pragma once

class threads_args
{
public:
  unsigned int size;
  unsigned int prefix_length;
};

auto nqueens(threads_args args) -> unsigned int;