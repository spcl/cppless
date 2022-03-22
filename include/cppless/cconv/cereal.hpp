#pragma once

#include <tuple>

#include <cereal/cereal.hpp>

template<class Task, class... Args>
class task_data
{
public:
  task_data(Task& task, std::tuple<Args...>& args)
      : m_task(task)
      , m_args(args)
  {
  }
  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::make_nvp("context", m_task), cereal::make_nvp("args", m_args));
  }

private:
  Task& m_task;
  std::tuple<Args...>& m_args;
};