#pragma once

#include <array>
#include <condition_variable>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <boost/beast/core/detail/base64.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cppless/detail/deduction.hpp>
#include <cppless/utils/fdstream.hpp>
#include <cppless/utils/tracing.hpp>
#include <sys/wait.h>
#include <unistd.h>

#include "cppless/dispatcher/sendable.hpp"

namespace cppless
{

/**
 * @brief Represents a value which will be set in the future
 *
 * @tparam Res The type of the value
 */
template<class Res>
class future
{
public:
  future() = default;

  // Delete copy constructor
  future(const future&) = delete;
  // Delete copy assignment operator
  auto operator=(const future&) -> future& = delete;

  // Delete move constructor
  future(future&&) = delete;

  auto operator=(future&&) = delete;

  ~future() = default;

  auto set_value(const Res& res) -> void
  {
    m_res = res;
  }

  auto value() -> Res&
  {
    return m_res.value();
  }

  auto value() const -> const Res&
  {
    return m_res.value();
  }

private:
  std::optional<Res> m_res;
};

/**
 * @brief A future which can be copied
 *
 * @tparam Res
 */
template<class Res>
class shared_future
{
public:
  shared_future()
      : m_future(std::make_shared<future<Res>>())
  {
  }

  auto set_value(const Res& res) -> void
  {
    m_future->set_value(res);
  }

  auto value() -> Res&
  {
    return m_future->value();
  }

  auto value() const -> const Res&
  {
    return m_future->value();
  }

private:
  std::shared_ptr<future<Res>> m_future;
};

template<class Res>
class identified_shared_future
{
public:
  identified_shared_future(int id, shared_future<Res>& f)
      : m_id(id)
      , m_f(f)
  {
  }

  [[nodiscard]] auto id() const -> int
  {
    return m_id;
  }

  auto value() -> Res&
  {
    return m_f.value();
  }

  auto value() const -> const Res&
  {
    return m_f.value();
  }

private:
  int m_id;
  shared_future<Res> m_f;
};

template<class InputArchive,
         class OutputArchive,
         class Out,
         class In,
         class Callback>
auto execute(const std::string& path, In input, Callback callback)
    -> std::thread
{
  std::array<int, 2> parent_to_child {};
  int parent_to_child_pipe_res = pipe(parent_to_child.begin());
  if (parent_to_child_pipe_res != 0) {
    throw std::runtime_error("Could not create parent->child pipe");
  }
  int child_read_fd = parent_to_child[0];
  int parent_write_fd = parent_to_child[1];

  std::array<int, 2> child_to_parent {};
  int child_to_parent_pipe_res = pipe(child_to_parent.begin());
  if (child_to_parent_pipe_res != 0) {
    throw std::runtime_error("Could not create child->parent pipe");
  }
  int parent_read_fd = child_to_parent[0];
  int child_write_fd = child_to_parent[1];

  // Good old fork
  auto child_pid = fork();
  if (child_pid == -1) {
    throw std::runtime_error("Failed to fork");
  }

  if (child_pid == 0) {
    // child process

    // Close pfd[0] in child process;
    close(parent_read_fd);
    // Connect pfd[1] to STDOUT_FILENO using dup2();
    dup2(child_write_fd, STDOUT_FILENO);
    // Close pfd[1];
    close(child_write_fd);

    // Do the same thing for STDIN
    close(parent_write_fd);
    dup2(child_read_fd, STDIN_FILENO);
    close(child_read_fd);

    // Determine location of executable
    std::array<char*, 1> argv = {nullptr};
    execve(path.c_str(), argv.begin(), nullptr);
    throw std::runtime_error("Failed to execve");
  }

  // parent process

  // Closed unused ends of pipes
  close(child_read_fd);
  close(child_write_fd);

  cppless::fdostream parent_to_child_stream(parent_write_fd);
  {
    OutputArchive oar(parent_to_child_stream);
    oar(input);
  }
  parent_to_child_stream << std::flush;
  // Close the write end of the pipe
  close(parent_write_fd);

  auto wait_for_result = [=]()
  {
    int status = 0;
    int wait_pid = waitpid(child_pid, &status, 0);
    if (wait_pid == -1) {
      throw std::runtime_error("Failed to wait for child process");
    }
    if (!WIFEXITED(status)) {
      throw std::runtime_error("Child process did not exit normally");
    }
    if (WEXITSTATUS(status) != 0) {
      throw std::runtime_error("Child process exited with non-zero status");
    }

    // Read the result
    cppless::fdistream child_to_parent_stream(parent_read_fd);

    InputArchive iar(child_to_parent_stream);
    //  Out result;
    std::tuple<Out, std::string> result;
    iar(result);
    // Close

    close(parent_read_fd);
    callback(std::get<0>(result), std::get<1>(result));
  };
  std::thread t(wait_for_result);
  // return thread and future
  return t;
}

class json_structured_archive
{
public:
  using input_archive = cereal::JSONInputArchive;
  using output_archive = cereal::JSONOutputArchive;

