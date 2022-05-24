#pragma once

#include <span>
#include <vector>

#include <argparse/argparse.hpp>

auto ok(unsigned int n, std::span<const unsigned char> solution) -> bool;

auto nqueens_serial(unsigned int j, std::span<unsigned char> scratchpad)
    -> unsigned int;

auto nqueens_prefixes(unsigned int start,
                      unsigned int end,
                      unsigned int j,
                      unsigned int n,
                      std::span<unsigned char> scratchpad,
                      std::vector<unsigned char>& prefixes) -> void;

auto pow(unsigned int base, unsigned int exp) -> unsigned int;

class common_args
{
public:
  unsigned int size;
};

auto parse_args(argparse::ArgumentParser& program, int argc, char* argv[])
    -> common_args;