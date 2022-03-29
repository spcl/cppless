#include <future>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cppless/dispatcher/local.hpp>
#include <cppless/graph/execution.hpp>
#include <cppless/graph/executor.hpp>

using dispatcher =
    cppless::dispatchers::local_dispatcher<cereal::JSONInputArchive,
                                           cereal::JSONOutputArchive>;
using task = dispatcher::task;
template<class T>
using node = cppless::executor::node<dispatcher, T>;

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  std::vector<std::string> args {argv, argv + argc};
  auto local = std::make_shared<dispatcher>(args[0]);
  cppless::executor::executor<dispatcher> exec {local};

  using cppless::execution::schedule, cppless::execution::then;

  auto e = schedule(exec, []() { return 2; });
  auto q = schedule(exec, []() { return 3; });
  auto sum = then(e, q, [](int x, int y) { return x + y; });

  exec.await_all();

  std::cout << "Result: " << sum->get_result() << std::endl;
}
