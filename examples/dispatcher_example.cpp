#include <future>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/local.hpp>

using dispatcher =
    cppless::dispatchers::local_dispatcher<cereal::BinaryInputArchive,
                                           cereal::BinaryOutputArchive>;
using task = cppless::task<dispatcher>;

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  std::vector<std::string> args {argv, argv + argc};
  dispatcher local {args[0]};

  {
    auto instance = local.create_instance();

    int a = -1;

    task::sendable t0 = [=]() { return a + 3; };
    task::sendable t1 = [=]() { return 1 - a; };
    auto f0 = instance.dispatch(t0, std::make_tuple());
    auto f1 = instance.dispatch(t1, std::make_tuple());

    int x = instance.wait_one();
    std::cout << "x = " << x << std::endl;
    int y = instance.wait_one();
    std::cout << "y = " << y << std::endl;

    std::cout << f0.get_value() << std::endl;
    std::cout << f1.get_value() << std::endl;
  }
}
