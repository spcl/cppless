#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

namespace cppless
{

template<class Res, class... Args>
class execution_node
{
public:
  execution_node() = default;
  // From input nodes

  auto push_completion_listener(std::function<void(Res)> listener) -> void
  {
    m_completion_listeners.push_back(listener);
  }

  auto execute() -> void
  {
    std::cout << "Executing " << this << std::endl;
  }

private:
  std::vector<std::function<void(Res)>> m_completion_listeners;
  std::tuple<std::optional<Args>...> m_args;
};

template<class Res>
class sender
{
public:
  template<class... Args>
  explicit sender(const std::shared_ptr<execution_node<Res, Args...>>& node)
  {
    m_push_completion_listener = [=](std::function<void(Res)> f) {  // NOLINT
      node->push_completion_listener(f);
    };
  }
  auto push_completion_listener(const std::function<void(Res)>& listener)
      -> void
  {
    m_push_completion_listener(listener);
  }

private:
  std::function<void(std::function<void(Res)>)> m_push_completion_listener;
};

template<>
class sender<void>
{
public:
  template<class... Args>
  explicit sender(const std::shared_ptr<execution_node<void, Args...>>& node)
  {
    m_push_completion_listener = [=](std::function<void()> f) {  // NOLINT
      node->push_completion_listener(f);
    };
  }
  auto push_completion_listener(const std::function<void(void)>& listener)
      -> void
  {
    m_push_completion_listener(listener);
  }

private:
  std::function<void(std::function<void()>)> m_push_completion_listener;
};

template<class Dispatcher>
class executor
{
public:
  explicit executor(Dispatcher& dispatcher)
      : m_dispatcher(dispatcher)
  {
  }

  auto schedule() -> sender<void>
  {
    auto node = std::make_shared<execution_node<void>>();
    m_sources.push_back(node);
    sender<void> s {node};
    return s;
  }

  auto execute() -> void
  {
    for (auto& source : m_sources) {
      std::cout << "Executing source" << std::endl;
      source->execute();
    }
  }

private:
  Dispatcher& m_dispatcher;
  std::vector<std::shared_ptr<execution_node<void>>> m_sources;
};
}  // namespace cppless