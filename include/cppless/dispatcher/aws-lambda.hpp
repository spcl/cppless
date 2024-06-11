#pragma once

#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <aws/lambda-runtime/runtime.h>
#include <boost/algorithm/hex.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/dispatcher/sendable.hpp>
#include <cppless/provider/aws/auth.hpp>
#include <cppless/provider/aws/lambda.hpp>
#include <cppless/utils/crypto/wrappers.hpp>
#include <cppless/utils/fixed_string.hpp>
#include <cppless/utils/fixed_string_serialization.hpp>
#include <cppless/utils/tracing.hpp>
#include <cppless/utils/uninitialized.hpp>
#include <nlohmann/json.hpp>

//#include "common.hpp"

#ifndef TARGET_NAME
#  define TARGET_NAME "cppless"  // NOLINT
#endif

namespace cppless
{

/**
 * @brief Types specific to the aws lambda dispatcher
 */
namespace aws
{

/**
 * @brief The default config of a an aws lambda task
 */
struct default_config
{
  constexpr static std::string_view description;
  constexpr static unsigned int memory = 1024;  // MB
  constexpr static unsigned int ephemeral_storage = 512;  // MB
  constexpr static unsigned int timeout = 10;  // seconds
};

template<class A, A Description>
struct with_description
{
  template<class Base>
  struct apply : public Base
  {
    constexpr static A description = Description;
  };
};

template<unsigned int Memory>
struct with_memory
{
  template<class Base>
  struct apply : public Base
  {
    constexpr static unsigned int memory = Memory;
  };
};

template<unsigned int EphemeralStorage>
struct with_ephemeral_storage
{
  template<class Base>
  struct apply : public Base
  {
    constexpr static unsigned int ephemeral_storage = EphemeralStorage;
  };
};

template<unsigned int Timeout>
class with_timeout
{
  template<class Base>
  class apply : public Base
  {
    constexpr static unsigned int timeout = Timeout;
  };
};

template<class... Modifiers>
class config;

template<class Modifier, class... Modifiers>
class config<Modifier, Modifiers...>
    : public Modifier::template apply<config<Modifiers...>>
{
};

template<>
class config<> : public default_config
{
};

}  // namespace aws

template<class T>
auto task_function_name(const T& task) -> std::string
{
  auto raw_function_name = task.identifier();

  std::string function_name = TARGET_NAME;
  function_name += "-";

  evp_md_ctx ctx;
  ctx.update(raw_function_name);
  auto binary_digest = ctx.final();

  std::string function_hex;
  boost::algorithm::hex_lower(binary_digest, std::back_inserter(function_hex));

  // First 8 chars
  function_name += function_hex.substr(0, 8);

  return function_name;
}

template<class RequestArchive, class ResponseArchive>
class base_aws_lambda_dispatcher;

template<class RequestArchive = json_binary_archive,
         class ResponseArchive = json_binary_archive>
class aws_lambda_nghttp2_dispatcher;

template<class RequestArchive, class ResponseArchive>
class aws_lambda_nghttp2_dispatcher_instance
{
public:
  using id_type = int;
  using dispatcher_type =
      aws_lambda_nghttp2_dispatcher<RequestArchive, ResponseArchive>;

  static auto create_sessions(boost::asio::io_service& io_service,
                              const cppless::aws::lambda::client& lambda_client,
                              int num_conns)
      -> std::vector<nghttp2::asio_http2::client::session>
  {
    boost::system::error_code ec;

    boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
    tls.set_default_verify_paths();
    nghttp2::asio_http2::client::configure_tls_context(ec, tls);

    std::vector<nghttp2::asio_http2::client::session> sessions;
    sessions.reserve(num_conns);
    for (int i = 0; i < num_conns; ++i) {
      sessions.emplace_back(io_service, tls, lambda_client.hostname(), "443");
    }

    return sessions;
  }

