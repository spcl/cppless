#include <iostream>
#include <string_view>

struct default_aws_lambda_task_configuration
{
  constexpr static std::string_view description = "";
  constexpr static unsigned int memory = 1024;  // MB
  constexpr static unsigned int ephemeral_storage = 512;  // MB
  constexpr static unsigned int timeout = 60 * 5;  // seconds
};

template<class A, A Description>
struct with_description
{
  template<class Base>
  struct apply : public Base
  {
    constexpr static A description = Description;
  };
};

template<unsigned int Memory>
struct with_memory
{
  template<class Base>
  struct apply : public Base
  {
    constexpr static unsigned int memory = Memory;
  };
};

template<unsigned int EphemeralStorage>
struct with_ephemeral_storage
{
  template<class Base>
  struct apply : public Base
  {
    constexpr static unsigned int ephemeral_storage = EphemeralStorage;
  };
};

template<unsigned int Timeout>
class with_timeout
{
  template<class Base>
  class apply : public Base
  {
    constexpr static unsigned int timeout = Timeout;
  };
};

template<class... Modifiers>
class aws_lambda_config;

template<class Modifier, class... Modifiers>
class aws_lambda_config<Modifier, Modifiers...>
    : public Modifier::template apply<aws_lambda_config<Modifiers...>>
{
};

template<>
class aws_lambda_config<> : public default_aws_lambda_task_configuration
{
};

using namespace std;
int main(int argc, char* argv[])
{
  constexpr auto memory = 2048;
  constexpr auto ephemeral_storage = 128;
  using config = aws_lambda_config<with_memory<memory>,
                                   with_ephemeral_storage<ephemeral_storage>>;

  std::cout << "description: " << config::description << std::endl;
  std::cout << "memory: " << config::memory << std::endl;
  std::cout << "ephemeral_storage: " << config::ephemeral_storage << std::endl;
  std::cout << "timeout: " << config::timeout << std::endl;
}