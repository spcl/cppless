#pragma once

#include <cppless/graph/graph.hpp>

namespace cppless::executor
{
class noop_executor
{
public:
  class node_core : public graph::basic_node_core<noop_executor>
  {
  };

  template<class Arg>
  class receiver_slot : public graph::basic_receiver_slot<noop_executor, Arg>
  {
  };

  template<class... Args>
  class receiver : public graph::basic_receiver<noop_executor, Args...>
  {
  };

  template<class Res>
  class sender : public graph::basic_sender<noop_executor, Res>
  {
  };

  template<>
  class sender<void> : public graph::basic_sender<noop_executor, void>
  {
  };

  template<class Task>
  class task_node : public graph::basic_task_node<noop_executor, Task>
  {
  };

  class source_node : public graph::basic_source_node<noop_executor>
  {
  };
};
}  // namespace cppless::executor