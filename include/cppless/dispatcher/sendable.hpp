#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cppless/cconv/cereal.hpp>
#include <cppless/detail/deduction.hpp>
#include <cppless/utils/fixed_string.hpp>

namespace cppless
{

template<class L, class... Args>
constexpr auto function_identifier()
{
  constexpr auto prefix = make_fixed_string(__BASE_FILE__);
  constexpr auto suffix = type_unique_name<L>();
  return prefix + "@" + suffix + "<" + types_unique_names<Args...>() + ">";
}

template<class Archive, class Lambda, int X, int L, class... Args>
inline void serialize_helper(Archive& ar, Lambda& l, const Args&... args)
{
  if constexpr (X == L) {
    ar(args...);
  } else {
    auto& arg = l.template capture<X>();
    serialize_helper<Archive, Lambda, X + 1, L, Args..., decltype(arg)>(
        ar, l, args..., arg);
  }
}

template<class Lambda, class Res, class... Args>
class receivable_lambda
{
public:
  template<class Archive>
  auto serialize(Archive& ar) -> void
  {
    constexpr int capture_count = Lambda::capture_count();
    if constexpr (capture_count > 0) {
      serialize_helper<Archive, Lambda, 0, capture_count>(ar, m_lambda);
    }
  }
  Lambda m_lambda;
};

template<class Dispatcher>
struct task
{
  using input_archive = typename Dispatcher::input_archive;
  using output_archive = typename Dispatcher::output_archive;

  class sendable_lambda_base
  {
  public:
    virtual auto serialize(output_archive& ar) -> void = 0;
    virtual auto identifier() -> std::string = 0;
    virtual ~sendable_lambda_base() = default;
  };

  template<class Lambda, class Res, class... Args>
  class sendable_lambda : public sendable_lambda_base
  {
  public:
    explicit sendable_lambda(const Lambda& l)
        : m_lambda(l)
    {
    }

    auto serialize(output_archive& ar) -> void override
    {
      constexpr int capture_count = Lambda::capture_count();
      if constexpr (capture_count > 0) {
        serialize_helper<output_archive, Lambda, 0, capture_count>(ar,
                                                                   m_lambda);
      }
    }

    auto identifier() -> std::string override
    {
      return function_identifier<Lambda, Args...>().str();
    }

    __attribute((entry))
    __attribute((meta(function_identifier<Lambda, Args...>()))) static auto
    main(int argc, char* argv[]) -> int
    {
      return Dispatcher::template main<Lambda, Res, Args...>(argc, argv);
    }

    Lambda m_lambda;
  };

  template<class T>
  class sendable;

  template<class Res, class... Args>
  class sendable<Res(Args...)>
  {
  public:
    using args = std::tuple<Args...>;
    using res = Res;

    template<class Lambda>
    sendable(const Lambda& l)
        : m_base(std::make_unique<sendable_lambda<Lambda, Res, Args...>>(l))
    {
    }

    inline void serialize(output_archive& ar)
    {
      m_base->serialize(ar);
    }

    [[nodiscard]] auto identifier() const -> std::string
    {
      return m_base->identifier();
    }

  private:
    std::unique_ptr<sendable_lambda_base> m_base;
  };

  template<class F, class Op = decltype(&F::operator())>
  sendable(F) -> sendable<cppless::detail::deduce_function_t<Op>>;
};

}  // namespace cppless
