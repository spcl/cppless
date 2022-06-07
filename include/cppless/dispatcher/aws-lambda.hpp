#pragma once

#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <aws/lambda-runtime/runtime.h>
#include <boost/algorithm/hex.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
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

#ifndef TARGET_NAME
#  define TARGET_NAME "cppless"  // NOLINT
#endif

namespace cppless::dispatcher
{

namespace aws
{

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

template<class Archive>
class base_aws_lambda_dispatcher;

template<class Archive>
class aws_lambda_nghttp2_dispatcher_instance
{
public:
  using id_type = int;

  static auto create_session(boost::asio::io_service& io_service,
                             const cppless::aws::lambda::client& lambda_client)
      -> nghttp2::asio_http2::client::session
  {
    boost::system::error_code ec;

    boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
    tls.set_default_verify_paths();
    nghttp2::asio_http2::client::configure_tls_context(ec, tls);

    return {io_service, tls, lambda_client.hostname(), "443"};
  }

  explicit aws_lambda_nghttp2_dispatcher_instance(
      base_aws_lambda_dispatcher<Archive>& dispatcher)
      : m_lambda_client(dispatcher.lambda_client())
      , m_key(dispatcher.key())
      , m_session(create_session(m_io_service, m_lambda_client))
      , m_dispatcher(dispatcher)
  {
    bool connected = false;
    m_session.on_connect([&connected](const auto&) { connected = true; });
    m_session.on_error(
        [](const boost::system::error_code& ec)
        { std::cerr << "Error: " << ec.message() << std::endl; });

    while (!connected) {
      m_io_service.run_one();
    }
  }

  // Destructor
  ~aws_lambda_nghttp2_dispatcher_instance()
  {
    m_session.shutdown();
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
      , m_session(std::move(other.m_session))
      , m_requests(std::move(other.m_requests))
      , m_finished(std::move(other.m_finished))
      , m_next_id(other.m_next_id)
      , m_dispatcher(other.m_dispatcher)
  {
  }

  // Delete move assignment
  auto operator=(aws_lambda_nghttp2_dispatcher_instance&& other) noexcept
      -> aws_lambda_nghttp2_dispatcher_instance& = delete;

  template<class TaskType>
  auto dispatch(TaskType& t,
                cppless::shared_future<typename TaskType::res> result_future,
                typename TaskType::args args,
                std::optional<tracing_span_ref> span = std::nullopt) -> int
  {
    int id = m_next_id++;

    std::string payload;

    {
      scoped_tracing_span serialization_span(span, "serialization");

      task_data data {t, args};
      payload = Archive::serialize(data);
    }

    std::unique_ptr<cppless::aws::lambda::nghttp2_invocation_request> req =
        std::make_unique<cppless::aws::lambda::nghttp2_invocation_request>(
            task_function_name(t), "$LATEST", payload);

    m_requests.push_back(std::move(req));
    std::unique_ptr<cppless::aws::lambda::nghttp2_invocation_request>& req_ref =
        m_requests.back();

    req_ref->on_result(
        [id, result_future, this, span](const std::string& data) mutable
        {
          scoped_tracing_span deserialization_span(span, "deserialization");
          cppless::shared_future<typename TaskType::res> copy(result_future);

          typename TaskType::res result;
          Archive::deserialize(data, result);

          copy.set_value(result);

          m_finished.insert(id);
        });
    req_ref->submit(m_session, m_lambda_client, m_key, span);

    return id;
  }

  auto wait_one() -> int
  {
    while (m_finished.empty()) {
      m_io_service.run_one();
    }
    auto it = *m_finished.begin();
    m_finished.erase(it);
    return it;
  }

private:
  boost::asio::io_service m_io_service;
  cppless::aws::lambda::client m_lambda_client;
  cppless::aws::aws_v4_derived_key m_key;
  nghttp2::asio_http2::client::session m_session;

  std::vector<std::unique_ptr<cppless::aws::lambda::nghttp2_invocation_request>>
      m_requests;
  std::unordered_set<int> m_finished;

  int m_next_id = 0;

  base_aws_lambda_dispatcher<Archive>& m_dispatcher;
};

template<class Archive>
class aws_lambda_beast_dispatcher_instance
{
public:
  using id_type = int;

  explicit aws_lambda_beast_dispatcher_instance(
      base_aws_lambda_dispatcher<Archive>& dispatcher)
      : m_tls(boost::asio::ssl::context::tlsv12_client)
      , m_lambda_client(dispatcher.lambda_client())
      , m_resolver(m_ioc)
      , m_key(dispatcher.key())
      , m_dispatcher(dispatcher)
  {
    m_resolver.run(m_lambda_client.hostname(), "443");
    m_tls.set_default_verify_paths();
  }

  ~aws_lambda_beast_dispatcher_instance()
  {
    m_ioc.run();
  }

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

