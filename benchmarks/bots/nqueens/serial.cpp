#include <iostream>
#include <span>
#include <vector>

#include <argparse/argparse.hpp>

#include "./common.hpp"

int main(int argc, char* argv[])
{
  argparse::ArgumentParser program("nqueens_serial");
  auto args = parse_args(program, argc, argv);
  unsigned int size = args.size;

  auto scratchpad = std::vector<unsigned char>(size);

  auto res = nqueens_serial(0, scratchpad);

  std::cout << res << std::endl;
}