#pragma once

class graph_args
{
public:
  unsigned int size;
  unsigned int prefix_length;
};

auto nqueens(graph_args args) -> unsigned int;