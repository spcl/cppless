#pragma once

#include <iostream>
#include <string>
#include <string_view>

// Adopted from
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0259r0.pdf
template<class CharT, size_t S>
class basic_fixed_string
{
public:
  using size_type = size_t;
  using value_type = CharT;

  static constexpr size_t n = S - 1;

  using reference = value_type&;
  using const_reference = const value_type&;

  constexpr basic_fixed_string();
  constexpr explicit basic_fixed_string(const value_type (&a)[S]);  // NOLINT

  [[nodiscard]] constexpr auto size() const noexcept -> size_type;
  [[nodiscard]] constexpr auto data() const noexcept -> const value_type*;

  [[nodiscard]] auto str() const noexcept -> std::string;

  constexpr auto operator[](size_type pos) const noexcept -> const_reference;
  constexpr auto operator[](size_type pos) noexcept -> reference;

  value_type data_[S];  // NOLINT
};

template<class CharT, size_t S>
constexpr basic_fixed_string<CharT, S>::basic_fixed_string()
    : data_()
{
  for (size_t i = 0; i < S; i++) {
    data_[i] = CharT(0);
  }
}

template<class CharT, size_t S>
constexpr basic_fixed_string<CharT, S>::basic_fixed_string(
    const CharT (&a)[S])  // NOLINT
{
  for (size_t i = 0; i < S; i++) {
    data_[i] = a[i];  // NOLINT
  }
}

template<class CharT, size_t S>
constexpr auto basic_fixed_string<CharT, S>::size() const noexcept -> size_t
{
  return S - 1;
}

template<class CharT, size_t S>
constexpr auto basic_fixed_string<CharT, S>::data() const noexcept
    -> const CharT*
{
  return data_;
}

template<class CharT, size_t S>
auto basic_fixed_string<CharT, S>::str() const noexcept -> std::string
{
  return std::string {data_};
}

template<class CharT, size_t S>
constexpr auto basic_fixed_string<CharT, S>::operator[](size_type pos)
    const noexcept -> typename basic_fixed_string<CharT, S>::const_reference
{
  return data_[pos];
}

template<class CharT, size_t S>
constexpr auto basic_fixed_string<CharT, S>::operator[](size_type pos) noexcept
    -> typename basic_fixed_string<CharT, S>::reference
{
  return data_[pos];
}

template<class CharT, size_t N, size_t M>
constexpr auto operator+(const basic_fixed_string<CharT, N>& lhs,
                         const basic_fixed_string<CharT, M>& rhs) noexcept
    -> basic_fixed_string<CharT, N + M - 1>
{
  basic_fixed_string<CharT, N + M - 1> res;
  for (size_t i = 0; i < N - 1; i++) {
    res[i] = lhs[i];
  }
  for (size_t j = 0; j < M - 1; j++) {
    res[N + j - 1] = rhs[j];
  }
  return res;
}

template<class CharT, size_t N, size_t M>
constexpr auto operator+(const basic_fixed_string<CharT, N>& lhs,
                         const CharT (&rhs)[M]) noexcept  // NOLINT
    -> basic_fixed_string<CharT, N + M - 1>
{
  basic_fixed_string<CharT, M> r(rhs);
  return lhs + r;
}

template<class CharT, size_t N, size_t M>
constexpr auto operator+(const CharT (&lhs)[N],  // NOLINT
                         const basic_fixed_string<CharT, M>& rhs) noexcept
    -> basic_fixed_string<CharT, N + M - 1>
{
  basic_fixed_string<CharT, N> l(lhs);
  return l + rhs;
}

template<class CharT, size_t N>
auto operator<<(std::ostream& os, const basic_fixed_string<CharT, N>& str)
    -> std::ostream&
{
  os << str.data();
  return os;
}

template<class CharT, size_t X>
constexpr auto make_fixed_string(const CharT (&a)[X]) noexcept  // NOLINT
    -> basic_fixed_string<CharT, X>
{
  basic_fixed_string<CharT, X> s {a};
  return s;
}

template<class CharT>
constexpr auto length(const CharT* c_str) noexcept -> size_t
{
  size_t len = 0;
  while (*c_str++) {
    len++;
  }
  return len;
}

// This doesn't really work, but it's better than an error
template<class T>
constexpr auto type_display_name() noexcept
{
  auto s = __PRETTY_FUNCTION__;
  return s;
}

template<class T>
constexpr auto type_unique_name() noexcept
{
#ifdef __CPPLESS__
  constexpr auto name = __builtin_unique_stable_name(T);
  constexpr size_t l = length(name);
#else
  constexpr auto name = type_display_name<T>();
  constexpr size_t l = length(name);
#endif

  basic_fixed_string<char, l> s;
  for (size_t i = 0; i < length(name); i++) {
    s[i] = name[i];
  }
  return s;
}

template<typename T, typename... Ts>
constexpr auto types_unique_names_helper()
{
  if constexpr (sizeof...(Ts) == 0) {
    return type_unique_name<T>();
  } else {
    return type_unique_name<T>() + make_fixed_string(", ")
        + type_unique_name<Ts...>();
  }
}

template<typename... Ts>
constexpr auto types_unique_names()
{
  if constexpr (sizeof...(Ts) == 0) {
    return make_fixed_string("");
  } else {
    return types_unique_names_helper<Ts...>();
  }
}

template<size_t N>
using fixed_string = basic_fixed_string<char, N>;

template<size_t S>
class intermediate_fixed_string
{
public:
  constexpr intermediate_fixed_string(const char (&a)[S])  // NOLINT
  {
    for (size_t i = 0; i < S; i++) {
      data[i] = a[i];  // NOLINT
    }
  };
  char data[S] {};  // NOLINT
};

template<intermediate_fixed_string S>
constexpr auto operator"" _f()
{
  return make_fixed_string(S.data);
}