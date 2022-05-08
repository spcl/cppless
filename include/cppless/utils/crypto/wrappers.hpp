#pragma once

#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace cppless
{

class evp_p_key
{
public:
  explicit evp_p_key(const std::span<unsigned char>& key_data)
      : m_pkey(EVP_PKEY_new_mac_key(
          EVP_PKEY_HMAC, nullptr, key_data.data(), key_data.size()))  // NOLINT
  {
  }

  explicit evp_p_key(const std::string& key_data)
      : m_pkey(EVP_PKEY_new_mac_key(
          EVP_PKEY_HMAC,
          nullptr,
          reinterpret_cast<const unsigned char*>(key_data.data()),  // NOLINT
          key_data.size()))  // NOLINT
  {
  }

  // Copy constructor
  evp_p_key(const evp_p_key& other)
      : m_pkey(other.get_retained())
  {
  }

  // Copy assignment operator
  auto operator=(const evp_p_key& other) -> evp_p_key&
  {
    if (this != &other) {
      m_pkey.reset(other.get_retained());
    }
    return *this;
  }

  [[nodiscard]] auto get() const -> EVP_PKEY*
  {
    return m_pkey.get();
  }

  [[nodiscard]] auto get_retained() const -> EVP_PKEY*
  {
    auto* pkey = m_pkey.get();
    EVP_PKEY_up_ref(pkey);
    return pkey;
  }

private:
  struct deleter
  {
    void operator()(EVP_PKEY* b)
    {
      EVP_PKEY_free(b);
    }
  };
  using p_key_ptr = std::unique_ptr<EVP_PKEY, deleter>;
  p_key_ptr m_pkey;
};

class evp_sign_ctx
{
public:
  explicit evp_sign_ctx(const evp_p_key& key)
      : m_ctx(EVP_MD_CTX_create())
  {
    EVP_DigestSignInit(m_ctx.get(), nullptr, EVP_sha256(), nullptr, key.get());
  }

  evp_sign_ctx()
      : m_ctx(EVP_MD_CTX_create())
  {
    EVP_DigestInit(m_ctx.get(), EVP_sha256());
  }

  auto update(const std::span<unsigned char>& t) -> void
  {
    EVP_DigestSignUpdate(m_ctx.get(), t.data(), t.size());
  }

  auto update(const std::span<char>& str) -> void
  {
    EVP_DigestSignUpdate(m_ctx.get(), str.data(), str.size());
  }

  auto update(const std::string& str) -> void
  {
    EVP_DigestSignUpdate(m_ctx.get(), str.data(), str.size());
  }

  auto final() -> std::vector<unsigned char>
  {
    std::vector<unsigned char> result;
    std::size_t out_len = 0;
    EVP_DigestSignFinal(m_ctx.get(), nullptr, &out_len);
    result.resize(out_len);
    EVP_DigestSignFinal(m_ctx.get(), result.data(), &out_len);
    return result;
  }

private:
  struct deleter
  {
    void operator()(EVP_MD_CTX* b)
    {
      EVP_MD_CTX_free(b);
    }
  };
  using md_ctx_ptr = std::unique_ptr<EVP_MD_CTX, deleter>;
  md_ctx_ptr m_ctx;
};

class evp_md_ctx
{
public:
  evp_md_ctx()
      : m_ctx(EVP_MD_CTX_create())
  {
    EVP_DigestInit(m_ctx.get(), EVP_sha256());
  }

  auto update(const std::span<unsigned char>& t) -> void
  {
    EVP_DigestUpdate(m_ctx.get(), t.data(), t.size());
  }

  auto update(const std::span<char>& str) -> void
  {
    EVP_DigestUpdate(m_ctx.get(), str.data(), str.size());
  }

  auto update(const std::string& str) -> void
  {
    EVP_DigestUpdate(m_ctx.get(), str.data(), str.size());
  }

  auto final() -> std::vector<unsigned char>
  {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest {};
    unsigned int out_len = 0;
    EVP_DigestFinal(m_ctx.get(), digest.data(), &out_len);

    std::vector<unsigned char> result {&digest[0], &digest.at(out_len)};
    return result;
  }

private:
  struct deleter
  {
    void operator()(EVP_MD_CTX* b)
    {
      EVP_MD_CTX_free(b);
    }
  };
  using md_ctx_ptr = std::unique_ptr<EVP_MD_CTX, deleter>;
  md_ctx_ptr m_ctx;
};
}  // namespace cppless