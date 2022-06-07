#pragma once

#include <optional>
#include <string>

#include <cppless/utils/crypto/hmac.hpp>
#include <cppless/utils/crypto/wrappers.hpp>

namespace cppless::aws
{

class aws_v4_derived_key
{
public:
  aws_v4_derived_key(const evp_p_key& key,
                     std::string id,
                     std::optional<std::string> security_token)
      : m_key(key)
      , m_id(std::move(id))
      , m_security_token(std::move(security_token))
  {
  }

  [[nodiscard]] auto get_key() const -> const evp_p_key&
  {
    return m_key;
  }

  [[nodiscard]] auto get_id() const -> std::string
  {
    return m_id;
  }

  [[nodiscard]] auto get_security_token() const -> std::optional<std::string>
  {
    return m_security_token;
  }

private:
  evp_p_key m_key;
  std::string m_id;
  std::optional<std::string> m_security_token;
};

class aws_v4_signing_key
{
public:
  std::string date;
  std::string region;
  std::string service;
  std::string secret;
  std::string id;
  std::optional<std::string> security_token;

  [[nodiscard]] auto derived_key() const -> aws_v4_derived_key
  {
    auto k_date = hmac("AWS4" + secret, date);
    auto k_region = hmac(std::span {k_date}, region);
    auto k_service = hmac(std::span {k_region}, service);
    auto k_signing = hmac(std::span {k_service}, "aws4_request");

    return aws_v4_derived_key {evp_p_key {k_signing}, id, security_token};
  };
};

}  // namespace cppless::aws