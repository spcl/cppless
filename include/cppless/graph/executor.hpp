#pragma once

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <cppless/utils/empty.hpp>
#include <cppless/utils/tuple.hpp>

#include "cppless/dispatcher/common.hpp"

namespace cppless::executor
{
template<class Dispatcher>
class executor_core;

class basic_node_core
{
public:
  explicit basic_node_core(size_t id)
      : m_id(id)
  {
  }
  virtual ~basic_node_core() = default;
  virtual auto successors()
      -> std::vector<std::shared_ptr<basic_node_core>> = 0;
  virtual auto is_ready() -> bool = 0;
  virtual auto propagate_value() -> void = 0;

  [[nodiscard]] auto get_id() const -> size_t
  {
    return m_id;
  }

  [[nodiscard]] auto get_dependency_count() const -> int
  {
    return m_dependency_count;
  }

  auto increment_dependency_count() -> void
  {
    ++m_dependency_count;
  }

  auto decrement_dependency_count() -> void
  {
    --m_dependency_count;
  }

private:
  size_t m_id;
  int m_dependency_count = 0;
};

template<class Dispatcher>
class node_core : public basic_node_core

{
public:
  using dispatcher = Dispatcher;

  explicit node_core(size_t id,
                     std::weak_ptr<executor_core<Dispatcher>> executor)
      : basic_node_core(id)
      , m_executor(std::move(executor))
  {
  }

  virtual auto run(typename Dispatcher::instance& dispatcher) -> int = 0;
  auto get_executor() -> std::shared_ptr<executor_core<Dispatcher>>
  {
    return m_executor.lock();
  }

private:
  std::weak_ptr<executor_core<Dispatcher>> m_executor;
};

template<class Arg>
class receiver_slot
{
public:
  using arg_type = Arg;

  explicit receiver_slot(size_t owning_node_id)
      : m_owning_node_id(owning_node_id)
  {
  }

  [[nodiscard]] auto is_connected() const -> bool
  {
    return m_is_connected;
  }

  [[nodiscard]] auto is_ready() const -> bool
  {
    return m_arg.has_value();
  }

  auto get() -> std::optional<Arg> const&
  {
    return m_arg;
  }

  auto set_value(Arg arg) -> void
  {
    m_arg = std::move(arg);
  }

  [[nodiscard]] auto get_owning_node_id() const -> size_t
  {
    return m_owning_node_id;
  }

private:
  std::optional<Arg> m_arg;
  bool m_is_connected = false;
  size_t m_owning_node_id = 0UL;
};

template<>
class receiver_slot<void>
{
public:
  using arg_type = void;

  explicit receiver_slot(size_t owning_node_id)
      : m_owning_node_id(owning_node_id)
  {
  }

  [[nodiscard]] auto is_connected() const -> bool
  {
    return m_is_connected;
  }

  [[nodiscard]] auto is_ready() const -> bool
  {
    return m_is_ready;
  }

  auto set_value() -> void
  {
    m_is_ready = true;
  }

  [[nodiscard]] auto get_owning_node_id() const -> size_t
  {
    return m_owning_node_id;
  }

private:
  bool m_is_ready = false;
  bool m_is_connected = false;
  size_t m_owning_node_id = 0UL;
};

template<class Dispatcher, class... Args>
class receiver;

template<class Dispatcher, class... Args>
class receiver<Dispatcher, std::tuple<Args...>>
    : virtual public node_core<Dispatcher>
{
public:
  explicit receiver(size_t id)
      : m_slots({})
      , m_empty_slots({})
  {
    fill_tuple(m_slots,
               [id]<class T>(std::shared_ptr<receiver_slot<T>>& slot)
               { slot = std::make_shared<receiver_slot<T>>(id); });
  }
  auto is_ready() -> bool
  {
    for (auto& slot : m_empty_slots) {
      if (!slot->is_ready()) {
        return false;
      }
    }
    return reduce_tuple(
        m_slots,
        [](bool ready, const auto& slot) { return ready && slot->is_ready(); },
        true);
  }

  template<int I>
  auto get_slot() -> std::shared_ptr<
      receiver_slot<std::tuple_element_t<I, std::tuple<Args...>>>>
  {
    return std::get<I>(m_slots);
  }

  auto get_args() -> std::tuple<Args...>
  {
    return map_tuple(m_slots,
                     []<class T>(std::shared_ptr<receiver_slot<T>> slot)
                     {
                       const std::optional<T>& arg = slot->get();
                       return arg.value();
                     });
  }

  auto create_empty_slot() -> std::shared_ptr<receiver_slot<void>>
  {
    auto slot = std::make_shared<receiver_slot<void>>(this->get_id());
    m_empty_slots.push_back(slot);
    return slot;
  }

private:
  std::tuple<std::shared_ptr<receiver_slot<Args>>...> m_slots {};
  std::vector<std::shared_ptr<receiver_slot<void>>> m_empty_slots {};
};

template<class Dispatcher, class Res>
class sender : virtual public node_core<Dispatcher>
{
public:
  using dispatcher = Dispatcher;
  using sending_type = Res;

