#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <cppless/graph/graph.hpp>
#include <cppless/utils/tuple.hpp>

#include "cppless/dispatcher/common.hpp"

namespace cppless::executor
{

template<class Dispatcher>
class host_controller_executor
    : public std::enable_shared_from_this<host_controller_executor<Dispatcher>>
{
  using executor_type = host_controller_executor<Dispatcher>;

public:
  using task = typename Dispatcher::task;

  class node_core : public graph::basic_node_core<executor_type>
  {
  public:
    node_core(std::size_t id,
              std::weak_ptr<graph::builder_core<executor_type>> builder)
        : graph::basic_node_core<executor_type>(id, std::move(builder))
    {
    }

    virtual auto propagate_value() -> void = 0;
    virtual auto run(typename Dispatcher::instance& dispatcher) -> int = 0;

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
    int m_dependency_count = 0;
  };

  template<class Arg>
  class receiver_slot : public graph::basic_receiver_slot<executor_type, Arg>
  {
  public:
    explicit receiver_slot(std::size_t owning_node_id)
        : graph::basic_receiver_slot<executor_type, Arg>(owning_node_id)
    {
    }

    auto get() -> std::optional<Arg> const&
    {
      return m_arg;
    }

    auto set_value(Arg arg) -> void
    {
      m_arg = std::move(arg);
    }

  private:
    std::optional<Arg> m_arg;
  };

  template<>
  class receiver_slot<void>
      : public graph::basic_receiver_slot<executor_type, void>
  {
  public:
    explicit receiver_slot(std::size_t owning_node_id)
        : graph::basic_receiver_slot<executor_type, void>(owning_node_id)
    {
    }
  };

  template<class... Args>
  class receiver;

  template<class... Args>
  class receiver<std::tuple<Args...>>
      : public virtual graph::node_core<executor_type>
      , public graph::basic_receiver<executor_type, std::tuple<Args...>>
  {
  public:
    explicit receiver(std::size_t id,
                      std::weak_ptr<graph::builder_core<executor_type>> builder)
        : graph::node_core<executor_type>(id, builder)
        , graph::basic_receiver<executor_type, std::tuple<Args...>>(id, builder)
    {
    }

    auto get_args() -> std::tuple<Args...>
    {
      return map_tuple(this->get_slots(),
                       []<class T>(std::shared_ptr<receiver_slot<T>> slot)
                       {
                         const std::optional<T>& arg = slot->get();
                         return arg.value();
                       });
    }
  };

  template<class Res>
  class sender
      : public virtual graph::node_core<executor_type>
      , public graph::basic_sender<executor_type, Res>
  {
  public:
    sender(std::size_t id,
           std::weak_ptr<graph::builder_core<executor_type>> builder)
        : graph::node_core<executor_type>(id, builder)
        , graph::basic_sender<executor_type, Res>(id, builder)
    {
    }

    auto get_result() -> Res&
    {
      return m_future.get_value();
    }

    auto get_future() -> cppless::shared_future<Res>&
    {
      return m_future;
    }

  private:
    cppless::shared_future<Res> m_future;
  };

  template<>
  class sender<void> : public graph::basic_sender<executor_type, void>
  {
  public:
    sender(std::size_t id,
           std::weak_ptr<graph::builder_core<executor_type>> builder)
        : graph::node_core<executor_type>(id, builder)
        , graph::basic_sender<executor_type, void>(id, builder)
    {
    }

    auto get_future() -> std::shared_ptr<void>
    {
      return nullptr;
    }
  };

  template<class Task>
  class task_node
      : virtual public graph::node_core<executor_type>
      , public graph::basic_task_node<executor_type, Task>
  {
  public:
    explicit task_node(
        std::size_t id,
        std::weak_ptr<graph::builder_core<executor_type>> builder,
        Task& task)
        : graph::node_core<executor_type>(id, builder)
        , graph::basic_task_node<executor_type, Task>(id, builder, task)
    {
    }

    auto run(typename Dispatcher::instance& dispatcher) -> int override
    {
      typename Task::args arg_values = this->get_args();
      cppless::graph::tracing_span dispatch_span {"dispatch"};
      int fut_id =
          dispatcher.dispatch(this->get_task(), this->get_future(), arg_values);
      dispatch_span.end();
      this->add_tracing_span(dispatch_span);
      return fut_id;
    }
    auto propagate_value() -> void override
    {
      typename Task::res& res_value = this->get_result();
      std::shared_ptr<graph::builder_core<host_controller_executor<Dispatcher>>>
          builder = this->get_builder();
      std::shared_ptr<executor_type> exec = builder->get_executor();
      for (auto& successor :
           graph::sender<host_controller_executor<Dispatcher>,
                         typename Task::res>::get_successors())
      {
        successor->set_value(res_value);
        exec->notify(successor->get_owning_node_id());
      }
    }
  };

  class source_node
      : virtual public graph::node_core<executor_type>
      , public graph::basic_source_node<executor_type>
  {
  public:
    source_node(std::size_t id,
                std::weak_ptr<graph::builder_core<executor_type>> builder)
        : graph::node_core<executor_type>(id, builder)
        , graph::basic_source_node<executor_type>(id, builder)
    {
    }

    auto run(typename Dispatcher::instance& /*dispatcher*/) -> int override
    {
      return -1;
    }
    auto propagate_value() -> void override
    {
      std::shared_ptr<graph::builder_core<host_controller_executor<Dispatcher>>>
          builder = this->get_builder();
      std::shared_ptr<executor_type> exec = builder->get_executor();
      for (auto& successor : graph::sender<host_controller_executor<Dispatcher>,
                                           void>::get_successors())
      {
        exec->notify(successor->get_owning_node_id());
      }
    }
  };

  explicit host_controller_executor(std::shared_ptr<Dispatcher> dispatcher)
      : m_instance(dispatcher->create_instance())
      , m_dispatcher(dispatcher)
  {
  }

  auto set_builder(std::weak_ptr<graph::builder_core<executor_type>> builder)
      -> void
  {
    m_builder = builder;
  }

  auto await_all() -> void
  {
    auto builder = m_builder.lock();
    if (!builder) {
      return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::unordered_set<std::size_t> running_nodes;

    std::unordered_map<std::size_t, cppless::graph::tracing_span> wait_spans;
    int finished_nodes = 0;

    // Populate the m_ready_nodes vector with all nodes that don't have any
    // dependencies

    for (auto& node : builder->get_nodes()) {
      if (node->get_dependency_count() == 0) {
        m_ready_nodes.push_back(node->get_id());
      }
    }

    do {
      // Run all ready nodes
      while (!m_ready_nodes.empty()) {
        std::size_t node_id = m_ready_nodes.back();
        auto node = builder->get_node(node_id);
        m_ready_nodes.pop_back();
        int future_id = node->run(m_instance);

        wait_spans.emplace(node_id, "wait");

        if (future_id == -1) {
          m_finished_nodes++;
          finished_nodes++;

          auto span_it = wait_spans.find(node_id);
          if (span_it != wait_spans.end()) {
            cppless::graph::tracing_span& span = span_it->second;
            span.end();
            node->add_tracing_span(span);
            wait_spans.erase(span_it);
          }
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
      std::size_t finished_node_id = m_future_node_map[finished];
      auto node = builder->get_node(finished_node_id);

      auto span_it = wait_spans.find(finished_node_id);
      if (span_it != wait_spans.end()) {
        cppless::graph::tracing_span& span = span_it->second;
        span.end();
        node->add_tracing_span(span);
        wait_spans.erase(span_it);
      }

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

  auto notify(std::size_t id) -> void
  {
    auto builder = m_builder.lock();
    if (!builder) {
      return;
    }

    // Decrease dependency count
    auto node = builder->get_node(id);
    node->decrement_dependency_count();
    if (node->get_dependency_count() == 0) {
      m_ready_nodes.push_back(node->get_id());
    }
  }

private:
  std::weak_ptr<graph::builder_core<executor_type>> m_builder;

  std::size_t m_finished_nodes = 0;
  std::vector<std::size_t> m_ready_nodes {};
  std::unordered_map<int, std::size_t> m_future_node_map {};
  typename Dispatcher::instance m_instance;
  std::shared_ptr<Dispatcher> m_dispatcher;
};
}  // namespace cppless::executor
