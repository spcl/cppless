#include "./serial.hpp"

auto fib_serial(int i) -> int
{
  if (i <= 1) {
    return i;
  }
  return fib_serial(i - 1) + fib_serial(i - 2);
}

auto fib(serial_args args) -> int
{
  return fib_serial(args.n);
}