#pragma once
#include <algorithm>
#include <memory>
#include <optional>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/detail/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/utils/tuple.hpp>

namespace cppless::graph
{

template<class Executor>
class builder_core;

template<class Executor>
class basic_node_core
{
public:
  using executor = Executor;

  basic_node_core(std::size_t id, std::weak_ptr<builder_core<Executor>> builder)
      : m_id(id)
      , m_builder(std::move(builder))
  {
  }
  virtual ~basic_node_core() = default;

  // Delete copy and move constructors and assign operators
  basic_node_core(const basic_node_core&) = delete;
  basic_node_core(basic_node_core&&) = delete;

  basic_node_core& operator=(const basic_node_core&) = delete;
  basic_node_core& operator=(basic_node_core&&) = delete;

  virtual auto get_successor_ids() -> std::vector<std::size_t> = 0;

  [[nodiscard]] auto get_id() const -> std::size_t
  {
    return m_id;
  }

  [[nodiscard]] auto get_builder() const
      -> std::shared_ptr<builder_core<executor>>
  {
    return m_builder.lock();
  }

private:
  std::size_t m_id;
  std::weak_ptr<builder_core<Executor>> m_builder;
};

template<class Executor>
class node_core : public Executor::node_core
{
public:
  explicit node_core(std::size_t id,
                     std::weak_ptr<builder_core<Executor>> builder)
      : Executor::node_core(id, std::move(builder))
  {
  }
};

template<class Executor, class Arg>
class basic_receiver_slot
{
public:
  using arg_type = Arg;

  explicit basic_receiver_slot(std::size_t owning_node_id)
      : m_owning_node_id(owning_node_id)
  {
  }

  [[nodiscard]] auto is_connected() const -> bool
  {
    return m_is_connected;
  }

  [[nodiscard]] auto get_owning_node_id() const -> std::size_t
  {
    return m_owning_node_id;
  }

private:
  bool m_is_connected = false;
  std::size_t m_owning_node_id = 0UL;
};

template<class Executor, class... Args>
class receiver_slot : public Executor::template receiver_slot<Args...>
{
public:
  explicit receiver_slot(std::size_t owning_node_id)
      : Executor::template receiver_slot<Args...>(owning_node_id)
  {
  }
};

template<class Executor, class... Args>
class basic_receiver;

template<class Executor, class... Args>
class basic_receiver<Executor, std::tuple<Args...>>
    : virtual public node_core<Executor>
{
public:
  explicit basic_receiver(std::size_t id,
                          std::weak_ptr<builder_core<Executor>> builder)
      : node_core<Executor>(id, std::move(builder))
      , m_slots()

  {
    fill_tuple(m_slots,
               [id]<class T>(std::shared_ptr<receiver_slot<Executor, T>>& slot)
               { slot = std::make_shared<receiver_slot<Executor, T>>(id); });
  }

  template<int I>
  auto get_slot() -> std::shared_ptr<
      receiver_slot<Executor, std::tuple_element_t<I, std::tuple<Args...>>>>
  {
    return std::get<I>(m_slots);
  }

  auto create_empty_slot() -> std::shared_ptr<receiver_slot<Executor, void>>
  {
    auto slot = std::make_shared<receiver_slot<Executor, void>>(this->get_id());
    m_empty_slots.push_back(slot);
    return slot;
  }

  auto get_slots()
      -> std::tuple<std::shared_ptr<receiver_slot<Executor, Args>>...>
  {
    return m_slots;
  }

  auto get_empty_slots()
      -> std::vector<std::shared_ptr<receiver_slot<Executor, void>>>
  {
    return m_empty_slots;
  }

private:
  std::tuple<std::shared_ptr<receiver_slot<Executor, Args>>...> m_slots;
  std::vector<std::shared_ptr<receiver_slot<Executor, void>>> m_empty_slots;
};

template<class Executor, class... Args>
class receiver;

template<class Executor, class... Args>
class receiver<Executor, std::tuple<Args...>>
    : public virtual node_core<Executor>
    , public Executor::template receiver<std::tuple<Args...>>
{
public:
  explicit receiver(std::size_t id,
                    std::weak_ptr<builder_core<Executor>> builder)
      : node_core<Executor>(id, builder)
      , Executor::template receiver<std::tuple<Args...>>(id, builder)
  {
  }
};

template<class Executor, class Res>
class basic_sender : virtual public node_core<Executor>
{
public:
  basic_sender(std::size_t id, std::weak_ptr<builder_core<Executor>> builder)
      : node_core<Executor>(id, std::move(builder))
  {
  }

