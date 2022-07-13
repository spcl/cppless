#pragma once

#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <cppless/provider/aws/client.hpp>
#include <cppless/utils/crypto/hmac.hpp>
#include <cppless/utils/crypto/wrappers.hpp>
#include <cppless/utils/time.hpp>
#include <cppless/utils/url.hpp>
#include <nghttp2/asio_http2_client.h>

namespace cppless::aws::lambda
{

class client : public cppless::aws::client
{
public:
  explicit client(const std::string& region = default_region())
      : cppless::aws::client(
          "lambda." + region + ".amazonaws.com", region, "lambda")
  {
  }
};

template<class Base>
class base_invocation_request : public Base
{
public:
  base_invocation_request()
  {
    auto now = std::chrono::system_clock::now();
    m_date = format_aws_date(now);
  }

  explicit base_invocation_request(
      std::string function_name,
      std::string qualifier,
      std::string payload = "",
      std::string date = format_aws_date(std::chrono::system_clock::now()))
      : m_date(std::move(date))
      , m_function_name(std::move(function_name))
      , m_qualifier(std::move(qualifier))
      , m_payload(std::move(payload))
  {
  }

private:
  std::string m_date;
  std::string m_function_name;
  std::string m_qualifier;
  std::string m_payload = {};

public:
  auto set_tags(tracing_span_ref span) -> void
  {
    span.set_tag("date", m_date);
    span.set_tag("function_name", m_qualifier);
    span.set_tag("payload_size", std::to_string(m_payload.size()));
  }
  [[nodiscard]] auto date() const -> std::string
  {
    return m_date;
  }

  static auto http_request_method() -> std::string
  {
    return "POST";
  }

  [[nodiscard]] auto canonical_url() const -> std::string
  {
    return "/2015-03-31/functions/" + m_function_name + "/invocations";
  }

  [[nodiscard]] auto canonical_query_string() const -> std::string
  {
    return "Qualifier=" + url_encode(m_qualifier);
  }
  [[nodiscard]] auto query_string() const -> std::string
  {
    return "Qualifier=" + m_qualifier;
  }
  [[nodiscard]] auto payload_hash() const -> std::vector<unsigned char>
  {
    evp_md_ctx ctx;
    ctx.update({m_payload.begin(), m_payload.end()});
    return ctx.final();
  }

  [[nodiscard]] auto payload() const -> std::string
  {
    return m_payload;
  }
};

struct invocation_response
{
  std::string body;
  std::string request_id;  // x-amzn-RequestId
} __attribute__((aligned(64)));

class nghttp2_invocation_request
    : public base_invocation_request<
          nghttp2_request<nghttp2_invocation_request, invocation_response>>
{
public:
  using base_invocation_request::base_invocation_request;
  auto on_http2_response(const nghttp2::asio_http2::client::response& res,
                         std::optional<tracing_span_ref> span) -> void
  {
    if (span) {
      set_tags(*span);
    }
    if (res.status_code() != 200) {
      res.on_data(
          [&res, buffer = std::vector<unsigned char> {}](
              const uint8_t* data, std::size_t len) mutable
          {
            buffer.insert(buffer.end(), &data[0], &data[len]);  // NOLINT
            if (len == 0) {
              std::cerr << "status_code: " << res.status_code() << std::endl;
              std::cerr << "buffer: " << std::endl;
              std::cerr << buffer.data() << std::endl;
            }
          });
      return;
    }

    res.on_data(
        [this, &res, span](const uint8_t* data, std::size_t len) mutable
        {
          m_result.insert(m_result.end(), &data[0], &data[len]);  // NOLINT
          if (len == 0) {
            auto request_id_it = res.header().find("x-amzn-RequestId");
            std::string request_id = request_id_it != res.header().end()
                ? request_id_it->second.value
                : "";
            auto date_it = res.header().find("Date");
            std::string date =
                date_it != res.header().end() ? date_it->second.value : "";
            if (span) {
              span->set_tag("request_id", request_id);
              span->set_tag("response_date", date);
            }
            m_result_callback({
                .body = std::string {m_result.begin(), m_result.end()},
                .request_id = request_id,
            });
          }
        });
  }

private:
  std::vector<unsigned char> m_result = {};
};

class beast_invocation_request
    : public base_invocation_request<
          beast_request<beast_invocation_request, invocation_response>>
{
public:
  using base_invocation_request::base_invocation_request;
  auto on_http1_response(
      const boost::beast::http::response<boost::beast::http::string_body>& res,
      std::optional<tracing_span_ref> span) -> void
  {
    if (span) {
      set_tags(*span);
    }
    if (res.result() != boost::beast::http::status::ok) {
      std::cerr << "status_code: " << res.result() << std::endl;
      std::cerr << "body: " << res.body() << std::endl;

      return;
    }
    std::string request_id = std::string {res["x-amzn-RequestId"]};
    std::string response_date = std::string {res["Date"]};
    if (span) {
      span->set_tag("request_id", request_id);
      span->set_tag("response_date", response_date);
    }
    m_result_callback({
        .body = res.body(),
        .request_id = request_id,
    });
  }
};

}  // namespace cppless::aws::lambda