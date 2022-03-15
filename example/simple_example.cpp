#include <iostream>
#include <string>

template<class L>
class entry_wrapper
{
public:
  __attribute((entry)) static int test123(int, char*[])
  {
    L l;
    std::cout << "Hello from " << l() << std::endl;
    return 0;
  }
};

template<class L>
auto generate(L) -> void
{
  entry_wrapper<L> x;
  (void)x;
}

auto main(int, char* argv[]) -> int
{
  generate([]() { return "A"; });
  generate([]() { return "B"; });

  std::cout << __builtin_unique_stable_name(double) << std::endl;
  std::cout << argv[0] << std::endl;
}
