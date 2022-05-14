#include <atomic>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

#include <argparse/argparse.hpp>

#include "./common.hpp"

int main(int argc, char* argv[])
{
  argparse::ArgumentParser program("nqueens_threads");
  program.add_argument("prefix-length")
      .help("The length of the prefix from which threads should be started")
      .default_value(1)
      .scan<'i', unsigned int>();
  auto args = parse_args(program, argc, argv);
  auto size = args.size;
  auto prefix_length = program.get<unsigned int>("prefix-length");

  auto prefixes = std::vector<unsigned char>();
  prefixes.reserve(pow(size, prefix_length));
  auto scratchpad = std::vector<unsigned char>(size);

  nqueens_prefixes(0,
                   prefix_length,
                   0,
                   size,
                   std::span<unsigned char> {scratchpad},
                   prefixes);

  std::vector<std::thread> threads;
  std::atomic<unsigned int> res;

  auto num_prefixes = prefixes.size() / prefix_length;
  for (unsigned int i = 0; i < prefixes.size(); i += prefix_length) {
    std::vector<unsigned int> prefix(prefixes.begin() + i,
                                     prefixes.begin() + i + prefix_length);
    threads.emplace_back(
        [i, prefix, size, prefix_length, &res]
        {
          auto scratchpad = std::vector<unsigned char>(size);
          std::copy(prefix.begin(), prefix.end(), scratchpad.begin());
          res += nqueens_serial(prefix_length, scratchpad);
        });
  }

  for (auto& t : threads) {
    t.join();
  }

  std::cout << res << std::endl;
}