  constexpr static int num_conns = 16;
  explicit aws_lambda_nghttp2_dispatcher_instance(
      base_aws_lambda_dispatcher<RequestArchive, ResponseArchive>& dispatcher)
      : m_lambda_client(dispatcher.lambda_client())
      , m_key(dispatcher.key())
      , m_sessions(create_sessions(m_io_service, m_lambda_client, num_conns))
      , m_dispatcher(dispatcher)
  {
    int connected = 0;
    bool error = false;

    for (auto& session : m_sessions) {
      session.on_connect([&connected](const auto&) { connected++; });
      session.on_error(
          [&error](const boost::system::error_code& ec)
          {
            error = true;
            std::cerr << "Error: " << ec.message() << std::endl;
          });
    }
    while (connected < num_conns && !error) {
      m_io_service.run_one();
    }
  }

  // Destructor
  ~aws_lambda_nghttp2_dispatcher_instance()
  {
    for (auto& session : m_sessions) {
      session.shutdown();
    }
    m_io_service.run();
  }

  // Delete copy constructor
  aws_lambda_nghttp2_dispatcher_instance(
      const aws_lambda_nghttp2_dispatcher_instance&) = delete;
  // Delete copy assignment
  auto operator=(const aws_lambda_nghttp2_dispatcher_instance&)
      -> aws_lambda_nghttp2_dispatcher_instance& = delete;

  // Move constructor
  aws_lambda_nghttp2_dispatcher_instance(
      aws_lambda_nghttp2_dispatcher_instance&& other) noexcept
      : m_lambda_client(std::move(other.m_lambda_client))
      , m_key(std::move(other.m_key))
      , m_sessions(std::move(other.m_sessions))
      , m_requests(std::move(other.m_requests))
      , m_spans(std::move(other.m_spans))
      , m_finished(std::move(other.m_finished))
      , m_started(std::move(other.m_started))
      , m_completed(other.m_completed)
      , m_dispatcher(other.m_dispatcher)
  {
  }

  // Delete move assignment
  auto operator=(aws_lambda_nghttp2_dispatcher_instance&& other) noexcept
      -> aws_lambda_nghttp2_dispatcher_instance& = delete;

  template<class TaskType>
  auto dispatch_impl(TaskType& t,
                     typename TaskType::res& result_target,
                     typename TaskType::args args,
                     std::optional<tracing_span_ref> span = std::nullopt) -> int
  {
    std::string payload;

    {
      scoped_tracing_span serialization_span(span, "serialization");

      task_data data {t, args};
      payload = RequestArchive::serialize(data);
    }

    //std::cout << "payload size: " << payload.size() << std::endl;

    int id = m_started++;
    auto req =
        std::make_unique<cppless::aws::lambda::nghttp2_invocation_request>(
            task_function_name(t), "$LATEST", payload);
    m_requests.push_back(std::move(req));
    m_spans.push_back(span);

    auto submit_req =
        [this](
            std::unique_ptr<cppless::aws::lambda::nghttp2_invocation_request>&
                req,
            std::optional<tracing_span_ref> span)
    {
      auto& session = m_sessions[m_next_session];
      m_next_session = (m_next_session + 1) % num_conns;
      req->submit(session, m_lambda_client, m_key, span);
    };

    auto cb = [this, id, &result_target, span](
                  const cppless::aws::lambda::invocation_response& res) mutable
    {
      scoped_tracing_span deserialization_span(span, "deserialization");

      std::tuple<typename TaskType::res&, execution_statistics> result{result_target, execution_statistics{"", false}};
      ResponseArchive::deserialize(res.body, result);

      m_finished[id] = std::get<1>(result);
      m_completed++;
    };

    auto err_cb = [this, submit_req, id](
                      const cppless::aws::lambda::invocation_error& err)
    {
      if (std::holds_alternative<
              cppless::aws::lambda::invocation_error_too_many_requests>(err))
      {
        submit_req(m_requests[id], m_spans[id]);
      } else {
        std::cerr << "Error." << std::endl;
      }
    };

    auto& req_ref = m_requests.back();
    req_ref->on_result(cb);
    req_ref->on_error(err_cb);

    submit_req(req_ref, span);

    return id;
  }

  auto wait_one() -> std::tuple<int, execution_statistics>
  {
    while (m_finished.empty()) {
      m_io_service.run_one();
    }
    auto it = m_finished.begin();
    auto ret = *it;
    m_finished.erase(it);
    return ret;
  }

private:
  boost::asio::io_service m_io_service;
  cppless::aws::lambda::client m_lambda_client;
  cppless::aws::aws_v4_derived_key m_key;
  int m_next_session = 0;
  std::vector<nghttp2::asio_http2::client::session> m_sessions;

