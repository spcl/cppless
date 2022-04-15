#include "lib.hpp"

static signed int i = 0;
static const signed int truth = 42;

void foo2()
{
  i = -1;
}

static auto foo3() -> int
{
  foo4();
  return truth;
}

auto foo1() -> int
{
  int data = 1;

  if (i < 0) {
    data = foo3();
  }

  data = data + truth;
  return data;
}