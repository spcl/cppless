#pragma once
#include <tuple>

template<int I, int L, class Lambda, class T, class... Elements>
inline auto reduce_helper(std::tuple<Elements...> tuple, Lambda&& lambda, T&& t)
{
  if constexpr (I == L) {
    return t;
  } else {
    return reduce_helper<I + 1, L, Lambda, T, Elements...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::forward<T>(lambda(std::forward<T>(t), std::get<I>(tuple))));
  }
}

template<class Lambda, class T, class... Elements>
auto reduce_tuple(std::tuple<Elements...> tuple, Lambda&& lambda, T&& base)
{
  return reduce_helper<0, sizeof...(Elements), Lambda, T, Elements...>(
      tuple, std::forward<Lambda>(lambda), std::forward<T>(base));
}

template<int I, int L, class Lambda, class Tuple, class... Elements>
inline auto map_helper(Tuple tuple, Lambda&& lambda, Elements&&... elements)
{
  if constexpr (I == L) {
    return std::make_tuple(std::forward<Elements>(elements)...);
  } else {
    return map_helper<I + 1,
                      L,
                      Lambda,
                      Tuple,
                      Elements...,
                      decltype(lambda(std::get<I>(tuple)))>(
        tuple,
        std::forward<Lambda>(lambda),
        std::forward<Elements>(elements)...,
        std::forward<Lambda>(lambda)(std::get<I>(tuple)));
  }
}

/**
 * @brief Maps the elements of a tuple to a new tuple.
 *
 * @tparam Lambda - The type of the function that is used for mapping.
 * @tparam Elements - The elements of the tuple to map.
 * @param tuple - The tuple to map.
 * @param lambda - The invocable to map the elements with.
 * @return auto - The mapped tuple.
 */
template<class Lambda, class... Elements>
auto map_tuple(std::tuple<Elements...> tuple, Lambda&& lambda)
{
  return map_helper<0, sizeof...(Elements), Lambda>(
      tuple, std::forward<Lambda>(lambda));
}

template<int I, int L, class Lambda, class Tuple>
inline auto fill_helper(Tuple& tuple, Lambda&& lambda)
{
  if constexpr (I < L) {
    lambda(std::get<I>(tuple));
    fill_helper<I + 1, L, Lambda, Tuple>(tuple, std::forward<Lambda>(lambda));
  }
}
/**
 * @brief Fills a tuple by supplying a function with l-values to each element.
 *
 * @tparam Lambda - The type of the function that is used for filling.
 * @tparam Elements - The elements of the tuple to fill.
 * @param tuple - The tuple to fill.
 * @param lambda - The invocable to fill the elements with.
 * @return Nothing
 */
template<class Lambda, class... Elements>
auto fill_tuple(std::tuple<Elements...>& tuple, Lambda&& lambda) -> void
{
  fill_helper<0, sizeof...(Elements), Lambda>(tuple,
                                              std::forward<Lambda>(lambda));
}