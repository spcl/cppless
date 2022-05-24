#include <iostream>
#include <span>
#include <vector>

#include "./common.hpp"

#include <argparse/argparse.hpp>

auto ok(unsigned int n, std::span<const unsigned char> solution) -> bool
{
  for (unsigned int i = 0; i < n; i++) {
    const unsigned char p = solution[i];

    for (unsigned int j = i + 1; j < n; j++) {
      const unsigned char q = solution[j];
      if (q == p || q == p - (j - i) || q == p + (j - i)) {
        return false;
      }
    }
  }
  return true;
}

auto nqueens_serial(unsigned int j, std::span<unsigned char> scratchpad)
    -> unsigned int
{
  unsigned int n = scratchpad.size();
  if (n == j) {
    return 1;
  }
  unsigned int res = 0;
  for (unsigned int i = 0; i < n; i++) {
    scratchpad[j] = static_cast<char>(i);
    if (ok(j + 1, scratchpad)) {
      res += nqueens_serial(j + 1, scratchpad);
    }
  }
  return res;
}

auto nqueens_prefixes(unsigned int start,
                      unsigned int end,
                      unsigned int j,
                      unsigned int n,
                      std::span<unsigned char> scratchpad,
                      std::vector<unsigned char>& prefixes) -> void
{
  if (j == end) {
    prefixes.insert(
        prefixes.end(), scratchpad.begin() + start, scratchpad.begin() + end);
    return;
  }
  for (unsigned int i = 0; i < n; i++) {
    scratchpad[j] = static_cast<char>(i);
    if (ok(j + 1, scratchpad)) {
      nqueens_prefixes(start, end, j + 1, n, scratchpad, prefixes);
    }
  }
}

auto pow(unsigned int base, unsigned int exp) -> unsigned int
{
  unsigned int res = 1;
  for (unsigned int i = 0; i < exp; i++) {
    res *= base;
  }
  return res;
}

auto parse_args(argparse::ArgumentParser& program, int argc, char* argv[])
    -> common_args
{
  program.add_argument("size")
      .help("The size osf the chess board")
      .scan<'i', unsigned int>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  return common_args {.size = program.get<unsigned int>("size")};
}