#pragma once

class dispatcher_args
{
public:
  unsigned int size;
  unsigned int prefix_length;
};

auto nqueens(dispatcher_args args) -> unsigned int;