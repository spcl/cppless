#pragma once

#include <type_traits>

#include <cppless/graph/graph.hpp>
#include <cppless/utils/apply.hpp>
#include <cppless/utils/tuple.hpp>

#include "cppless/detail/deduction.hpp"

namespace cppless::execution
{

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

template<int I, class Executor, class SenderType, class InputTask>
auto connect(std::shared_ptr<SenderType>& output,
             std::shared_ptr<graph::task_node<Executor, InputTask>>& input)
{
  input->increment_dependency_count();
  auto input_slot = input->template slot<I>();
  output->add_successor(input_slot);
}

template<class Executor, class SenderType, class InputTask>
auto connect_empty(
    std::shared_ptr<SenderType>& output,
    std::shared_ptr<graph::task_node<Executor, InputTask>>& input)
{
  input->increment_dependency_count();
  auto input_slot = input->create_empty_slot();
  output->add_successor(input_slot);
}

template<class Executor>
auto schedule(graph::builder<Executor> executor)
{
  auto node = executor.create_source_node();
  return node;
}

template<int I,
         class Executor,
         class InputTask,
         class FirstSenderType,
         class... RestSenderTypes>
auto then_connect(
    std::shared_ptr<graph::task_node<Executor, InputTask>> input_node,
    std::shared_ptr<FirstSenderType> head,
    std::shared_ptr<RestSenderTypes>... tail) -> void
{
  if constexpr (std::is_void<typename FirstSenderType::sending_type>()) {
    connect_empty(head, input_node);
    if constexpr (sizeof...(tail) > 0) {
      then_connect<I, Executor, InputTask, RestSenderTypes...>(input_node,
                                                               tail...);
    }
  } else {
    connect<I>(head, input_node);
    if constexpr (sizeof...(tail) > 0) {
      then_connect<I + 1, Executor, InputTask, RestSenderTypes...>(input_node,
                                                                   tail...);
    }
  }
}

template<class CustomTaskType = void, class... Args>
auto then(Args... args)
{
  return tail_apply(
      []<class InputTask, class FirstSenderType, class... RestSenderTypes>(
          InputTask input_thing,
          std::shared_ptr<FirstSenderType> first_sender,
          RestSenderTypes... rest_senders) {
        using executor = typename FirstSenderType::executor;
        typename std::conditional<std::is_void<CustomTaskType>::value,
                                  typename executor::task,
                                  CustomTaskType>::type::sendable
            input_task(input_thing);

        auto builder = first_sender->builder();
        std::shared_ptr<graph::task_node<executor, decltype(input_task)>>
            input_node = builder->create_node(input_task);
        then_connect<0, executor>(input_node, first_sender, rest_senders...);
        return input_node;
      },
      std::forward<Args>(args)...);
}

}  // namespace cppless::execution