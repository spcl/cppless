#pragma once

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <boost/algorithm/hex.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/core/ignore_unused.hpp>
#include <cppless/provider/aws/auth.hpp>
#include <cppless/utils/beast/http_request_session.hpp>
#include <cppless/utils/tracing.hpp>
#include <nghttp2/asio_http2_client.h>

namespace cppless::aws
{

inline auto default_region() -> std::string
{
  return std::getenv("AWS_REGION");  // NOLINT
}

class client
{
public:
  client(std::string hostname, std::string region, std::string service)
      : m_hostname(std::move(hostname))
      , m_region(std::move(region))
      , m_service(std::move(service))
  {
  }

  [[nodiscard]] auto create_derived_key(
      const std::string& id,
      const std::string& secret,
      std::optional<std::string> security_token = std::nullopt) const
      -> aws_v4_derived_key
  {
    // Get current date
    auto now = std::chrono::system_clock::now();

    std::time_t time = std::chrono::system_clock::to_time_t(now);
    tm gm_tm = *gmtime(&time);  // NOLINT

    // YYYYMMDD
    std::stringstream ss;
    const auto tm_start_year = 1900;
    ss << std::setw(4) << std::setfill('0') << gm_tm.tm_year + tm_start_year;
    ss << std::setw(2) << std::setfill('0') << gm_tm.tm_mon + 1;
    ss << std::setw(2) << std::setfill('0') << gm_tm.tm_mday;

    aws_v4_signing_key key {
        .date = ss.str(),
        .region = m_region,
        .service = m_service,
        .secret = secret,
        .id = id,
        .security_token = std::move(security_token),
    };

    return key.derived_key();
  }

  [[nodiscard]] auto create_derived_key_from_env() const -> aws_v4_derived_key
  {
    auto* key_id_env = std::getenv("AWS_ACCESS_KEY_ID");  // NOLINT
    auto* key_secret_env = std::getenv("AWS_SECRET_ACCESS_KEY");  // NOLINT
    if (key_id_env == nullptr || key_secret_env == nullptr) {
      throw std::runtime_error(
          "AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY must be set");
    }

    auto* session_token_env = std::getenv("AWS_SESSION_TOKEN");  // NOLINT
    std::optional<std::string> session_token;
    if (session_token_env != nullptr) {
      session_token = std::string {session_token_env};
    }
    return create_derived_key(key_id_env, key_secret_env, session_token);
  }

  [[nodiscard]] auto hostname() const -> std::string
  {
    return m_hostname;
  }

  [[nodiscard]] auto region() const -> std::string
  {
    return m_region;
  }

  [[nodiscard]] auto service() const -> std::string
  {
    return m_service;
  }

private:
  std::string m_hostname;
  std::string m_region;
  std::string m_service;
};

template<class DerivedRequest, class ResultType, class ErrorType>
class base_request
{
public:
  base_request() = default;

  [[nodiscard]] auto compute_canonical_hash(const std::string& payload_hash_hex,
                                            const client& client) const
      -> std::vector<unsigned char>
  {
    using namespace std::string_literals;
    evp_md_ctx ctx;

    // Cast self to derived type.
    const auto& request = static_cast<const DerivedRequest&>(*this);

    ctx.update(request.http_request_method());
    ctx.update("\n"s);

    ctx.update(request.canonical_url());
    ctx.update("\n"s);

    ctx.update(request.canonical_query_string());
    ctx.update("\n"s);

    ctx.update(request.canonical_headers(client));
    ctx.update("\n"s);

    ctx.update(request.signed_headers());
    ctx.update("\n"s);

    ctx.update(payload_hash_hex);

    return ctx.final();
  }

  [[nodiscard]] auto compute_signature(const std::string& payload_hash_hex,
                                       const client& client,
                                       const evp_p_key& p_key) const
      -> std::vector<unsigned char>
  {
    auto canonical_hash = compute_canonical_hash(payload_hash_hex, client);

    // Cast self to derived type.
    const auto& request = static_cast<const DerivedRequest&>(*this);

    evp_sign_ctx ctx {p_key};
    ctx.update("AWS4-HMAC-SHA256\n");
    auto date_time = request.date();
    const auto date_length = 8;
    ctx.update(date_time + "\n");
    auto date = date_time.substr(0, date_length);
    ctx.update(date + "/" + client.region() + "/" + client.service()
               + "/aws4_request\n");
    std::string canonical_hash_hex;

    boost::algorithm::hex_lower(canonical_hash.cbegin(),
                                canonical_hash.cend(),
                                std::back_inserter(canonical_hash_hex));
    ctx.update(canonical_hash_hex);
    return ctx.final();
  }

  [[nodiscard]] auto compute_authorization_header(
      const std::string& payload_hash_hex,
      const client& client,
      const aws_v4_derived_key& key) const -> std::string
  {
    // Cast self to derived type.
    const auto& request = static_cast<const DerivedRequest&>(*this);

    auto signature = compute_signature(payload_hash_hex, client, key.key());
    std::vector<unsigned char> signature_hex;
    boost::algorithm::hex_lower(signature.cbegin(),
                                signature.cend(),
                                std::back_inserter(signature_hex));

    const auto date_length = 8;
    std::stringstream ss;
    ss << "AWS4-HMAC-SHA256 Credential=" << key.id() << "/"
       << request.date().substr(0, date_length) << "/" << client.region() << "/"
       << client.service()
       << "/aws4_request, SignedHeaders=" << request.signed_headers()
       << ", Signature="
       << std::string {signature_hex.cbegin(), signature_hex.cend()};
    return ss.str();
  }