  std::vector<std::unique_ptr<cppless::aws::lambda::nghttp2_invocation_request>>
      m_requests;
  std::vector<std::optional<tracing_span_ref>> m_spans;
  std::unordered_map<int, execution_statistics> m_finished;

  std::vector<int> m_retry_queue;
  int m_started = 0;
  int m_completed = 0;

  base_aws_lambda_dispatcher<RequestArchive, ResponseArchive>& m_dispatcher;
};

template<class RequestArchive = json_binary_archive,
         class ResponseArchive = binary_archive>
class aws_lambda_beast_dispatcher;

template<class RequestArchive, class ResponseArchive>
class aws_lambda_beast_dispatcher_instance
{
public:
  using id_type = int;
  using dispatcher_type =
      aws_lambda_beast_dispatcher<RequestArchive, ResponseArchive>;

  explicit aws_lambda_beast_dispatcher_instance(
      base_aws_lambda_dispatcher<RequestArchive, ResponseArchive>& dispatcher)
      : m_tls(boost::asio::ssl::context::tlsv12_client)
      , m_lambda_client(dispatcher.lambda_client())
      , m_resolver(m_ioc)
      , m_key(dispatcher.key())
      , m_dispatcher(dispatcher)
  {
    m_resolver.run(m_lambda_client.hostname(), "443");
    m_tls.set_default_verify_paths();
  }

  ~aws_lambda_beast_dispatcher_instance() { m_ioc.run(); }

  // Delete copy constructor
  aws_lambda_beast_dispatcher_instance(
      const aws_lambda_beast_dispatcher_instance&) = delete;
  // Delete copy assignment
  auto operator=(const aws_lambda_beast_dispatcher_instance&)
      -> aws_lambda_beast_dispatcher_instance& = delete;

  // Move constructor
  aws_lambda_beast_dispatcher_instance(
      aws_lambda_beast_dispatcher_instance&& other) noexcept
      : m_ioc(std::move(other.m_ioc))
      , m_tls(std::move(other.m_tls))
      , m_lambda_client(std::move(other.m_lambda_client))
      , m_resolver(std::move(other.m_resolver))
      , m_key(std::move(other.m_key))
      , m_requests(std::move(other.m_requests))
      , m_finished(std::move(other.m_finished))
      , m_next_id(other.m_next_id)
      , m_dispatcher(other.m_dispatcher)
  {
  }

  // Delete move assignment
  auto operator=(aws_lambda_beast_dispatcher_instance&& other) noexcept
      -> aws_lambda_beast_dispatcher_instance& = delete;

  template<class Task>
  auto dispatch_impl(Task& t,
                     typename Task::res& result_target,
                     typename Task::args args,
                     std::optional<tracing_span_ref> span = std::nullopt) -> int
  {
    int id = m_next_id++;

    std::string payload;

    {
      scoped_tracing_span serialization_span(span, "serialization");

      task_data data {t, args};
      payload = RequestArchive::serialize(data);
    }

    std::shared_ptr<cppless::aws::lambda::beast_invocation_request> req =
        std::make_shared<cppless::aws::lambda::beast_invocation_request>(
            task_function_name(t), "$LATEST", payload);

    m_requests.push_back(req);

    req->on_result(
        [id, &result_target, this, span](
            const cppless::aws::lambda::invocation_response& res) mutable
        {
          scoped_tracing_span deserialization_span(span, "deserialization");

          std::tuple<typename Task::res&, execution_statistics> result {
              result_target, execution_statistics {"", false}};
          ResponseArchive::deserialize(res.body, result);

          m_finished[id] = std::get<1>(result);
        });
    req->submit(m_resolver, m_ioc, m_tls, m_lambda_client, m_key, span);

    return id;
  }

  auto wait_one() -> std::tuple<int, execution_statistics>
  {
    while (m_finished.empty()) {
      m_ioc.run_one();
    }
    auto it = m_finished.begin();
    auto ret = *it;
    m_finished.erase(it);
    return ret;
  }

private:
  boost::asio::io_context m_ioc;
  boost::asio::ssl::context m_tls;
  cppless::aws::lambda::client m_lambda_client;
  beast::resolver_session m_resolver;
  cppless::aws::aws_v4_derived_key m_key;

