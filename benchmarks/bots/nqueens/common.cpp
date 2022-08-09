#include <iostream>
#include <span>
#include <vector>

#include "./common.hpp"

#include <argparse/argparse.hpp>

template<unsigned char N>
inline auto nqueens_serial_template(unsigned long& res,  // NOLINT
                                    unsigned long min_diag,
                                    unsigned long maj_diag,
                                    unsigned long vertical) -> void
{
  constexpr unsigned long mask = (1UL << N) - 1;

  if (vertical == mask) {
    res++;
    return;
  }
  unsigned long bitmap = mask & ~(min_diag | maj_diag | vertical);
  while (bitmap) {
    unsigned long bit = (~bitmap + 1) & bitmap;
    bitmap ^= bit;

    nqueens_serial_template<N>(
        res, (bit | min_diag) >> 1UL, (bit | maj_diag) << 1UL, bit | vertical);
  }
}

template<unsigned char NConst>
inline auto nqueens_serial_dispatch(unsigned char n,
                                    unsigned long min_diag,
                                    unsigned long maj_diag,
                                    unsigned long vertical) -> unsigned long
{
  if (NConst == n) {
    unsigned long res = 0;
    nqueens_serial_template<NConst>(res, min_diag, maj_diag, vertical);
    return res;
  }
  return nqueens_serial_dispatch<NConst - 1>(n, min_diag, maj_diag, vertical);
}

template<>
inline auto nqueens_serial_dispatch<0>(unsigned char /*n*/,
                                       unsigned long /*min_diag*/,
                                       unsigned long /*maj_diag*/,
                                       unsigned long /*vertical*/)
    -> unsigned long
{
  return 0;
}

auto nqueens_serial(unsigned char n,
                    unsigned long min_diag,
                    unsigned long maj_diag,
                    unsigned long vertical) -> unsigned long
{
  return nqueens_serial_dispatch<20>(n, min_diag, maj_diag, vertical);
}

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

auto nqueens_prefixes(unsigned int start,  // NOLINT
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

auto pow(unsigned int base, unsigned int exp) -> unsigned int  // NOLINT
{
  unsigned int res = 1;
  for (unsigned int i = 0; i < exp; i++) {
    res *= base;
  }
  return res;
}

auto nqueens_serial_prefix(unsigned char n, std::span<unsigned char> prefix)
    -> unsigned long
{
  unsigned long min_diag = 0;
  unsigned long maj_diag = 0;
  unsigned long vertical = 0;

  for (unsigned char row : prefix) {
    unsigned long bit = 1UL << row;
    min_diag = (bit | min_diag) >> 1UL;
    maj_diag = (bit | maj_diag) << 1UL;
    vertical = bit | vertical;
  }

  return nqueens_serial(n, min_diag, maj_diag, vertical);
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