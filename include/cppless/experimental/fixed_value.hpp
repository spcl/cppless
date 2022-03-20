#pragma once

#include <cstddef>
#include <iostream>
#include <string_view>

#include "../fixed_string.hpp"

template<size_t KeyL, class ValueT>
class map_entry
{
public:
  using key_type = fixed_string<KeyL>;
  using value_type = ValueT;

  constexpr map_entry(const key_type key, const value_type value)
      : m_key(key)
      , m_value(value)
  {
  }

  constexpr auto key() const noexcept -> const key_type&
  {
    return m_key;
  }

  constexpr auto value() const noexcept -> const value_type&
  {
    return m_value;
  }

private:
  key_type m_key;
  value_type m_value;
};

template<class... EntryT>
class map_impl
{
public:
  // Empty
};

// Specialize for n = 0
template<>
class map_impl<>
{
public:
  // Empty
};

// Specialize for n >= 1
template<class T, class... EntryT>
class map_impl<T, EntryT...>
{
public:
  using entry_type = T;

  constexpr explicit map_impl(const T& entry, const EntryT&... rest)
      : m_entry(entry)
      , m_map_impl(rest...)
  {
  }

private:
  T m_entry;
  map_impl<EntryT...> m_map_impl;
};

template<class... EntryT>
class map
{
public:
  constexpr explicit map(EntryT... x)
      : m_map(map_impl<EntryT...>(x...))
  {
  }

private:
  map_impl<EntryT...> m_map;
};

template<size_t S>
class fixed_string_key
{
public:
  constexpr fixed_string_key() = default;
  constexpr fixed_string_key(const char (&a)[S])  // NOLINT
  {
    for (size_t i = 0; i < S; i++) {
      data[i] = a[i];  // NOLINT
    }
  }
  char data[S];  // NOLINT

  template<class V>
  constexpr auto operator=(const V v) const noexcept  // NOLINT
  {
    fixed_string<S> k;
    for (size_t i = 0; i < S; i++) {
      k[i] = data[i];  // NOLINT
    }
    return map_entry(k, v);
  }
};

template<fixed_string_key K>
constexpr auto operator"" _k()
{
  return K;
}
