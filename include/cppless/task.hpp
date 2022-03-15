#pragma once

#include <string>

#include "sendable.hpp"

template<typename T>
class task;

template<typename Res, typename... Args>
class task<Res(Args...)>
{
public:
  template<typename F>
  task(F f)
  {
    sendable_functor.reset(new sendable<F, Args...>(f));
  }

  std::string serialize(byte_sink& dst, Args&&... args)
  {
    return sendable_functor->serialize(dst, std::forward<Args>(args)...);
  }

  Res deserialize(byte_source& src)
  {
    return sendable_functor->deserialize(std::forward<byte_source>(src));
  }

private:
  std::unique_ptr<sendable_base<Res, Args...>> sendable_functor;
};

template<typename F>
class static_task
{
public:
  static_task(F f)
      : f(f)
  {
  }

  template<typename... Args>
  std::string serialize(byte_sink& dst, Args&&... args)
  {
    serialize_all<Args...>(dst, args...);
    std::string type = typeid(F).name();
    return __BASE_FILE__ + std::string(":") + type;
  }

private:
  F f;
};