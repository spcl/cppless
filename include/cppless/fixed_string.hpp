#include <iostream>
#include <string>
#include <string_view>

// Adopted from
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0259r0.pdf
template<class charT, size_t N>
class basic_fixed_string
{
public:
  using size_type = size_t;
  using value_type = charT;

  using reference = value_type&;
  using const_reference = const value_type&;

  constexpr basic_fixed_string();
  constexpr basic_fixed_string(const charT (&a)[N + 1]);

  constexpr size_type size() const noexcept;
  constexpr const charT* data() const noexcept;

  constexpr const_reference operator[](size_type pos) const noexcept;
  constexpr reference operator[](size_type pos) noexcept;

private:
  char data_[N + 1];
};

template<class charT, size_t N>
constexpr basic_fixed_string<charT, N>::basic_fixed_string()
{
  data_[N] = 0;
}

template<class charT, size_t N>
constexpr basic_fixed_string<charT, N>::basic_fixed_string(
    const charT (&a)[N + 1])
{
  for (int i = 0; i < N; i++) {
    data_[i] = a[i];
  }
  data_[N] = 0;
}

template<class charT, size_t N>
constexpr size_t basic_fixed_string<charT, N>::size() const noexcept
{
  return N;
}

template<class charT, size_t N>
constexpr const charT* basic_fixed_string<charT, N>::data() const noexcept
{
  return data_;
}

template<class charT, size_t N>
constexpr typename basic_fixed_string<charT, N>::const_reference
basic_fixed_string<charT, N>::operator[](size_type pos) const noexcept
{
  return data_[pos];
}

template<class charT, size_t N>
constexpr typename basic_fixed_string<charT, N>::reference
basic_fixed_string<charT, N>::operator[](size_type pos) noexcept
{
  return data_[pos];
}

template<class charT, size_t N, size_t M>
constexpr basic_fixed_string<charT, N + M> operator+(
    const basic_fixed_string<charT, N>& lhs,
    const basic_fixed_string<charT, M>& rhs) noexcept
{
  basic_fixed_string<charT, N + M> res;
  for (int i = 0; i < N; i++) {
    res[i] = lhs[i];
  }
  for (int j = 0; j < M; j++) {
    res[N + j] = rhs[j];
  }
  return res;
}

template<class charT, size_t N, size_t M>
constexpr basic_fixed_string<charT, N + M - 1> operator+(
    const basic_fixed_string<charT, N>& lhs, const charT (&rhs)[M]) noexcept
{
  basic_fixed_string<charT, M - 1> r(rhs);
  return lhs + r;
}

template<class charT, size_t N, size_t M>
constexpr basic_fixed_string<charT, N + M - 1> operator+(
    const charT (&lhs)[N], const basic_fixed_string<charT, M>& rhs) noexcept
{
  basic_fixed_string<charT, N - 1> l(lhs);
  return l + rhs;
}

template<class charT, size_t N>
std::ostream& operator<<(std::ostream& os,
                         const basic_fixed_string<charT, N>& str)
{
  os << str.data();
  return os;
}

template<class charT, size_t X>
constexpr basic_fixed_string<charT, X - 1> make_fixed_string(
    const charT (&a)[X]) noexcept
{
  basic_fixed_string<charT, X - 1> s {a};
  return s;
}

template<class charT>
constexpr size_t length(const charT* c_str) noexcept
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
  while (*s != '=')
    s++;
  return s + 2;
}

template<class T>
constexpr auto type_unique_name() noexcept
{
#ifdef __CPPLESS__
  constexpr auto name = __builtin_unique_stable_name(T);
  constexpr size_t l = length(name);
#else
  constexpr auto name = type_display_name<T>();
  constexpr size_t l = length(name) - 1;
#endif
  basic_fixed_string<char, l> s;
  for (int i = 0; i < length(name); i++) {
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
