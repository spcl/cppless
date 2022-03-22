#pragma once

#include <type_traits>

#include "executor.hpp"

namespace cppless::execution
{
template<class Executor>
auto schedule(Executor& executor)
{
  return executor.schedule();
}

template<class RT, class... Ts>
struct filter_fn;

template<class RT>
struct filter_fn<RT>
{
  using type = std::function<RT()>;
};

template<class RT, class T, class... Ts>
struct filter_fn<RT, T, Ts...>
{
  template<class... Tail>
  struct cons;

  template<class... Tail>
  struct cons<void, std::function<RT(Tail...)>>
  {
    using type = void;
  };

  template<class Head, class... Tail>
  struct cons<Head, std::function<RT(Tail...)>>
  {
    using type = std::function<RT(Head, Tail...)>;
  };

  using type = typename std::conditional<
      std::is_void<T>::value,
      typename filter_fn<RT, Ts...>::type,
      typename cons<T, typename filter_fn<RT, Ts...>::type>::type>::type;
};

template<class Res, class... InputArgs>
auto then(sender<InputArgs>... inputs,
          typename filter_fn<Res, InputArgs...>::type f);

}  // namespace cppless::execution