  using executor = Executor;
  using sending_type = Res;

  auto add_successor(std::shared_ptr<receiver_slot<Executor, Res>> successor)
      -> void
  {
    m_successors.push_back(successor);
  }

  auto get_successors()
      -> std::vector<std::shared_ptr<receiver_slot<Executor, Res>>>&
  {
    return m_successors;
  }

  auto get_successor_ids() -> std::vector<std::size_t> override
  {
    std::vector<std::size_t> ids;

    for (auto& successor : m_successors) {
      ids.push_back(successor->get_owning_node_id());
    }
    return ids;
  }

private:
  std::vector<std::shared_ptr<receiver_slot<Executor, Res>>> m_successors {};
};

template<class Executor>
class basic_sender<Executor, void> : virtual public node_core<Executor>
{
public:
  basic_sender(std::size_t id, std::weak_ptr<builder_core<Executor>> builder)
      : node_core<Executor>(id, std::move(builder))
  {
  }

  using executor = Executor;
  using sending_type = void;

  auto add_successor(
      const std::shared_ptr<receiver_slot<Executor, void>>& successor) -> void
  {
    m_successors.push_back(successor);
  }

  auto get_successors()
      -> std::vector<std::shared_ptr<receiver_slot<Executor, void>>>&
  {
    return m_successors;
  }

  auto get_successor_ids() -> std::vector<std::size_t> override
  {
    std::vector<std::size_t> ids;

    for (auto& successor : m_successors) {
      ids.push_back(successor->get_owning_node_id());
    }
    return ids;
  }

private:
  std::vector<std::shared_ptr<receiver_slot<Executor, void>>> m_successors {};
};

template<class Executor, class Res>
class sender
    : public virtual node_core<Executor>
    , public Executor::template sender<Res>
{
public:
  sender(std::size_t id, std::weak_ptr<builder_core<Executor>> builder)
      : node_core<Executor>(id, builder)
      , Executor::template sender<Res>(id, builder)
  {
  }

  auto get_future() -> cppless::shared_future<Res>&
  {
    // Cast to Executor::template sender<Res>
    auto sender = static_cast<typename Executor::template sender<Res>*>(this);
    return sender->get_future();
  }
};

template<class Executor, class Task>
class basic_task_node
    : public virtual node_core<Executor>
    , public receiver<Executor, typename Task::args>
    , public sender<Executor, typename Task::res>
{
public:
  using task = Task;
  using args = typename task::args;
  using res = typename task::res;
  using executor = Executor;
  using sending_type = typename task::res;

  basic_task_node(std::size_t id,
                  std::weak_ptr<builder_core<Executor>> builder,
                  Task& task)
      : node_core<Executor>(id, builder)
      , receiver<Executor, args>(id, builder)
      , sender<Executor, res>(id, builder)
      , m_task(std::move(task))
  {
  }

  auto get_task() -> Task&
  {
    return m_task;
  }

private:
  Task m_task;
};

template<class Executor, class Task>
class task_node : public Executor::template task_node<Task>
{
public:
  task_node(std::size_t id,
            std::weak_ptr<builder_core<Executor>> builder,
            Task& task)
      : node_core<Executor>(id, std::move(builder))
      , Executor::template task_node<Task>(id, builder, task)
  {
  }
};

template<class Executor>
class basic_source_node
    : virtual public node_core<Executor>
    , public sender<Executor, void>
{
public:
  using executor = Executor;
  using sending_type = void;

  basic_source_node(std::size_t id,
                    std::weak_ptr<builder_core<Executor>> builder)
      : node_core<Executor>(id, builder)
      , sender<Executor, void>(id, builder)
  {
  }
};

template<class Executor>
class source_node : public Executor::source_node
{
public:
  source_node(std::size_t id, std::weak_ptr<builder_core<Executor>> builder)
      : node_core<Executor>(id, builder)
      , Executor::source_node(id, builder)
  {
  }
};

template<class Executor>
class builder_core : public std::enable_shared_from_this<builder_core<Executor>>
{
public:
  explicit builder_core(std::shared_ptr<Executor> executor)
      : m_executor(std::move(executor))
  {
  }

