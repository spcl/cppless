#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <cppless/dispatcher/common.hpp>
#include <cppless/dispatcher/sendable.hpp>
#include <cppless/graph/graph.hpp>
#include <cppless/utils/tracing.hpp>
#include <cppless/utils/tuple.hpp>

namespace cppless::executor
{

template<class Dispatcher>
class host_controller_executor
    : public std::enable_shared_from_this<host_controller_executor<Dispatcher>>
{
  using executor_type = host_controller_executor<Dispatcher>;

public:
  using default_lambda_factory = lambda_task_factory<Dispatcher>;

  template<class Config>
  using lambda_factory = lambda_task_factory<Dispatcher, Config>;

  class node_core : public graph::basic_node_core<executor_type>
  {
  public:
    node_core(std::size_t id,
              std::weak_ptr<graph::builder_core<executor_type>> builder,
              std::optional<tracing_span_ref> span)
        : graph::basic_node_core<executor_type>(
            id, std::move(builder), std::move(span))
    {
    }

    virtual auto propagate_value() -> void = 0;
    virtual auto run(typename Dispatcher::instance& dispatcher) -> int = 0;

    [[nodiscard]] auto dependency_count() const -> int
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
                      std::weak_ptr<graph::builder_core<executor_type>> builder,
                      std::optional<tracing_span_ref> span)
        : graph::node_core<executor_type>(id, builder, span)
        , graph::basic_receiver<executor_type, std::tuple<Args...>>(
              id, builder, span)
    {
    }

    auto args() -> std::tuple<Args...>
    {
      return map_tuple(
          this->slots(),
          []<class R, class T>(std::shared_ptr<graph::receiver_slot<R, T>> slot)
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
           std::weak_ptr<graph::builder_core<executor_type>> builder,
           std::optional<tracing_span_ref> span)
        : graph::node_core<executor_type>(id, builder, span)
        , graph::basic_sender<executor_type, Res>(id, builder, span)
    {
    }

    auto result() -> Res&
    {
      return m_future.value();
    }

    auto future() -> cppless::shared_future<Res>&
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
           std::weak_ptr<graph::builder_core<executor_type>> builder,
           std::optional<tracing_span_ref> span)
        : graph::node_core<executor_type>(id, builder, span)
        , graph::basic_sender<executor_type, void>(id, builder, span)
    {
    }

    auto future() -> std::shared_ptr<void>
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
        std::optional<tracing_span_ref> span,
        Task& task)
        : graph::node_core<executor_type>(id, builder, span)
        , graph::basic_task_node<executor_type, Task>(id, builder, span, task)
    {
    }

    auto run(typename Dispatcher::instance& dispatcher) -> int override
    {
      typename Task::args arg_values = this->args();

      auto dispatch_span = this->create_child_tracing_span("dispatch");
      if (dispatch_span) {
        m_dispatch_span.emplace(*dispatch_span);
        m_dispatch_span->start();
      }
      int fut_id = dispatcher.dispatch_impl(
          this->task(), this->future().value(), arg_values, m_dispatch_span);

      return fut_id;
    }
    auto propagate_value() -> void override
    {
      if (m_dispatch_span) {
        m_dispatch_span->end();
      }

      typename Task::res& res_value = this->result();
      std::shared_ptr<graph::builder_core<host_controller_executor<Dispatcher>>>
          builder = this->builder();
      std::shared_ptr<executor_type> exec = builder->executor();
      for (auto& successor : graph::sender<host_controller_executor<Dispatcher>,
                                           typename Task::res>::successors())
      {
        successor->set_value(res_value);
        exec->notify(successor->owning_node_id());
      }
    }

  private:
    std::optional<tracing_span_ref> m_dispatch_span;
  };

  class source_node
      : virtual public graph::node_core<executor_type>
      , public graph::basic_source_node<executor_type>
  {
  public:
    source_node(std::size_t id,
                std::weak_ptr<graph::builder_core<executor_type>> builder,
                std::optional<tracing_span_ref> span)
        : graph::node_core<executor_type>(id, builder, span)
        , graph::basic_source_node<executor_type>(id, builder, span)
    {
    }

    auto run(typename Dispatcher::instance& /*dispatcher*/) -> int override
    {
      auto run_span = this->create_child_tracing_span("run");
      if (run_span) {
        run_span->start();
        run_span->end();
      }
      return -1;
    }
    auto propagate_value() -> void override
    {
      std::shared_ptr<graph::builder_core<host_controller_executor<Dispatcher>>>
          builder = this->builder();
      std::shared_ptr<executor_type> exec = builder->executor();
      for (auto& successor : graph::sender<host_controller_executor<Dispatcher>,
                                           void>::successors())
      {
        exec->notify(successor->owning_node_id());
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

    std::unordered_set<std::size_t> running_nodes;

    int finished_nodes = 0;

    // Populate the m_ready_nodes vector with all nodes that don't have any
    // dependencies

    for (auto& node : builder->nodes()) {
      if (node->dependency_count() == 0) {
        m_ready_nodes.push_back(node->id());
      }
    }

    do {
      // Run all ready nodes
      while (!m_ready_nodes.empty()) {
        std::size_t node_id = m_ready_nodes.back();
        auto node = builder->node(node_id);
        m_ready_nodes.pop_back();

        int future_id = node->run(m_instance);

        if (future_id == -1) {
          m_finished_nodes++;
          finished_nodes++;

          node->propagate_value();
        } else {
          running_nodes.insert(node_id);
          m_future_node_map[future_id] = node->id();
        }
      }

      if (running_nodes.empty()) {
        break;
      }

      auto res = m_instance.wait_one();
      int finished = std::get<0>(res);

      std::size_t finished_node_id = m_future_node_map[finished];
      auto node = builder->node(finished_node_id);

      m_finished_nodes++;
      finished_nodes++;
      running_nodes.erase(node->id());
      // Propagate the value
      node->propagate_value();
      // This also adds the node to the ready nodes
    } while (true);
  }

  auto notify(std::size_t id) -> void
  {
    auto builder = m_builder.lock();
    if (!builder) {
      return;
    }

    // Decrease dependency count
    auto node = builder->node(id);
    node->decrement_dependency_count();
    if (node->dependency_count() == 0) {
      m_ready_nodes.push_back(node->id());
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
