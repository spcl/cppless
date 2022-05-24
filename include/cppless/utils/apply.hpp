#pragma once

#include <tuple>

namespace cppless
{

template<class Tuple, std::size_t... Is>
auto rotate_tuple_impl(Tuple t, std::index_sequence<Is...> /*unused*/)
{
  constexpr auto length = std::tuple_size<Tuple>();
  return std::make_tuple(std::get<length - 1>(t), std::get<Is>(t)...);
}

template<class... Args>
auto rotate_tuple(std::tuple<Args...> t)
{
  if constexpr (sizeof...(Args) == 0) {
    return std::tuple<> {};
  } else {
    return rotate_tuple_impl(t,
                             std::make_index_sequence<sizeof...(Args) - 1> {});
  }
}

template<class B, class Tuple, std::size_t... Is>
auto apply_impl(B b, Tuple t, std::index_sequence<Is...> /*unused*/)
{
  return b(std::get<Is>(t)...);
}

/**
 * @brief Invokes `b` with the first argument being the argument passed last to
 * the call to `tail_apply` and the rest of the arguments in their order as
 * passed to `tail_apply`.
 *
 * @tparam B - The type of the function to invoke.
 * @tparam Args - The types of the arguments to pass to `b`, in the order as
 * passed to `tail_apply`
 * @param b - The invocable to invoke.
 * @param args - The arguments to pass to `b`
 * @return auto - The result of invoking `b` with the changed argument order.
 */
template<class B, class... Args>
auto inline tail_apply(B&& b, Args&&... args)
{
  auto tuple = std::make_tuple(std::forward<Args>(args)...);
  auto rotated = rotate_tuple(tuple);
  return apply_impl(b, rotated, std::index_sequence_for<Args...> {});
}

}  // namespace cppless