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
#include <cppless/utils/uninitialized.hpp>

namespace cppless::dispatcher
{

class aws_lambda_dispatcher
{
public:
  using input_archive = cereal::JSONInputArchive;
  using output_archive = cereal::JSONOutputArchive;

  using task = cppless::task<aws_lambda_dispatcher>;
  template<class T>
  using sendable_task = typename task::template sendable<T>;

  explicit aws_lambda_dispatcher(std::string prefix,
                                 cppless::aws::lambda::client client,
                                 cppless::aws::aws_v4_derived_key key)
      : m_prefix(std::move(prefix))
      , m_client(std::move(client))
      , m_key(std::move(key))
  {
  }

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
          std::stringstream ss_in {request.payload};
          cereal::JSONInputArchive iar {ss_in};
          uninitialized_recv u;
          std::tuple<Args...> s_args;
          // task_data takes both of its constructor arguments by reference,
          // thus deserializing into `t_data` will populate the context into
          // `m_self` and the arguments into `s_args`.
          task_data<recv, Args...> t_data {u.m_self, s_args};
          iar(t_data);
          std::stringstream ss_out;
          {
            Res res = std::apply(u.m_self.m_lambda, s_args);
            cereal::JSONOutputArchive oar(ss_out);
            oar(res);
          }
          return invocation_response::success(ss_out.str(), "application/json");
        });
    return 0;
  }

  class instance
  {
  public:
    using id_type = int;

    static auto create_session(
        boost::asio::io_service& io_service,
        const cppless::aws::lambda::client& lambda_client)
        -> nghttp2::asio_http2::client::session
    {
      boost::system::error_code ec;

      boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
      tls.set_default_verify_paths();
      nghttp2::asio_http2::client::configure_tls_context(ec, tls);

      return {io_service, tls, lambda_client.get_hostname(), "443"};
    }

    explicit instance(aws_lambda_dispatcher& dispatcher)
        : m_key(m_lambda_client.create_derived_key_from_env())
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

    // Delete copy constructor
    instance(const instance&) = delete;
    // Delete copy assignment
    auto operator=(const instance&) -> instance& = delete;

    // Move constructor
    instance(instance&& other) noexcept
        : m_lambda_client(std::move(other.m_lambda_client))
        , m_key(std::move(other.m_key))
        , m_session(std::move(other.m_session))
        , m_requests(std::move(other.m_requests))
        , m_finished(std::move(other.m_finished))
        , m_pending_requests(other.m_pending_requests)
        , m_next_id(other.m_next_id)
        , m_dispatcher(other.m_dispatcher)
    {
    }

    // Move assignment
    auto operator=(instance&& other) noexcept -> instance&
    {
      m_lambda_client = std::move(other.m_lambda_client);
      m_key = std::move(other.m_key);
      m_session = std::move(other.m_session);
      m_requests = std::move(other.m_requests);
      m_finished = std::move(other.m_finished);
      m_pending_requests = other.m_pending_requests;
      m_next_id = other.m_next_id;
      m_dispatcher = other.m_dispatcher;
      return *this;
    }

    template<class Res, class... Args>
    auto dispatch(sendable_task<Res(Args...)>& t,
                  cppless::shared_future<Res> result_future,
                  std::tuple<Args...> args) -> int
    {
      using specialized_task_data =
          task_data<sendable_task<Res(Args...)>, Args...>;

      int id = m_next_id++;

      auto raw_function_name = t.identifier();
      std::string function_name;

      evp_md_ctx ctx;
      ctx.update(raw_function_name);
      auto binary_digest = ctx.final();

      boost::algorithm::hex_lower(binary_digest,
                                  std::back_inserter(function_name));

      std::stringstream ss_out;
      {
        cereal::JSONOutputArchive oar(ss_out);
        specialized_task_data data {t, args};
        oar(data);
      }

      auto string_payload = ss_out.str();
      std::vector<unsigned char> payload(string_payload.begin(),
                                         string_payload.end());
      std::unique_ptr<cppless::aws::lambda::invocation_request> req =
          std::make_unique<cppless::aws::lambda::invocation_request>(
              function_name, "$LATEST", payload);

      m_requests.push_back(std::move(req));
      std::unique_ptr<cppless::aws::lambda::invocation_request>& req_ref =
          m_requests.back();

      req_ref->on_result(
          [id, result_future, this](const std::vector<unsigned char>& data)
          {
            cppless::shared_future<Res> copy(result_future);
            std::string result_string {data.begin(), data.end()};
            std::stringstream ss_in {result_string};
            cereal::JSONInputArchive iar {ss_in};

            Res result;

            iar(result);

            copy.set_value(result);
            m_finished.insert(id);
          });
      const auto* sess_req = req_ref->submit(m_session, m_lambda_client, m_key);
      sess_req->on_close(
          [this](const auto&)
          {
            m_pending_requests--;
            if (m_pending_requests == 0) {
              m_session.shutdown();
            }
          });

      m_pending_requests++;

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

    std::vector<std::unique_ptr<cppless::aws::lambda::invocation_request>>
        m_requests;
    std::unordered_set<int> m_finished;

    int m_pending_requests = 0;
    int m_next_id = 0;

    aws_lambda_dispatcher& m_dispatcher;
  };

  auto create_instance() -> instance
  {
    return instance(*this);
  }

private:
  std::string m_prefix;

  cppless::aws::lambda::client m_client;
  cppless::aws::aws_v4_derived_key m_key;
};
}  // namespace cppless::dispatcher