  std::vector<std::shared_ptr<cppless::aws::lambda::beast_invocation_request>>
      m_requests;
  std::unordered_map<int, execution_statistics> m_finished;

  int m_next_id = 0;

  base_aws_lambda_dispatcher<RequestArchive, ResponseArchive>& m_dispatcher;
};

template<class RequestArchive, class ResponseArchive>
class base_aws_lambda_dispatcher
{
public:
  using default_config = aws::config<>;
  using request_input_archive = typename RequestArchive::input_archive;
  using request_output_archive = typename RequestArchive::output_archive;
  using response_input_archive = typename ResponseArchive::input_archive;
  using response_output_archive = typename ResponseArchive::output_archive;

  template<class... Modifiers>
  using task =
      cppless::task<base_aws_lambda_dispatcher, aws::config<Modifiers...>>;

  explicit base_aws_lambda_dispatcher(cppless::aws::lambda::client client,
                                      cppless::aws::aws_v4_derived_key key)
      : m_client(std::move(client))
      , m_key(std::move(key))
  {
  }

  template<class Config>
  struct meta_serializer
  {
    template<unsigned int N>
    constexpr static auto serialize(basic_fixed_string<char, N> identifier)
    {
      using namespace cppless;  // NOLINT
      return cppless::encode_base64(cppless::serialize(
          map(kv("ephemeral_storage", Config::ephemeral_storage),
              kv("memory", Config::memory),
              kv("timeout", Config::timeout),
              kv("identifier", identifier))));
    }

    static auto identifier(const std::string& identifier) -> std::string
    {
      std::stringstream ss;
      ss << identifier << "#" << Config::ephemeral_storage << "#"
         << Config::memory << "#" << Config::timeout;
      return ss.str();
    }
  };

  template<class Receivable, class Res, class... Args>
  static auto main(int /*argc*/, char* /*argv*/[]) -> int
  {
    bool is_cold = true;

    using invocation_response = ::aws::lambda_runtime::invocation_response;
    using invocation_request = ::aws::lambda_runtime::invocation_request;

    using uninitialized_recv = cppless::uninitialized_data<Receivable>;
    ::aws::lambda_runtime::run_handler(
        [&is_cold](invocation_request const& request)
        {
          auto start = std::chrono::high_resolution_clock::now();
          uninitialized_recv u;
          std::tuple<Args...> s_args;
          // task_data takes both of its constructor arguments by reference,
          // thus deserializing into `t_data` will populate the context into
          // `m_self` and the arguments into `s_args`.
          task_data<Receivable, Args...> t_data {u.m_self, s_args};
          RequestArchive::deserialize(request.payload, t_data);
          auto end = std::chrono::high_resolution_clock::now();

          auto start2 = std::chrono::high_resolution_clock::now();
          std::tuple<Res, execution_statistics> res;
          std::get<0>(res) = std::apply(u.m_self, s_args);
          auto end2 = std::chrono::high_resolution_clock::now();
          std::get<1>(res) = {request.request_id, is_cold};
          if (is_cold)
            is_cold = false;

          auto start3 = std::chrono::high_resolution_clock::now();
          auto serialized_res = ResponseArchive::serialize(res);
          auto end3 = std::chrono::high_resolution_clock::now();

          std::cerr << "TRACE deser " << std::chrono::duration_cast<std::chrono::microseconds>(end-start).count() << std::endl;
          std::cerr << "TRACE compute " << std::chrono::duration_cast<std::chrono::microseconds>(end2-start2).count() << std::endl;
          std::cerr << "TRACE ser " << std::chrono::duration_cast<std::chrono::microseconds>(end3-start3).count() << std::endl;

          // serialized_res.
          return invocation_response::success(serialized_res,
                                              "application/json");
        });
    return 0;
  }

  [[nodiscard]] auto lambda_client() const noexcept { return m_client; }