  using executor = Executor;

  template<class Task>
  auto create_node(Task& task) -> std::shared_ptr<task_node<Executor, Task>>
  {
    int next_id = static_cast<int>(m_nodes.size());
    auto self = this->shared_from_this();
    auto new_node =
        std::make_shared<task_node<Executor, Task>>(next_id, self, task);
    m_nodes.push_back(new_node);
    return new_node;
  }

  auto create_source_node() -> std::shared_ptr<source_node<Executor>>
  {
    int next_id = static_cast<int>(m_nodes.size());
    auto self = this->shared_from_this();
    auto new_node = std::make_shared<source_node<Executor>>(next_id, self);
    m_nodes.push_back(new_node);
    return new_node;
  }

  auto get_nodes() -> std::vector<std::shared_ptr<node_core<Executor>>>&
  {
    return m_nodes;
  }

  auto get_node(std::size_t id) -> std::shared_ptr<node_core<Executor>>
  {
    return m_nodes[id];
  }

  auto write_graphviz(std::ostream& out)
  {
    using digraph = boost::adjacency_list<
        boost::vecS,
        boost::vecS,
        boost::directedS,
        boost::property<boost::vertex_name_t, std::string>>;
    digraph d(m_nodes.size());
    for (auto& node : m_nodes) {
      for (auto& successor : node->get_successor_ids()) {
        boost::add_edge(node->get_id(), successor, d);
      }
    }
    boost::write_graphviz(out, d);
  }

  auto await_all() -> void
  {
    m_executor->await_all();
  }

  auto get_executor() -> std::shared_ptr<Executor>
  {
    return m_executor;
  }

private:
  std::vector<std::shared_ptr<node_core<Executor>>> m_nodes {};
  std::shared_ptr<Executor> m_executor;
};

template<class Executor>
class builder
{
public:
  template<class... ExecutorArgs>
  explicit builder(ExecutorArgs&&... args)
      : m_core(std::make_shared<builder_core<Executor>>(
          std::make_shared<Executor>(std::forward<ExecutorArgs>(args)...)))
  {
    m_core->get_executor()->set_builder(m_core);
  }

  template<class Task>
  auto create_node(Task& task) -> std::shared_ptr<task_node<Executor, Task>>
  {
    return m_core->create_node(task);
  }

  auto create_source_node() -> std::shared_ptr<source_node<Executor>>
  {
    return m_core->create_source_node();
  }

  auto get_node(int id) -> std::shared_ptr<node_core<Executor>>
  {
    return m_core->get_node(id);
  }

  auto write_graphviz(std::ostream& out)
  {
    m_core->write_graphviz(out);
  }

  auto await_all() -> void
  {
    m_core->await_all();
  }

  auto get_core() -> std::shared_ptr<builder_core<Executor>>
  {
    return m_core;
  }

private:
  std::shared_ptr<builder_core<Executor>> m_core {};
};

}  // namespace cppless::graph