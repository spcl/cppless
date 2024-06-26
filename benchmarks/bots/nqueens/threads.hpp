#pragma once

class threads_args
{
public:
  unsigned int size;
  unsigned int prefix_length;
  int threads;
  int repetitions;
  std::string output_location;
};

auto nqueens(threads_args args) -> unsigned int;
