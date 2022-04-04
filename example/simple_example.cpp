#include <future>
#include <memory>
#include <string>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cppless/dispatcher/local.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/executor.hpp>

using dispatcher =
    cppless::dispatchers::local_dispatcher<cereal::BinaryInputArchive,
                                           cereal::BinaryOutputArchive>;
const int iterations = 100;
__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  using cppless::execution::schedule, cppless::execution::then,
      cppless::executor::shared_sender;
  std::vector<std::string> args {argv, argv + argc};
  auto local = std::make_shared<dispatcher>(args[0]);

  // 100 tasks executed serially
  cppless::executor::executor<dispatcher> exec_s {local};
  auto start_s = schedule(exec_s);

  shared_sender<dispatcher, int> sum = then(start_s, []() { return 1; });
  for (int i = 0; i < iterations; i++) {
    sum = then(sum, [](int prev_sum) { return prev_sum + 1; });
  }
  exec_s.await_all();

  // 100 tasks executed in parallel
  cppless::executor::executor<dispatcher> exec_p {local};
  auto start_p = schedule(exec_p);
  for (int i = 0; i < iterations; i++) {
    then(start_p, []() { return 1; });
  }
  exec_p.await_all();
}
