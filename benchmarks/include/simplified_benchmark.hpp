
#include <chrono>
#include <iostream>
#include <string>

#include <sys/time.h>
#include <x86intrin.h>

#include <argparse/argparse.hpp>


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

auto parse_args(
  argparse::ArgumentParser& program,
  int argc, char** argv,
  int& input_size,
  bool& flush_cache, int& repetitions,
  std::string& input, std::string& scenario,
  std::string& output
)
{
  program.add_argument("repetitions")
      .help("Number of repetitions")
      .scan<'i', int>();
  program.add_argument("input-size")
      .help("Size of input data?")
      .scan<'i', int>();
  program.add_argument("input")
      .help("Which input to use: integers, structures?");
  program.add_argument("--flush-cache")
      .help("Flush cache?")
      .default_value(true)
      .implicit_value(true);
  program.add_argument("scenario")
      .help("What scenario: ser base64?");
  program.add_argument("output")
      .help("What scenario: ser base64?");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  flush_cache = program.get<bool>("--flush-cache");
  repetitions = program.get<int>("repetitions");
  input_size = program.get<int>("input-size");
  input = program.get<std::string>("input");
  scenario = program.get<std::string>("scenario");
  output = program.get<std::string>("output");
}

inline void flush_cachelines(char* ptr, size_t size)
{
  constexpr static int CACHELINE_SIZE = 64;

  char* ptr_end = ptr + size;
  while(ptr <= ptr_end) {
     _mm_clflush(ptr);
    ptr += CACHELINE_SIZE; 
  }

  // verify that we also checked the last byte
  // will happen when the address is not 64-byte aligned
  // or the size is divisible by 64
  // this could be conditional but it's much easier to do
  // additional call
  _mm_clflush(ptr);
}


template<typename F>
std::vector<double> microbenchmark(
  int repetitions, bool flush_cacheline,
  F && f,
  char* ptr1, size_t size1,
  char* ptr2, size_t size2,
  char* ptr3 = nullptr, size_t size3 = 0
)
{
  using duration = std::chrono::high_resolution_clock::duration;
  using time_point = std::chrono::high_resolution_clock::time_point;
  time_point start;
  time_point stop;
  std::vector<double> times;

  for(int i = 0; i < repetitions + 1; ++i) {

    if(flush_cacheline && ptr1)
      flush_cachelines(ptr1, size1);

    if(flush_cacheline && ptr2)
      flush_cachelines(ptr2, size2);

    if(flush_cacheline && ptr3)
      flush_cachelines(ptr3, size3);

    start = std::chrono::high_resolution_clock::now();
    f();
    stop = std::chrono::high_resolution_clock::now();

    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
    times.push_back(ns);
  }

  return times;
}

}  // namespace benchmark
