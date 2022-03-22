#pragma once
namespace cppless::detail
{
template<typename T>
struct deduce_function
{
  using type = void;
};
template<typename T>
using deduce_function_t = typename deduce_function<T>::type;

template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...)>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) volatile>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const volatile>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...)&>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const&>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) volatile&>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const volatile&>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) noexcept>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const noexcept>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) volatile noexcept>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const volatile noexcept>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...)& noexcept>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const& noexcept>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) volatile& noexcept>
{
  using type = R(Args...);
};
template<typename G, typename R, typename... Args>
struct deduce_function<R (G::*)(Args...) const volatile& noexcept>
{
  using type = R(Args...);
};
}  // namespace cppless::detail