#include <future>
#include <string>
#include <vector>

#include <cereal/archives/json.hpp>
#include <cppless/dispatcher/local.hpp>
#include <cppless/dispatcher/sendable.hpp>

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  std::vector<std::string> args {argv, argv + argc};
  cppless::dispatchers::local_dispatcher dispatcher {args[0]};
  using ar = cereal::JSONOutputArchive;

  int a = 12;
  double b = 43;

  cppless::sendable_task<ar, int, int> s_task {
      [=](int x) { return static_cast<int>(a + b + x + 3); }};
  std::future<int> f = dispatcher.make_call<int, int>(s_task, 1);

  std::cout << f.get() << std::endl;
}