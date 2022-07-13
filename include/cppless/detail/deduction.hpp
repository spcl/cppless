#pragma once
#include <tuple>
namespace cppless::detail
{
/**
 * @brief Determines the canonical function type of a function, void if the
 * function is not callable
 *
 * @tparam T The type of the function or method
 */
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

template<typename X>
struct function_args;

template<typename R, typename... T>
struct function_args<R(T...)>
{
  using type = std::tuple<T...>;
};

template<typename X>
struct function_res;

template<typename R, typename... T>
struct function_res<R(T...)>
{
  using type = R;
};

}  // namespace cppless::detail