#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>

namespace benchmark
{

auto dry_run = false;
const long long min_time = 1000000000UL;  // 1 second

auto parse_args(argparse::ArgumentParser& program, int argc, char** argv)
    -> std::tuple<std::string>
{
  program.add_argument("--filter")
      .help("Specifies the filter to use")
      .default_value(std::string {""});
  program.add_argument("--color")
      .help("Whether to use color")
      .default_value(true)
      .implicit_value(true);
  program.add_argument("--dry-run")
      .help("Whether to run a dry run")
      .default_value(false)
      .implicit_value(true);

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  auto return_value = std::make_tuple(program.get<std::string>("--filter"));

  boost::ut::cfg<boost::ut::override> = {
      .filter = std::get<0>(return_value),
      .colors = program["--color"] == true
          ? boost::ut::colors {.none = "", .pass = "", .fail = ""}
          : boost::ut::colors {},
      .dry_run = program["--dry-run"] == true};
  dry_run = program["--dry-run"] == true;

  return return_value;
}

double stddev(const std::vector<double>& func)
{
  double mean = std::accumulate(func.begin(), func.end(), 0.0) / func.size();
  double sq_sum = std::inner_product(
      func.begin(),
      func.end(),
      func.begin(),
      0.0,
      [](double const& x, double const& y) { return x + y; },
      [mean](double const& x, double const& y)
      { return (x - mean) * (y - mean); });
  return std::sqrt(sq_sum / func.size());
}

template<class L>
class benchmark_accessor
{
public:
  explicit benchmark_accessor(L l)
      : m_l(std::move(l))
  {
  }

  template<class T>
  auto operator=(T&& t) -> benchmark_accessor&
  {
    m_l(t);
    return *this;
  }

private:
  L m_l;
};

struct benchmark : boost::ut::detail::test
{
  explicit benchmark(std::string name)
      : boost::ut::detail::test {"benchmark", name}
  {
  }

  template<class Test>
  auto operator=(Test test)
  {
    static_cast<boost::ut::detail::test&>(*this) = [&test, this]
    {
      if (dry_run) {
        return;
      }
      using duration = std::chrono::high_resolution_clock::duration;
      using time_point = std::chrono::high_resolution_clock::time_point;

      time_point initial_start;
      time_point initial_stop;
      auto l = [&](auto run)
      {
        initial_start = std::chrono::high_resolution_clock::now();
        run();
        initial_stop = std::chrono::high_resolution_clock::now();
      };
      test(benchmark_accessor {l});
      const auto initial_ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(initial_stop
                                                               - initial_start);
      std::vector<duration> times;
      // Scale up to 0.01s
      const auto runs_per_s =
          10000000.0 / static_cast<double>(initial_ns.count());
      unsigned int batch_size {0};
      if (runs_per_s < 1.0) {
        batch_size = 1;
        times.push_back(initial_ns);
      } else {
        batch_size = static_cast<int>(runs_per_s);
      }

      unsigned int total_runs = 1;
      auto total_ns = initial_ns;

      while (total_ns.count() < min_time) {
        time_point start;
        time_point stop;
        if (batch_size > 1) {
          auto l = [&](auto run)
          {
            start = std::chrono::high_resolution_clock::now();
            for (unsigned int i = 0; i < batch_size; i++) {
              run();
            }
            stop = std::chrono::high_resolution_clock::now();
          };

          test(benchmark_accessor {l});

        } else {
          auto l = [&](auto run)
          {
            start = std::chrono::high_resolution_clock::now();
            run();
            stop = std::chrono::high_resolution_clock::now();
          };
          test(benchmark_accessor {l});
        };

        const auto ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
        times.push_back(ns);
        total_ns += ns;
        total_runs += batch_size;
      }

      auto average_ns = static_cast<double>(total_ns.count())
          / static_cast<double>(total_runs);

      std::string unit = "ns";
      double scale = 1.0;
      if (average_ns / scale > 1000) {
        scale *= 1000;
        unit = "us";
      }
      if (average_ns / scale > 1000) {
        scale *= 1000;
        unit = "ms";
      }
      if (average_ns / scale > 1000) {
        scale *= 1000;
        unit = "s";
      }

      std::vector<double> times_scaled;
      std::transform(times.begin(),
                     times.end(),
                     std::back_inserter(times_scaled),
                     [scale](duration const& x)
                     { return static_cast<double>(x.count()) / scale; });

      std::clog << "[" << name << "] " << (average_ns / scale) << unit;
      std::clog << " σ=" << stddev(times_scaled);

      std::clog << " (";
      std::clog << "runs=" << total_runs;
      if (batch_size > 1) {
        std::clog << ", batch_size=" << batch_size;
      }

      std::clog << ")\n";

      for (auto const& x : times) {
        std::clog << x.count() << " ";
      }
      std::clog << "\n";
    };
  }

private:
  std::string m_name;
};

[[nodiscard]] auto operator""_benchmark(const char* name,
                                        decltype(sizeof("")) size)
{
  return ::benchmark::benchmark {{name, size}};
}

#if defined(__GNUC__) or defined(__clang__)
template<class T>
void do_not_optimize(T&& t)
{
  asm volatile("" ::"m"(t) : "memory");
}
#else
#  pragma optimize("", off)
template<class T>
void do_not_optimize(T&& t)
{
  reinterpret_cast<char volatile&>(t) =
      reinterpret_cast<char const volatile&>(t);
}
#  pragma optimize("", on)
#endif
}  // namespace benchmark