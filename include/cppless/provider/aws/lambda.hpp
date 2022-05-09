#pragma once

#include <iomanip>
#include <span>
#include <sstream>
#include <utility>
#include <vector>

#include <cppless/provider/aws/client.hpp>
#include <cppless/utils/crypto/hmac.hpp>
#include <cppless/utils/time.hpp>
#include <cppless/utils/url.hpp>
#include <nghttp2/asio_http2_client.h>

#include "cppless/utils/crypto/wrappers.hpp"
namespace cppless::aws::lambda
{

class client : public cppless::aws::client
{
public:
  explicit client(const std::string& region = get_default_region())
      : cppless::aws::client(
          "lambda." + region + ".amazonaws.com", region, "lambda")
  {
  }
};

class invocation_request
    : public request<invocation_request, std::vector<unsigned char>>
{
public:
  invocation_request()
  {
    auto now = std::chrono::system_clock::now();
    m_date = format_aws_date(now);
  }

  explicit invocation_request(
      std::string function_name,
      std::string qualifier,
      std::vector<unsigned char> payload = std::vector<unsigned char> {},
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
  std::vector<unsigned char> m_payload = {};
  std::vector<unsigned char> m_result = {};

public:
  [[nodiscard]] auto get_date() const -> std::string
  {
    return m_date;
  }

  static auto get_http_request_method() -> std::string
  {
    return "POST";
  }

  [[nodiscard]] auto get_canonical_url() const -> std::string
  {
    return "/2015-03-31/functions/" + m_function_name + "/invocations";
  }

  [[nodiscard]] auto get_canonical_query_string() const -> std::string
  {
    return "Qualifier=" + url_encode(m_qualifier);
  }
  [[nodiscard]] auto get_query_string() const -> std::string
  {
    return "Qualifier=" + m_qualifier;
  }

  auto on_http2_response(const nghttp2::asio_http2::client::response& res)
      -> void
  {
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
        [this](const uint8_t* data, std::size_t len)
        {
          m_result.insert(m_result.end(), &data[0], &data[len]);  // NOLINT
          if (len == 0) {
            m_result_callback(m_result);
          }
        });
  }

  [[nodiscard]] auto get_payload_hash() const -> std::vector<unsigned char>
  {
    evp_md_ctx ctx;
    ctx.update({m_payload.cbegin(), m_payload.cend()});
    return ctx.final();
  }

  [[nodiscard]] auto get_payload() const -> std::string
  {
    return {m_payload.cbegin(), m_payload.cend()};
  }
};

}  // namespace cppless::aws::lambda