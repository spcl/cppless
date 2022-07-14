#include <memory>
#include <string>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/dispatcher/local.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/graph.hpp>
#include <cppless/graph/host_controller_executor.hpp>
#include <cppless/utils/tracing.hpp>
#include <nlohmann/json.hpp>

using dispatcher = cppless::local_dispatcher<cereal::BinaryInputArchive,
                                             cereal::BinaryOutputArchive>;
using executor = cppless::executor::host_controller_executor<dispatcher>;
const int iterations = 100;
__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  using cppless::execution::schedule, cppless::execution::then;
  std::vector<std::string> args {argv, argv + argc};
  cppless::tracing_span_container spans;
  auto root = spans.create_root("root");
  auto local = std::make_shared<dispatcher>(args[0]);

  // 100 tasks executed serially
  cppless::graph::builder<executor> builder {root, local};

  auto q = schedule(builder);
  auto asd = then(q, []() { return 12; });
  cppless::shared_future<int> m =
      then(asd, [](int m) { return m + 1; })->future();

  builder.await_all();

  std::cout << m.value() << std::endl;

  nlohmann::json j = spans;
  std::cout << j.dump(4) << std::endl;
}

// https://github.com/bsc-pm/bots
// https://github.com/Mantevo/HPCCG