  template<class TaskType>
  auto dispatch(TaskType& t,
                cppless::shared_future<typename TaskType::res> result_future,
                typename TaskType::args args,
                std::optional<tracing_span_ref> span = std::nullopt) -> int
  {
    int id = m_next_id++;

    std::string payload;

    {
      scoped_tracing_span serialization_span(span, "serialization");

      task_data data {t, args};
      payload = Archive::serialize(data);
    }

    std::shared_ptr<cppless::aws::lambda::beast_invocation_request> req =
        std::make_shared<cppless::aws::lambda::beast_invocation_request>(
            task_function_name(t), "$LATEST", payload);

    m_requests.push_back(req);

    req->on_result(
        [id, result_future, this, span](const std::string& data) mutable
        {
          scoped_tracing_span deserialization_span(span, "deserialization");
          cppless::shared_future<typename TaskType::res> copy(result_future);

          typename TaskType::res result;
          Archive::deserialize(data, result);

          copy.set_value(result);

          m_finished.insert(id);
        });
    req->submit(m_resolver, m_ioc, m_tls, m_lambda_client, m_key, span);

    return id;
  }

  auto wait_one() -> int
  {
    while (m_finished.empty()) {
      m_ioc.run_one();
    }
    auto it = *m_finished.begin();
    m_finished.erase(it);
    return it;
  }

private:
  boost::asio::io_context m_ioc;
  boost::asio::ssl::context m_tls;
  cppless::aws::lambda::client m_lambda_client;
  beast::resolver_session m_resolver;
  cppless::aws::aws_v4_derived_key m_key;

  std::vector<std::shared_ptr<cppless::aws::lambda::beast_invocation_request>>
      m_requests;
  std::unordered_set<int> m_finished;

  int m_next_id = 0;

  base_aws_lambda_dispatcher<Archive>& m_dispatcher;
};

template<class Archive>
class base_aws_lambda_dispatcher
{
public:
  using default_config = aws::config<>;
  using input_archive = typename Archive::input_archive;
  using output_archive = typename Archive::output_archive;

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

  template<class Lambda, class Res, class... Args>
  static auto main(int /*argc*/, char* /*argv*/[]) -> int
  {
    using invocation_response = ::aws::lambda_runtime::invocation_response;
    using invocation_request = ::aws::lambda_runtime::invocation_request;

    using recv = cppless::receivable_lambda<Lambda, Res, Args...>;
    using uninitialized_recv = cppless::uninitialized_data<recv>;
    ::aws::lambda_runtime::run_handler(
        [](invocation_request const& request)
        {
          uninitialized_recv u;
          std::tuple<Args...> s_args;
          // task_data takes both of its constructor arguments by reference,
          // thus deserializing into `t_data` will populate the context into
          // `m_self` and the arguments into `s_args`.
          task_data<recv, Args...> t_data {u.m_self, s_args};
          Archive::deserialize(request.payload, t_data);
          Res res = std::apply(u.m_self.m_lambda, s_args);

          return invocation_response::success(Archive::serialize(res),
                                              "application/json");
        });
    return 0;
  }

  [[nodiscard]] auto lambda_client() const noexcept
  {
    return m_client;
  }

  [[nodiscard]] auto key() const noexcept
  {
    return m_key;
  }

private:
  cppless::aws::lambda::client m_client;
  cppless::aws::aws_v4_derived_key m_key;
};

template<class Base>
class aws_lambda_env_dispatcher : public Base
{
  static auto from_env() -> Base
  {
    cppless::aws::lambda::client lambda_client;
    auto key = lambda_client.create_derived_key_from_env();
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

template<class Archive = json_binary_archive>
class aws_lambda_nghttp2_dispatcher : public base_aws_lambda_dispatcher<Archive>
{
public:
  using instance = aws_lambda_nghttp2_dispatcher_instance<Archive>;
  using base_aws_lambda_dispatcher<Archive>::base_aws_lambda_dispatcher;
  auto create_instance() -> aws_lambda_nghttp2_dispatcher_instance<Archive>
  {
    return aws_lambda_nghttp2_dispatcher_instance<Archive> {*this};
  }

  using from_env =
      aws_lambda_env_dispatcher<aws_lambda_nghttp2_dispatcher<Archive>>;
};

template<class Archive = json_binary_archive>
class aws_lambda_beast_dispatcher : public base_aws_lambda_dispatcher<Archive>
{
public:
  using instance = aws_lambda_nghttp2_dispatcher_instance<Archive>;
  using base_aws_lambda_dispatcher<Archive>::base_aws_lambda_dispatcher;
  auto create_instance() -> aws_lambda_beast_dispatcher_instance<Archive>
  {
    return aws_lambda_beast_dispatcher_instance<Archive> {*this};
  }

  using from_env =
      aws_lambda_env_dispatcher<aws_lambda_beast_dispatcher<Archive>>;
};

}  // namespace cppless::dispatcher