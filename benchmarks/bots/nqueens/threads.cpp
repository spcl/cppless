#include <atomic>
#include <span>
#include <thread>
#include <vector>

#include "./threads.hpp"

#include "./common.hpp"

auto nqueens(threads_args args) -> unsigned int
{
  auto size = args.size;
  auto prefix_length = args.prefix_length;

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

  for (unsigned int i = 0; i < prefixes.size(); i += prefix_length) {
    std::vector<unsigned int> prefix(prefixes.begin() + i,
                                     prefixes.begin() + i + prefix_length);
    threads.emplace_back(
        [prefix, size, prefix_length, &res]
        {
          auto scratchpad = std::vector<unsigned char>(size);
          std::copy(prefix.begin(), prefix.end(), scratchpad.begin());
          res += nqueens_serial(prefix_length, scratchpad);
        });
  }

  for (auto& t : threads) {
    t.join();
  }

  return res;
}