  auto on_result(std::function<void(const ResultType&)> callback)
  {
    m_result_callback = std::move(callback);
  }

  auto on_error(std::function<void(const ErrorType&)> callback)
  {
    m_error_callback = std::move(callback);
  }

  // Default implementations

  [[nodiscard]] auto canonical_headers(const client& client) const
      -> std::string
  {
    const auto& request = static_cast<const DerivedRequest&>(*this);
    return "host:" + client.hostname() + "\n" + "x-amz-date:" + request.date()
        + "\n";
  }
  static auto signed_headers() -> std::string
  {
    return "host;x-amz-date";
  }

protected:
  std::function<void(const ResultType&)> m_result_callback;  // NOLINT
  std::function<void(const ErrorType&)> m_error_callback;  // NOLINT
};

// nghttp2
template<class DerivedRequest, class ResultType, class ErrorType>
class nghttp2_request
    : public base_request<DerivedRequest, ResultType, ErrorType>
{
public:
  auto submit(nghttp2::asio_http2::client::session& sess,
              const client& client,
              const aws_v4_derived_key& key,
              std::optional<tracing_span_ref> span)
      -> const nghttp2::asio_http2::client::request*
  {
    boost::ignore_unused(span);

    boost::system::error_code ec;

    auto& request = static_cast<DerivedRequest&>(*this);

    auto payload_hash = request.payload_hash();
    std::string payload_hash_hex;
    payload_hash_hex.reserve(payload_hash.size() * 2L);
    boost::algorithm::hex_lower(payload_hash.begin(),
                                payload_hash.end(),
                                std::back_inserter(payload_hash_hex));

    auto auth_header =
        request.compute_authorization_header(payload_hash_hex, client, key);

    auto full_url = "https://" + client.hostname() + request.canonical_url();
    auto query_string = request.canonical_query_string();
    if (!query_string.empty()) {
      full_url += "?" + query_string;
    }

    nghttp2::asio_http2::header_map headers = {
        {"X-Amz-Date", {request.date(), false}},
        {"X-Amz-Content-Sha256", {payload_hash_hex, false}},
        {"Authorization", {auth_header, true}},
    };

    auto security_token = key.security_token();
    if (security_token) {
      headers.insert({"X-Amz-Security-Token", {*security_token, true}});
    }

    const nghttp2::asio_http2::client::request* sess_req =
        sess.submit(ec,
                    request.http_request_method(),
                    full_url,
                    request.payload(),
                    headers);
    sess_req->on_response(
        [&request, span](const nghttp2::asio_http2::client::response& res)
        { request.on_http2_response(res, span); });
    return sess_req;
  }
};

// beast
template<class DerivedRequest, class ResultType, class ErrorType>
class beast_request : public base_request<DerivedRequest, ResultType, ErrorType>
{
public:
  auto submit(beast::resolver_session& resolver_session,
              boost::beast::net::io_context& ioc,
              boost::asio::ssl::context& tls,
              const client& client,
              const aws_v4_derived_key& key,
              std::optional<tracing_span_ref> span = std::nullopt)
  {
    if (span) {
      span->inline_children();
    }

    auto& request = static_cast<DerivedRequest&>(*this);

    auto payload_hash = request.payload_hash();
    std::string payload_hash_hex;
    payload_hash_hex.reserve(payload_hash.size() * 2L);
    boost::algorithm::hex_lower(payload_hash.begin(),
                                payload_hash.end(),
                                std::back_inserter(payload_hash_hex));

    std::optional<tracing_span_ref> authorization_span;
    if (span) {
      authorization_span.emplace(span->create_child("authorization").start());
    }

    auto auth_header =
        request.compute_authorization_header(payload_hash_hex, client, key);

    if (authorization_span) {
      authorization_span->end();
    }

    const auto method = request.http_request_method();

    auto target = request.canonical_url();
    auto query_string = request.query_string();
    if (!query_string.empty()) {
      target += "?" + query_string;
    }

    m_request_session = std::make_shared<beast::http_request_session>(ioc, tls);
    boost::beast::http::request<boost::beast::http::string_body>& req =
        m_request_session->request();
    req.body() = request.payload();
    req.method(boost::beast::http::string_to_verb(method));
    req.target(target);

    req.set(boost::beast::http::field::host, client.hostname());
    req.set("X-Amz-Content-Sha256", payload_hash_hex);
    req.set("X-Amz-Date", request.date());

    auto security_token = key.security_token();
    if (security_token) {
      req.set("X-Amz-Security-Token", *security_token);
    }

    req.set(boost::beast::http::field::authorization, auth_header);
    req.set(boost::beast::http::field::content_length,
            std::to_string(req.body().size()));
    req.set(boost::beast::http::field::accept, "*/*");
    req.version(11);
    req.keep_alive(/*value=*/false);

    m_request_session->set_callback(
        [&request, span](
            const boost::beast::http::response<boost::beast::http::string_body>&
                res)
        {
          if (res.result() != boost::beast::http::status::ok) {
            std::cerr << "status_code: " << res.result() << std::endl;
            std::cerr << "body: " << res.body() << std::endl;

            return;
          }
          request.on_http1_response(res, span);
        });

    std::optional<tracing_span_ref> request_span;
    if (span) {
      request_span.emplace(
          span->create_child("http_request").inline_children());
    }
    m_request_session->run(resolver_session, request_span);
  }

private:
  std::shared_ptr<beast::http_request_session> m_request_session;
};

}  // namespace cppless::aws