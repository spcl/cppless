#pragma once

#include <string>

#include <cppless/utils/crypto/hmac.hpp>
#include <cppless/utils/crypto/wrappers.hpp>

namespace cppless::aws
{

class aws_v4_derived_key
{
public:
  aws_v4_derived_key(evp_p_key key, std::string id)
      : m_key(std::move(key))
      , m_id(std::move(id))
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

private:
  evp_p_key m_key;
  std::string m_id;
};

class aws_v4_signing_key
{
public:
  std::string date;
  std::string region;
  std::string service;
  std::string secret;
  std::string id;

  [[nodiscard]] auto derived_key() const -> aws_v4_derived_key
  {
    auto k_date = hmac("AWS4" + secret, date);
    auto k_region = hmac(std::span {k_date}, region);
    auto k_service = hmac(std::span {k_region}, service);
    auto k_signing = hmac(std::span {k_service}, "aws4_request");

    return aws_v4_derived_key {evp_p_key {k_signing}, id};
  };
};

}  // namespace cppless::aws