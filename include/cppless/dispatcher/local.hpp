#include <array>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>

#include <cereal/cereal.hpp>
#include <cereal/types/tuple.hpp>
#include <cppless/cconv/cereal.hpp>
#include <cppless/dispatcher/sendable.hpp>
#include <cppless/utils/fdstream.hpp>
#include <nlohmann/json.hpp>
#include <unistd.h>

namespace cppless::dispatchers
{
using json = nlohmann::json;
namespace fs = std::filesystem;

class runtime_cppless_entry_data
{
public:
  std::string original_function_name;
  std::string filename;
  std::string user_meta;
};

inline void to_json(json& j, const runtime_cppless_entry_data& p)
{
  j = json {{"original_function_name", p.original_function_name},
            {"filename", p.filename},
            {"user_meta", p.user_meta}};
}

inline void from_json(const json& j, runtime_cppless_entry_data& p)
{
  j.at("original_function_name").get_to(p.original_function_name);
  j.at("filename").get_to(p.filename);
  j.at("user_meta").get_to(p.user_meta);
}

class runtime_cppless_meta
{
public:
  std::vector<runtime_cppless_entry_data> entry_points;
};

inline void to_json(json& j, const runtime_cppless_meta& p)
{
  j = json {{"entry_points", p.entry_points}};
}

inline void from_json(const json& j, runtime_cppless_meta& p)
{
  j.at("entry_points").get_to(p.entry_points);
}

class local_dispatcher
{
public:
  explicit local_dispatcher() = default;
  explicit local_dispatcher(std::string base_path)
      : m_base_path(std::move(base_path))
  {
    // Read the meta file
    auto meta_path = fs::path(m_base_path).replace_extension(".json");
    if (!fs::exists(meta_path)) {
      throw std::runtime_error("No meta file found at " + meta_path.string());
    }
    std::ifstream meta_file(meta_path);
    if (!meta_file.is_open()) {
      throw std::runtime_error("Could not open meta file at "
                               + meta_path.string());
    }
    json meta;
    meta_file >> meta;
    meta_file.close();

    // Deserialize the entry points
    runtime_cppless_meta runtime_meta = meta.get<runtime_cppless_meta>();

    // Build function map from user_meta -> filename
    for (auto& entry : runtime_meta.entry_points) {
      m_function_map[entry.user_meta] = entry.filename;
    }
  }

  template<class In, class Out>
  auto execute(const std::string& path, In& input) -> std::future<Out>
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
    cereal::JSONOutputArchive oar(parent_to_child_stream);

    oar(input);
    parent_to_child_stream << std::endl;
    // Close

    auto f = [=]
    {
      close(parent_write_fd);
      // This close is sus
      // Read the result
      cppless::fdistream child_to_parent_stream(parent_read_fd);

      cereal::JSONInputArchive iar(child_to_parent_stream);
      Out result;
      iar(result);
      // Close
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
      close(parent_read_fd);

      return result;
    };
    return std::async(std::launch::async, f);
  }

  template<class Res, class... Args>
  auto make_call(
      cppless::sendable_task<cereal::JSONOutputArchive, Res, Args...>& task,
      Args... args) -> std::future<Res>
  {
    // Get the function name
    auto function_name = task.identifier();

    std::tuple<Args...> s_args {args...};
    task_data<cppless::sendable_task<cereal::JSONOutputArchive, Res, Args...>,
              Args...>
        data(task, s_args);
    std::string location = m_function_map[function_name];
    return execute<
        task_data<
            cppless::sendable_task<cereal::JSONOutputArchive, Res, Args...>,
            Args...>,
        Res>(location, data);
  }

private:
  std::string m_base_path;
  std::map<std::string, std::string> m_function_map;
};

}  // namespace cppless::dispatchers