  [[nodiscard]] auto key() const noexcept { return m_key; }

private:
  cppless::aws::lambda::client m_client;
  cppless::aws::aws_v4_derived_key m_key;
};

template<class Base>
class aws_lambda_env_dispatcher : public Base
{
  static auto from_env() -> Base
  {
    std::string aws_region;
    std::string aws_access_key_id;
    std::string aws_secret_access_key;

    auto* env_aws_region = std::getenv("AWS_REGION");
    if (env_aws_region != nullptr) {
      aws_region = env_aws_region;
    }
    auto* env_aws_access_key_id = std::getenv("AWS_ACCESS_KEY_ID");
    if (env_aws_access_key_id != nullptr) {
      aws_access_key_id = env_aws_access_key_id;
    }
    auto* env_aws_secret_access_key = std::getenv("AWS_SECRET_ACCESS_KEY");
    if (env_aws_secret_access_key != nullptr) {
      aws_secret_access_key = env_aws_secret_access_key;
    }

    if (aws_region.empty() || aws_access_key_id.empty()
        || aws_secret_access_key.empty())
    {
      auto config_path = std::filesystem::path(std::getenv("HOME")) / ".cppless"
          / "config.json";
      std::ifstream ifs(config_path);
      if (!ifs.fail()) {
        nlohmann::json jf = nlohmann::json::parse(ifs);
        std::string profile_name = "default";
        nlohmann::json config;
        for (auto& profile : jf["profiles"]) {
          if (profile["name"] == profile_name) {
            config = profile["config"];
          }
        }
        nlohmann::json aws_config = config["providers"]["aws"];
        if (aws_region.empty()) {
          aws_region = aws_config["region"];
        }
        if (aws_access_key_id.empty()) {
          aws_access_key_id = aws_config["access_key_id"];
        }
        if (aws_secret_access_key.empty()) {
          aws_secret_access_key = aws_config["secret_access_key"];
        }
      }
    }

    if (aws_region.empty() || aws_access_key_id.empty()
        || aws_secret_access_key.empty())
    {
      throw std::runtime_error("missing aws credentials");
    }

    cppless::aws::lambda::client lambda_client(aws_region);

    auto* session_token_env = std::getenv("AWS_SESSION_TOKEN");  // NOLINT
    std::optional<std::string> session_token;
    if (session_token_env != nullptr) {
      session_token = session_token_env;
    }
    auto key = lambda_client.create_derived_key(
        aws_access_key_id, aws_secret_access_key, session_token);
    Base aws {lambda_client, key};
    return aws;
  }

public:
  aws_lambda_env_dispatcher()
      : Base(from_env())
  {
  }

  template<class SerializationArchive>
  void serialize(SerializationArchive& ar)
  {
    ar();
  }
};

template<class RequestArchive, class ResponseArchive>
class aws_lambda_nghttp2_dispatcher
    : public base_aws_lambda_dispatcher<RequestArchive, ResponseArchive>
{
public:
  using instance =
      aws_lambda_nghttp2_dispatcher_instance<RequestArchive, ResponseArchive>;
  using base_aws_lambda_dispatcher<RequestArchive,
                                   ResponseArchive>::base_aws_lambda_dispatcher;
  auto create_instance()
      -> aws_lambda_nghttp2_dispatcher_instance<RequestArchive, ResponseArchive>
  {
    return aws_lambda_nghttp2_dispatcher_instance<RequestArchive,
                                                  ResponseArchive> {*this};
  }

  using from_env = aws_lambda_env_dispatcher<
      aws_lambda_nghttp2_dispatcher<RequestArchive, ResponseArchive>>;
};

template<class RequestArchive, class ResponseArchive>
class aws_lambda_beast_dispatcher
    : public base_aws_lambda_dispatcher<RequestArchive, ResponseArchive>
{
public:
  using instance =
      aws_lambda_nghttp2_dispatcher_instance<RequestArchive, ResponseArchive>;
  using base_aws_lambda_dispatcher<RequestArchive,
                                   ResponseArchive>::base_aws_lambda_dispatcher;
  auto create_instance()
      -> aws_lambda_beast_dispatcher_instance<RequestArchive, ResponseArchive>
  {
    return aws_lambda_beast_dispatcher_instance<RequestArchive,
                                                ResponseArchive> {*this};
  }

  using from_env = aws_lambda_env_dispatcher<
      aws_lambda_beast_dispatcher<RequestArchive, ResponseArchive>>;
};

using aws_dispatcher = aws_lambda_nghttp2_dispatcher<>::from_env;

}  // namespace cppless