  template<class T>
  static inline auto serialize(T t) -> std::string
  {
    std::stringstream ss;
    {
      output_archive oar(ss, output_archive::Options::NoIndent());
      oar(t);
    }
    return ss.str();
  }

  template<class T>
  static inline auto deserialize(const std::string& s, T& t)
  {
    std::stringstream ss(s);
    {
      input_archive iar(ss);
      iar(t);
    }
  }
};

class json_binary_archive
{
public:
  using input_archive = cereal::BinaryInputArchive;
  using output_archive = cereal::BinaryOutputArchive;

  template<class T>
  static inline auto serialize(T t) -> std::string
  {
    // Binary stream
    std::stringstream bs;
    {
      output_archive oar(bs);
      oar(t);
    }
    std::string s = bs.str();
    unsigned int size = s.size();
    auto encoded_size = boost::beast::detail::base64::encoded_size(size);

    std::string encoded;
    encoded.resize(encoded_size + 2);
    encoded[0] = '"';
    boost::beast::detail::base64::encode(encoded.data() + 1, s.data(), size);
    encoded[encoded_size + 1] = '"';
    return encoded;
  }

  template<class T>
  static inline auto deserialize(const std::string& s, T& t) -> void
  {
    unsigned int size = s.size();
    auto decoded_size = boost::beast::detail::base64::decoded_size(size - 2);
    std::string decoded;
    decoded.resize(decoded_size);
    boost::beast::detail::base64::decode(
        decoded.data(), s.data() + 1, size - 2);

    std::stringstream os(decoded);
    {
      input_archive iar(os);
      iar(t);
    }
  }
};

class binary_archive
{
public:
  using input_archive = cereal::BinaryInputArchive;
  using output_archive = cereal::BinaryOutputArchive;

  template<class T>
  static inline auto serialize(T t) -> std::string
  {
    // Binary stream
    std::stringstream bs;
    {
      output_archive oar(bs);
      oar(t);
    }
    return bs.str();
  }

  template<class T>
  static inline auto deserialize(const std::string& s, T& t) -> void
  {
    std::stringstream os(s);
    {
      input_archive iar(os);
      iar(t);
    }
  }
};

template<class Task, class DispatcherInstance>
inline auto dispatch(DispatcherInstance& instance,
                     Task& task,
                     typename Task::res& result_target,
                     typename Task::args args = {},
                     std::optional<tracing_span_ref> span = std::nullopt)
{
  return instance.dispatch_impl(task, result_target, args, span);
}

template<class Config,
         class Fn,
         class DispatcherInstance,
         class FnType =
             typename detail::deduce_function<decltype(&Fn::operator())>::type>
inline auto dispatch(DispatcherInstance& instance,
                     Fn& fn,
                     typename detail::function_res<FnType>::type& result_target,
                     typename detail::function_args<FnType>::type args = {},
                     std::optional<tracing_span_ref> span = std::nullopt)
{
  auto task = lambda_task_factory<typename DispatcherInstance::dispatcher_type,
                                  Config>::create(fn);
  return instance.dispatch_impl(task, result_target, args, span);
}

template<class Fn,
         class DispatcherInstance,
         class FnType =
             typename detail::deduce_function<decltype(&Fn::operator())>::type>
inline auto dispatch(DispatcherInstance& instance,
                     Fn& fn,
                     typename detail::function_res<FnType>::type& result_target,
                     typename detail::function_args<FnType>::type args = {},
                     std::optional<tracing_span_ref> span = std::nullopt)
{
  return dispatch<typename DispatcherInstance::dispatcher_type::default_config>(
      instance, fn, result_target, args, span);
}

template<class DispatcherInstance>
inline auto wait(DispatcherInstance& instance, int n)
{
  for (int i = 0; i < n; i++) {
    instance.wait_one();
  }
}

}  // namespace cppless
