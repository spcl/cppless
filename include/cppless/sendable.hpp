#pragma once

#include <string>
#include <type_traits>

template<typename Res, typename... Args>
struct sendable_base
{
  virtual std::string serialize(byte_sink& ds, Args&&... args) = 0;
  virtual Res deserialize(byte_source& data) = 0;

  virtual ~sendable_base() {}
};

template<typename F, typename... Args>
std::string serialize_lambda(byte_sink& dst, F&& f, Args&&... args)
{
  auto* f_ptr = new F(std::forward<F>(f));
  serialize_all(dst, f_ptr, std::forward<Args>(args)...);
  std::string type = typeid(F).name();
  return __BASE_FILE__ + std::string(":") + type;
}

template<typename F, typename... Args>
struct sendable
    : sendable_base<typename std::invoke_result<F, Args...>::type, Args...>
{
public:
  sendable(F sendable_functor)
      : sendable_functor(sendable_functor)
  {
  }

  F sendable_functor;

  virtual std::string serialize(byte_sink& dst, Args&&... args)
  {
    serialize_all<Args...>(dst, args...);
    std::string type = typeid(F).name();
    return __BASE_FILE__ + std::string(":") + type;
  }

  virtual typename std::invoke_result<F, Args...>::type deserialize(
      byte_source& src)
  {
    typename std::invoke_result<F, Args...>::type x;
    return x;
  }

  __attribute((entry)) static int main(int argc, char** argv)
  {
    // Hello
  }
};