#include <iostream>
#include <random>
#include <vector>

#include "function.hpp"

auto main(int argc, char* argv[]) -> int
{
  int size = 10;
  std::cerr << "Input size of " << size << std::endl;

  double result = graph_pagerank(size);
 
  std::cout << "Received result " << result << std::endl;

  return 0;
}
