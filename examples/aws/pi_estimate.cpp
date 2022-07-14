#include <numeric>
#include <random>
#include <vector>

#include <cppless/dispatcher/aws-lambda.hpp>

double pi_estimate(int n)
{
  std::random_device r;
  std::default_random_engine e(r());
  std::uniform_real_distribution<double> dist;

  int hit = 0;
  for (int i = 0; i < n; i++) {
    double x = dist(e);
    double y = dist(e);
    if (x * x + y * y <= 1)
      hit++;
  }

  return 4 * static_cast<double>(hit) / n;
}

int main(int, char*[])
{
  const int n = 100000000;
  const int np = 128;

  cppless::aws_dispatcher dispatcher;
  auto aws = dispatcher.create_instance();

  std::vector<double> results(np);
  auto fn = [=] { return pi_estimate(n / np); };
  for (auto& result : results)
    cppless::dispatch(aws, fn, result);
  cppless::wait(aws, np);

  auto pi = std::reduce(results.begin(), results.end()) / np;
  std::cout << pi << std::endl;
}
