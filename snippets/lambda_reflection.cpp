#include <iostream>

auto main(int argc, char* /*argv*/[]) -> int
{
  double m = 12;
  auto lambda = [argc, m](int /*q*/)
  { std::cout << argc << " " << m << std::endl; };

  constexpr int capture_count = decltype(lambda)::capture_count();
  auto first_capture = lambda.capture<0>();
  auto second_capture = lambda.capture<1>();

  std::cout << "first: " << first_capture << std::endl;
  std::cout << "second: " << second_capture << std::endl;

  lambda.capture<0>() = 13;

  lambda(5);

  return 0;
}