#pragma once

#include <type_traits>

#include <cppless/graph/executor.hpp>
#include <cppless/utils/apply.hpp>
#include <cppless/utils/tuple.hpp>

namespace cppless::execution
{
template<class Executor>
auto schedule(Executor& executor)
{
  return executor.schedule();
}

template<class RT, class... Ts>
struct filter_fn;

template<class RT>
struct filter_fn<RT>
{
  using type = std::function<RT()>;
};

template<class RT, class T, class... Ts>
struct filter_fn<RT, T, Ts...>
{
  template<class... Tail>
  struct cons;

  template<class... Tail>
  struct cons<void, std::function<RT(Tail...)>>
  {
    using type = void;
  };

  template<class Head, class... Tail>
  struct cons<Head, std::function<RT(Tail...)>>
  {
    using type = std::function<RT(Head, Tail...)>;
  };

  using type = typename std::conditional<
      std::is_void<T>::value,
      typename filter_fn<RT, Ts...>::type,
      typename cons<T, typename filter_fn<RT, Ts...>::type>::type>::type;
};

template<int I, class Dispatcher, class OutputTask, class InputTask>
auto connect(
    std::shared_ptr<cppless::executor::node<Dispatcher, OutputTask>>& output,
    std::shared_ptr<cppless::executor::node<Dispatcher, InputTask>>& input)
{
  input->increment_dependency_count();
  auto input_slot = input->template get_slot<I>();
  output->add_successor(input_slot);
}

template<class Dispatcher, class InputTask>
auto schedule(executor::executor<Dispatcher> executor, InputTask input_thing)
{
  typename Dispatcher::task::sendable input_task(input_thing);
  auto node = executor.create_node(input_task);
  return node;
}

template<int I,
         class Dispatcher,
         class InputTask,
         class HeadOutputTask,
         class... TailOutputTasks>
auto then_connect(
    std::shared_ptr<cppless::executor::node<Dispatcher, InputTask>> input_node,
    std::shared_ptr<cppless::executor::node<Dispatcher, HeadOutputTask>> head,
    std::shared_ptr<
        cppless::executor::node<Dispatcher, TailOutputTasks>>... tail) -> void
{
  connect<I>(head, input_node);
  if constexpr (sizeof...(tail) > 0) {
    then_connect<I + 1, Dispatcher, InputTask, TailOutputTasks...>(input_node,
                                                                   tail...);
  }
}

template<class... Args>
auto then(Args... args)
{
  return tail_apply(
      []<class Dispatcher,
         class InputTask,
         class FirstOutputTask,
         class... OutputNodes>(
          InputTask input_thing,
          std::shared_ptr<cppless::executor::node<Dispatcher, FirstOutputTask>>
              first_output_node,
          OutputNodes... output_nodes) {
        typename Dispatcher::task::sendable input_task(input_thing);

        auto executor = first_output_node->get_executor();
        std::shared_ptr<
            cppless::executor::node<Dispatcher, decltype(input_task)>>
            input_node = executor->create_node(input_task);
        then_connect<0, Dispatcher>(
            input_node, first_output_node, output_nodes...);
        return input_node;
      },
      std::forward<Args>(args)...);
}

}  // namespace cppless::execution