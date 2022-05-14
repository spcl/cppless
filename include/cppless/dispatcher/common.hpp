#pragma once

#include <array>
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <cppless/utils/fdstream.hpp>
#include <sys/wait.h>
#include <unistd.h>

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

  auto set_value(const Res& res) -> void
  {
    m_res = res;
  }

  auto get_value() -> Res&
  {
    return m_res.value();
  }

  auto get_value() const -> const Res&
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

  // Copy constructor
  shared_future(const shared_future& other)
      : m_future(other.m_future)
  {
  }
  // Copy assignment operator
  auto operator=(const shared_future& other) -> shared_future&
  {
    if (this != &other) {
      m_future = other.m_future;
    }
    return *this;
  }

  auto set_value(const Res& res) -> void
  {
    m_future->set_value(res);
  }

  auto get_value() -> Res&
  {
    return m_future->get_value();
  }

  auto get_value() const -> const Res&
  {
    return m_future->get_value();
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

  [[nodiscard]] auto get_id() const -> int
  {
    return m_id;
  }

  auto get_value() -> Res&
  {
    return m_f.get_value();
  }

  auto get_value() const -> const Res&
  {
    return m_f.get_value();
  }

private:
  int m_id;
  shared_future<Res> m_f;
};

template<class InputArchive,
         class OutputArchive,
         class In,
         class Out,
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
    Out result;
    iar(result);
    // Close

    close(parent_read_fd);
    callback(result);
  };
  std::thread t(wait_for_result);
  // return thread and future
  return t;
}
}  // namespace cppless