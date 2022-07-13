#include <random>

auto serial_pi(int iterations) -> double
{
  std::random_device r;

  std::default_random_engine e(r());
  std::uniform_real_distribution<double> dist(0, 1);

  int hit = 0;
  for (int i = 0; i < iterations; i++) {
    double x = dist(e);
    double y = dist(e);
    if (x * x + y * y <= 1) {
      hit++;
    }
  }

  constexpr const int multiplier = 4;
  return multiplier * static_cast<double>(hit) / iterations;
}