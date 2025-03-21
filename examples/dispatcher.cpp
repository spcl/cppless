#include <future>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cppless/dispatcher/local.hpp>

#include "cppless/dispatcher/common.hpp"

using dispatcher = cppless::local_dispatcher<cereal::BinaryInputArchive,
                                             cereal::BinaryOutputArchive>;

__attribute((weak)) auto main(int argc, char* argv[]) -> int
{
  std::vector<std::string> args {argv, argv + argc};
  dispatcher local {args[0]};

  {
    auto instance = local.create_instance();

    int a = -1;

    auto t0 = [=]() { return a + 3; };
    auto t1 = [=]() { return 1 - a; };
    int t0_result, t1_result;
    cppless::dispatch(instance, t0, t0_result, {});
    cppless::dispatch(instance, t1, t1_result, {});

    auto x = instance.wait_one();
    std::cout << "x = " << std::get<0>(x) << " " << std::get<1>(x) << std::endl;
    auto y = instance.wait_one();
    std::cout << "y = " << std::get<0>(y) << " " << std::get<1>(y) << std::endl;

    std::cout << t0_result << std::endl;
    std::cout << t1_result << std::endl;
  }
}
