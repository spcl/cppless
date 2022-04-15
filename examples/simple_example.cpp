#include <memory>
#include <string>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cppless/dispatcher/local.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/graph.hpp>
#include <cppless/graph/host_controller_executor.hpp>

#include "cppless/dispatcher/common.hpp"

using dispatcher =
    cppless::dispatchers::local_dispatcher<cereal::BinaryInputArchive,
                                           cereal::BinaryOutputArchive>;
using executor = cppless::executor::host_controller_executor<dispatcher>;
const int iterations = 100;
__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  using cppless::execution::schedule, cppless::execution::then;
  std::vector<std::string> args {argv, argv + argc};
  auto local = std::make_shared<dispatcher>(args[0]);

  // 100 tasks executed serially
  cppless::graph::builder<executor> builder {local};

  auto q = schedule(builder);
  cppless::shared_future<int> m = then(q, []() { return 12; })->get_future();

  builder.await_all();

  std::cout << m.get_value() << std::endl;
}

// https://github.com/bsc-pm/bots
// https://github.com/Mantevo/HPCCG
