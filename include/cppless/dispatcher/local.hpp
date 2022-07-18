#pragma once

#include <array>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>

#include <cereal/cereal.hpp>
#include <cereal/types/tuple.hpp>
#include <cppless/detail/deduction.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/dispatcher/sendable.hpp>
#include <cppless/utils/cereal.hpp>
#include <cppless/utils/tracing.hpp>
#include <cppless/utils/uninitialized.hpp>
#include <nlohmann/json.hpp>

namespace cppless
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

/**
 * @brief A dispatcher which runs functions locally in new processes
 *
 * @tparam InputArchive - The cereal archive which should be used to unmarshal
 * the data send from the host to the task invocation and for the result send
 * from the task invocation to the host.
 * @tparam OutputArchive - The corresponding output archive, used to marshal
 * data.
 */
template<class InputArchive, class OutputArchive>
class local_dispatcher
{
public:
  using default_config = void;
  using request_input_archive = InputArchive;
  using request_output_archive = OutputArchive;
  using response_input_archive = InputArchive;
  using response_output_archive = OutputArchive;

  template<class Config>
  struct meta_serializer
  {
    template<unsigned int N>
    constexpr static auto serialize(basic_fixed_string<char, N> identifier)
    {
      return identifier;
    }

    static auto identifier(const std::string& identifier) -> std::string
    {
      return identifier;
    }
  };

  /**
   * @brief Construct a new local dispatcher object
   *
   * @param base_path The path at which the metadata for alternative entry
   * points can be found. The extension is changed to `.json`, thus this should
   * normally be `argv[0]`, assuming that `argv[0]´ contains the executable
   * name.
   */
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

    // Deserialize the entry point meta data
    runtime_cppless_meta runtime_meta = meta.get<runtime_cppless_meta>();

    // Build a map from `user_meta` -> `filename`
    for (auto& entry : runtime_meta.entry_points) {
      m_function_map[entry.user_meta] = entry.filename;
    }
  }

  template<class Lambda, class Receivable, class Res, class... Args>
  static auto main(int /*argc*/, char* /*argv*/[]) -> int  // NOLINT
  {
    using uninitialized_recv = cppless::uninitialized_data<Receivable>;
    input_archive iar(std::cin);
    uninitialized_recv u;
    std::tuple<Args...> s_args;
    // task_data takes both of its constructor arguments by reference, thus
    // deserializing into `t_data` will populate the context into `m_self` and
    // the arguments into `s_args`.
    task_data<Receivable, Args...> t_data {u.m_self, s_args};
    iar(t_data);

    Res res = std::apply(u.m_self, s_args);
    {
      output_archive oar(std::cout);
      oar(res);
    }

    return 0;
  }

  class instance
  {
  public:
    using id_type = int;
    using dispatcher_type = local_dispatcher;

    explicit instance(
        local_dispatcher<input_archive, output_archive>& dispatcher)
        : m_dispatcher(dispatcher)
    {
    }

    // Delete copy constructor
    instance(const instance&) = delete;
    // Delete copy assignment
    auto operator=(const instance&) -> instance& = delete;

    // Move constructor
    instance(instance&& other) noexcept
        : m_next_id(other.m_next_id)
        , m_mutex(std::move(other.m_mutex))
        , m_cv(std::move(other.m_cv))
        , m_finished(std::move(other.m_finished))
        , m_threads(std::move(other.m_threads))
        , m_dispatcher(other.m_dispatcher)
    {
    }
    // Move assignment
    auto operator=(instance&& other) noexcept -> instance&
    {
      m_next_id = other.m_next_id;
      m_mutex = std::move(other.m_mutex);
      m_cv = std::move(other.m_cv);
      m_finished = std::move(other.m_finished);
      m_threads = std::move(other.m_threads);
      m_dispatcher = other.m_dispatcher;
      return *this;
    }

    ~instance()
    {
      for (auto& thread : m_threads) {
        thread.join();
      }
    }

    /**
     * @brief Dispatches a task for execution
     *
     * @tparam Res - The return type of the task
     * @tparam Args - The types of the arguments of the task
     * @param t - The task to be dispatched
     * @param result_future - The future to which the result of the task should
     * be written to
     * @param args - The arguments with which the invocation should occur
     * @return int - The id of the invocation
     */
    template<class TaskType>
    auto dispatch_impl(TaskType& t,
                       typename TaskType::res& result_target,
                       typename TaskType::args args,
                       std::optional<tracing_span_ref> span = std::nullopt)
        -> int
    {
      // Get the function name
      auto function_name = t.identifier();

      task_data data {t, args};
      std::string location = m_dispatcher.m_function_map[function_name];
      int id = m_next_id++;
      auto cb = [this, &result_target, id](const typename TaskType::res& result)
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        result_target = result;
        m_finished.push_back(id);
        m_cv.notify_one();
      };

      {
        std::scoped_lock lock(m_mutex);
        std::thread thread =
            execute<InputArchive, OutputArchive, typename TaskType::res>(
                location, data, std::move(cb));
        m_threads.push_back(std::move(thread));
      }
      return id;
    }

    /**
     * @brief Waits until one arbitrary dispatched task invocation has finished
     * executing. This will block when there is no finished task at
     * the moment.
     *
     * Each dispatched task invocation, identified by the id of its associated
     * `future` is returned exactly once by the `wait_one()` method.
     * @return int - The `id` of the finished task.
     */
    auto wait_one() -> int
    {
      std::unique_lock lock(m_mutex);
      if (!m_finished.empty()) {
        int finished = m_finished.back();
        m_finished.pop_back();
        return finished;
      }
      m_cv.wait(lock, [this] { return !m_finished.empty(); });
      int finished = m_finished.back();
      m_finished.pop_back();
      return finished;
    }

  private:
    int m_next_id = 0;
    /**
     * Acts as a mutual exclusion guard for `m_finished`
     */
    std::mutex m_mutex;
    /**
     * Used as a chanel for communicating changes to `m_finished`
     */
    std::condition_variable m_cv;
    /**
     * The ids of task invocations which finished. `m_cv` should be
     * notified when changes were made. A lock on `m_mutex` is required.
     */
    std::vector<int> m_finished;
    /**
     * List of threads spawned by this instance. The destructor will ensure that
     * all threads are joined when the instance goes out of scope.
     */
    std::vector<std::thread> m_threads;
    local_dispatcher<input_archive, output_archive>& m_dispatcher;
  };

  /**
   * @brief Create a instance of the local dispatcher. The local dispatcher
   * instance will create a new process for each invocation and waits for the
   * process to finish on another thread. The lifetime of the dispatcher must
   * outlife the instance, as it might hold a reference to the dispatcher from
   * which it was created.
   *
   * @return A fresh instance
   */
  auto create_instance() -> instance
  {
    return instance(*this);
  }

private:
  std::string m_base_path;
  std::map<std::string, std::string> m_function_map;
};

}  // namespace cppless