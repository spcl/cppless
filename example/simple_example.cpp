#include <iostream>
#include <string>

template<class L>
class entry_wrapper
{
public:
  __attribute((entry)) static int test123(int argc, char* argv[])
  {
    L l;
    std::cout << "Hello from " << l() << std::endl;
    return 0;
  }
};

template<class L>
auto generate(L l) -> void
{
  entry_wrapper<L> _r;
}

auto main(int argc, char* argv[]) -> int
{
  generate([]() { return "A"; });
  generate([]() { return "B"; });

  std::cout << __builtin_unique_stable_name(int) << std::endl;
  std::cout << argv[0] << std::endl;
}