  auto add_successor(std::shared_ptr<receiver_slot<Res>> successor) -> void
  {
    m_successors.push_back(successor);
  }

  auto get_successors() -> std::vector<std::shared_ptr<receiver_slot<Res>>>&
  {
    return m_successors;
  }

  auto set_future(cppless::identified_shared_future<Res>& future) -> void
  {
    m_result = future;
  }

  auto get_result() -> Res&
  {
    return m_result.value().get_value();
  }

private:
  std::optional<cppless::identified_shared_future<Res>> m_result;
  std::vector<std::shared_ptr<receiver_slot<Res>>> m_successors {};
};

// Specialization for void
template<class Dispatcher>
class sender<Dispatcher, void> : virtual public node_core<Dispatcher>
{
public:
  using dispatcher = Dispatcher;
  using sending_type = void;

  auto add_successor(const std::shared_ptr<receiver_slot<void>>& successor)
      -> void
  {
    m_successors.push_back(successor);
  }

  auto get_successors() -> std::vector<std::shared_ptr<receiver_slot<void>>>&
  {
    return m_successors;
  }

private:
  std::vector<std::shared_ptr<receiver_slot<void>>> m_successors {};
};

template<class Dispatcher, class Res>
using shared_sender = std::shared_ptr<sender<Dispatcher, Res>>;

template<class Dispatcher, class Task>
class node
    : virtual public node_core<Dispatcher>
    , public receiver<Dispatcher, typename Task::args>
    , public sender<Dispatcher, typename Task::res>
{
public:
  using task = Task;
  using args = typename task::args;
  using res = typename task::res;
  using dispatcher = Dispatcher;
  using sending_type = typename task::res;

  node(size_t id, std::weak_ptr<executor_core<Dispatcher>> executor, Task& task)
      : node_core<Dispatcher>(id, std::move(executor))
      , receiver<Dispatcher, args>(id)
      , sender<Dispatcher, res>()
      , m_task(std::move(task))
  {
  }

  auto run(typename Dispatcher::instance& dispatcher) -> int override
  {
    args arg_values = this->get_args();
    cppless::identified_shared_future<res> id_fut =
        dispatcher.dispatch(m_task, arg_values);
    this->set_future(id_fut);
    return id_fut.get_id();
  }
  auto successors() -> std::vector<std::shared_ptr<basic_node_core>> override
  {
    throw std::runtime_error("not yet implemented");
  }
  auto is_ready() -> bool override
  {
    return static_cast<receiver<Dispatcher, args>*>(this)->is_ready();
  }

  auto propagate_value() -> void override
  {
    res& res_value = this->get_result();
    std::shared_ptr<executor_core<Dispatcher>> executor = this->get_executor();
    for (auto& successor : sender<Dispatcher, res>::get_successors()) {
      successor->set_value(res_value);
      executor->notify(successor->get_owning_node_id());
    }
  }

private:
  Task m_task;
};

template<class Dispatcher>
class source_node
    : virtual public node_core<Dispatcher>
    , public sender<Dispatcher, void>
{
public:
  using res = void;
  using dispatcher = Dispatcher;
  using sending_type = void;

  source_node(size_t id, std::weak_ptr<executor_core<Dispatcher>> executor)
      : node_core<Dispatcher>(id, std::move(executor))
      , sender<Dispatcher, res>()

  {
  }

  auto run(typename Dispatcher::instance& /*dispatcher*/) -> int override
  {
    return -1;
  }
  auto successors() -> std::vector<std::shared_ptr<basic_node_core>> override
  {
    throw std::runtime_error("not yet implemented");
  }
  auto is_ready() -> bool override
  {
    return true;
  }

  auto propagate_value() -> void override
  {
    std::shared_ptr<executor_core<Dispatcher>> executor = this->get_executor();
    for (auto& successor : sender<Dispatcher, res>::get_successors()) {
      successor->set_value();
      executor->notify(successor->get_owning_node_id());
    }
  }
};

template<class Dispatcher>
class executor_core
    : public std::enable_shared_from_this<executor_core<Dispatcher>>
{
public:
  executor_core(std::shared_ptr<Dispatcher> dispatcher)
      : m_instance(dispatcher->create_instance())
      , m_dispatcher(dispatcher)
  {
  }

  template<class Task>
  auto create_node(Task& task) -> std::shared_ptr<node<Dispatcher, Task>>
  {
    int next_id = static_cast<int>(m_nodes.size());
    auto self = this->shared_from_this();
    auto new_node =
        std::make_shared<node<Dispatcher, Task>>(next_id, self, task);
    m_nodes.push_back(new_node);
    return new_node;
  }

  auto create_source_node() -> std::shared_ptr<source_node<Dispatcher>>
  {
    int next_id = static_cast<int>(m_nodes.size());
    auto self = this->shared_from_this();
    auto new_node = std::make_shared<source_node<Dispatcher>>(next_id, self);
    m_nodes.push_back(new_node);
    return new_node;
  }

  auto get_node(int id) -> std::shared_ptr<node_core<Dispatcher>>
  {
    return m_nodes[id];
  }

  auto await_all() -> void
  {
    auto start = std::chrono::high_resolution_clock::now();

    std::unordered_set<size_t> running_nodes;
    int finished_nodes = 0;

    // Populate the m_ready_nodes vector with all nodes that don't have any
    // dependencies
    for (auto& node : m_nodes) {
      if (node->get_dependency_count() == 0) {
        m_ready_nodes.push_back(node->get_id());
      }
    }
    do {
      // Run all ready nodes
      while (!m_ready_nodes.empty()) {
        size_t node_id = m_ready_nodes.back();
        auto node = m_nodes[node_id];
        m_ready_nodes.pop_back();
        int future_id = node->run(m_instance);
        if (future_id == -1) {
          m_finished_nodes++;
          finished_nodes++;
          node->propagate_value();
        } else {
          running_nodes.insert(node_id);
          m_future_node_map[future_id] = node->get_id();
        }
      }

      if (running_nodes.empty()) {
        break;
      }

      int finished = m_instance.wait_one();
      size_t finished_node_id = m_future_node_map[finished];
      auto node = m_nodes[finished_node_id];
      m_finished_nodes++;
      finished_nodes++;
      running_nodes.erase(node->get_id());
      // Propagate the value
      node->propagate_value();
      // This also adds the node to the ready nodes
    } while (true);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "[executor] Executed " << finished_nodes << " tasks in "
              << duration.count() << "ms" << std::endl;
  }

  auto notify(size_t id) -> void
  {
    // Decrease dependency count
    auto node = m_nodes[id];
    node->decrement_dependency_count();
    if (node->get_dependency_count() == 0) {
      m_ready_nodes.push_back(node->get_id());
    }
  }

private:
  size_t m_finished_nodes = 0;
  std::vector<size_t> m_ready_nodes {};
  std::unordered_map<int, size_t> m_future_node_map {};
  std::vector<std::shared_ptr<node_core<Dispatcher>>> m_nodes {};
  typename Dispatcher::instance m_instance;
  std::shared_ptr<Dispatcher> m_dispatcher;
};

template<class Dispatcher>
class executor
{
public:
  explicit executor(std::shared_ptr<Dispatcher> dispatcher)
      : m_core(
          std::make_shared<executor_core<Dispatcher>>(std::move(dispatcher)))
  {
  }

  auto get_core() -> std::shared_ptr<executor_core<Dispatcher>>
  {
    return m_core;
  }

  template<class Task>
  auto create_node(Task& task) -> std::shared_ptr<node<Dispatcher, Task>>
  {
    return m_core->create_node(task);
  }

  auto create_source_node() -> std::shared_ptr<source_node<Dispatcher>>
  {
    return m_core->create_source_node();
  }

  auto await_all() -> void
  {
    m_core->await_all();
  }

private:
  std::shared_ptr<executor_core<Dispatcher>> m_core;
};

}  // namespace cppless::executor