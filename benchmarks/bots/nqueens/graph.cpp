#include <atomic>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/dispatcher/local.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/graph.hpp>
#include <cppless/graph/host_controller_executor.hpp>

#include "./common.hpp"

using dispatcher =
    cppless::dispatcher::local_dispatcher<cereal::JSONInputArchive,
                                          cereal::JSONOutputArchive>;
using executor = cppless::executor::host_controller_executor<dispatcher>;

int main(int argc, char* argv[])
{
  using cppless::execution::schedule, cppless::execution::then;

  argparse::ArgumentParser program("nqueens_threads");
  program.add_argument("prefix-length")
      .help("The length of the prefix from which threads should be started")
      .default_value(1)
      .scan<'i', unsigned int>();
  auto args = parse_args(program, argc, argv);
  auto size = args.size;
  auto prefix_length = program.get<unsigned int>("prefix-length");

  dispatcher local {argv[0]};
  cppless::graph::builder<executor> builder {local};

  auto prefixes = std::vector<unsigned char>();
  prefixes.reserve(pow(size, prefix_length));
  auto scratchpad = std::vector<unsigned char>(size);

  nqueens_prefixes(0,
                   prefix_length,
                   0,
                   size,
                   std::span<unsigned char> {scratchpad},
                   prefixes);

  std::vector<cppless::shared_future<unsigned int>> futures;

  auto start = schedule(builder);
  for (unsigned int i = 0; i < prefixes.size(); i += prefix_length) {
    std::vector<unsigned int> prefix(prefixes.begin() + i,
                                     prefixes.begin() + i + prefix_length);

    auto task = [i, prefix, size, prefix_length]
    {
      auto scratchpad = std::vector<unsigned char>(size);
      std::copy(prefix.begin(), prefix.end(), scratchpad.begin());
      return nqueens_serial(prefix_length, scratchpad);
    };
    futures.push_back(then(start, task)->get_future());
  }
  builder.await_all();
  unsigned int res = 0;
  for (auto& f : futures) {
    res += f.get_value();
  }

  std::cout << res << std::endl;
}