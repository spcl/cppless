#include <future>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/local.hpp>

#include "cppless/dispatcher/common.hpp"

using dispatcher =
    cppless::dispatcher::local_dispatcher<cereal::BinaryInputArchive,
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
    cppless::shared_future<int> t0_result;
    cppless::shared_future<int> t1_result;
    instance.dispatch(t0, t0_result, std::make_tuple());
    instance.dispatch(t1, t1_result, std::make_tuple());

    int x = instance.wait_one();
    std::cout << "x = " << x << std::endl;
    int y = instance.wait_one();
    std::cout << "y = " << y << std::endl;

    std::cout << t0_result.get_value() << std::endl;
    std::cout << t1_result.get_value() << std::endl;
  }
}
