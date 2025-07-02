#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>

#include <sys/time.h>
#include <x86intrin.h>

namespace benchmark
{

auto dry_run = false;
const long long min_time = 1000000000UL;  // 1 second

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

double mysecond()
{
/* struct timeval { long        tv_sec;
            long        tv_usec;        };

struct timezone { int   tz_minuteswest;
             int        tz_dsttime;      };     */

        struct timeval tp;
        struct timezone tzp;
        int i;

        i = gettimeofday(&tp,&tzp);
        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}


inline void flush_cachelines(char* ptr, size_t size)
{
  constexpr static int CACHELINE_SIZE = 64;

  int i = 1;
  char* ptr_end = ptr + size;
  while(ptr <= ptr_end) {
++i;
     _mm_clflush(ptr);
    ptr += CACHELINE_SIZE; 
  }

  // verify that we also checked the last byte
  // will happen when the address is not 64-byte aligned
  // or the size is divisible by 64
  // this could be conditional but it's much easier to do
  // additional call
  _mm_clflush(ptr);
 
  std::clog << "Flushes " << i << std::endl; 
}


template<typename F>
void microbenchmark(F && f, char* ptr1, size_t size1, char* ptr2, size_t size2)
{
  using duration = std::chrono::high_resolution_clock::duration;
  using time_point = std::chrono::high_resolution_clock::time_point;
  time_point start;
  time_point stop;
  std::vector<duration> times;

  uint64_t total_ns = 0;
  uint64_t total_runs = 0;
  double total_t = 0.0;
  for(int i = 0; i < 101; ++i) {

    if(ptr1)
      flush_cachelines(ptr1, size1);

    if(ptr2)
      flush_cachelines(ptr2, size2);

    start = std::chrono::high_resolution_clock::now();
    auto s = mysecond();
    f();
    auto e = mysecond();
    stop = std::chrono::high_resolution_clock::now();

    if(i != 0) {
      const auto ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
      times.push_back(ns);
      total_ns += ns.count();
      total_runs += 1;
      total_t += e - s;
    } else {
      const auto ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
      std::cerr << ns.count() << std::endl;
    }
  }

  auto average_ns = static_cast<double>(total_ns)
      / static_cast<double>(total_runs);

  std::clog << average_ns << std::endl;
  std::cerr << total_t << std::endl;
}

template<typename F>
void microbenchmark(F && f)
{
  using duration = std::chrono::high_resolution_clock::duration;
  using time_point = std::chrono::high_resolution_clock::time_point;
  time_point start;
  time_point stop;
  std::vector<duration> times;

  std::vector<double> cache_checkup(100*1000*1000, 0.0);

  uint64_t total_ns = 0;
  uint64_t total_runs = 0;
  double total_t = 0.0;
  for(int i = 0; i < 101; ++i) {

    for(int j = 0; j < cache_checkup.size(); ++j) {
      cache_checkup[j] += 0.001;
    }

    start = std::chrono::high_resolution_clock::now();
    auto s = mysecond();
    f();
    auto e = mysecond();
    stop = std::chrono::high_resolution_clock::now();

    if(i != 0) {
      const auto ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
      times.push_back(ns);
      total_ns += ns.count();
      total_runs += 1;
      total_t += e - s;
    } else {
      const auto ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
      std::cerr << ns.count() << std::endl;
    }
  }

  auto average_ns = static_cast<double>(total_ns)
      / static_cast<double>(total_runs);

  do_not_optimize(cache_checkup.data());
  std::clog << average_ns << std::endl;
  std::cerr << total_t << std::endl;
  std::clog << cache_checkup.size() << " " << cache_checkup[1000] << std::endl;
}

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
      using duration = std::chrono::steady_clock::duration;
      using time_point = std::chrono::steady_clock::time_point;

      time_point initial_start;
      time_point initial_stop;
      auto l = [&](auto run)
      {
        initial_start = std::chrono::steady_clock::now();
        run();
        initial_stop = std::chrono::steady_clock::now();
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

      time_point start;
      time_point stop;
      for(int i = 0; i < 101; ++i) {

        start = std::chrono::steady_clock::now();
        //run();
        stop = std::chrono::steady_clock::now();

        if(i != 0) {
          const auto ns =
              std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
          times.push_back(ns);
          total_ns += ns;
          total_runs += 1;
        }
      }

      //while (total_ns.count() < min_time) {
      //  time_point start;
      //  time_point stop;
      //  //if (batch_size > 1) {
      //  //  auto l = [&](auto run)
      //  //  {
      //  //    start = std::chrono::steady_clock::now();
      //  //    for (unsigned int i = 0; i < batch_size; i++) {
      //  //      run();
      //  //    }
      //  //    stop = std::chrono::steady_clock::now();
      //  //  };

      //  //  test(benchmark_accessor {l});

      //  //} else {
      //  //  auto l = [&](auto run)
      //  //  {
      //  //    start = std::chrono::steady_clock::now();
      //  //    run();
      //  //    stop = std::chrono::steady_clock::now();
      //  //  };
      //  //  test(benchmark_accessor {l});
      //  //};

      //  const auto ns =
      //      std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
      //  times.push_back(ns);
      //  total_ns += ns;
      //  total_runs += batch_size;
      //}

      auto average_ns = static_cast<double>(total_ns.count())
          / static_cast<double>(total_runs);

      std::clog << average_ns << std::endl;

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

}  // namespace benchmark
