#pragma once

class dispatcher_args
{
public:
  unsigned int size;
  unsigned int prefix_length;
  int threads;
  int repetitions;
  std::string output_location;
};

auto nqueens(dispatcher_args args) -> unsigned long;
