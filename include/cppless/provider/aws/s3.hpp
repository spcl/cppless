#pragma once

#include <initializer_list>
#include <iomanip>
#include <span>
#include <sstream>
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

namespace cppless::aws::s3
{
class client : public cppless::aws::client
{
public:
  explicit client(const std::string& bucket_name,
                  const std::string& region = default_region())
      : cppless::aws::client(
          bucket_name + ".s3." + region + ".amazonaws.com", region, "s3")
  {
  }
};

template<class Base>
class base_get_object_request : public Base
{
public:
  explicit base_get_object_request(
      std::string key,
      std::string date = format_aws_date(std::chrono::system_clock::now()))
      : m_date(std::move(date))
      , m_key(std::move(key))
  {
  }

private:
  std::string m_date;
  std::string m_key;

public:
  [[nodiscard]] auto date() const -> std::string
  {
    return m_date;
  }
  static auto http_request_method() -> std::string
  {
    return "GET";
  }
  [[nodiscard]] auto canonical_url() const -> std::string
  {
    return "/" + m_key;
  }
  [[nodiscard]] auto canonical_query_string() const -> std::string
  {
    return "";
  }
  [[nodiscard]] auto query_string() const -> std::string
  {
    return "";
  }
  [[nodiscard]] auto payload_hash() const -> std::vector<unsigned char>
  {
    // Sha256 of an empty string
    const std::initializer_list<unsigned char> empty_string = {
        227, 176, 196, 66,  152, 252, 28,  20, 154, 251, 244,
        200, 153, 111, 185, 36,  39,  174, 65, 228, 100, 155,
        147, 76,  164, 149, 153, 27,  120, 82, 184, 85};
    return empty_string;
  }

  [[nodiscard]] auto payload() const -> std::string
  {
    return "";
  }
};

class nghttp2_get_object_request
    : public base_get_object_request<
          nghttp2_request<nghttp2_get_object_request, std::string>>
{
public:
  using base_get_object_request::base_get_object_request;
  auto on_http2_response(const nghttp2::asio_http2::client::response& res)
      -> void
  {
    res.on_data(
        [this, result = std::vector<unsigned char> {}](const uint8_t* data,
                                                       std::size_t len) mutable
        {
          result.insert(result.end(), &data[0], &data[len]);  // NOLINT
          if (len == 0) {
            m_result_callback(std::string {result.begin(), result.end()});
          }
        });
  }
};

class beast_get_object_request
    : public base_get_object_request<
          beast_request<beast_get_object_request, std::string>>
{
public:
  using base_get_object_request::base_get_object_request;
  auto on_http1_response(
      const boost::beast::http::response<boost::beast::http::string_body>& res)
      -> void
  {
    m_result_callback(res.body());
  }
};

}  // namespace cppless::aws::s3