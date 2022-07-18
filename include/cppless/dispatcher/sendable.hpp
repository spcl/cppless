#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cppless/detail/deduction.hpp>
#include <cppless/utils/cereal.hpp>
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
  auto operator()(Args... args) -> Res
  {
    return m_lambda(args...);
  }

private:
  Lambda m_lambda;
};

template<class Dispatcher>
class task_base
{
  using input_archive = typename Dispatcher::request_input_archive;
  using output_archive = typename Dispatcher::request_output_archive;

public:
  virtual auto serialize(output_archive& ar) -> void = 0;
  virtual auto identifier() -> std::string = 0;
  virtual ~task_base() = default;
};

template<class Dispatcher, class T>
class task;

template<class Dispatcher, class Res, class... Args>
class task<Dispatcher, Res(Args...)>
{
  using input_archive = typename Dispatcher::request_input_archive;
  using output_archive = typename Dispatcher::request_output_archive;

public:
  using args = std::tuple<Args...>;
  using res = Res;

  explicit task(std::unique_ptr<task_base<Dispatcher>> base)
      : m_base(std::move(base))
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
  std::unique_ptr<task_base<Dispatcher>> m_base;
};

template<class Dispatcher, class Config, class Lambda, class T>
class lambda_task;

template<class Dispatcher, class Config, class Lambda, class Res, class... Args>
class lambda_task<Dispatcher, Config, Lambda, Res(Args...)>
    : public task_base<Dispatcher>
{
  using input_archive = typename Dispatcher::request_input_archive;
  using output_archive = typename Dispatcher::request_output_archive;

public:
  explicit lambda_task(const Lambda& l)
      : m_lambda(l)
  {
  }

  auto serialize(output_archive& ar) -> void override
  {
    constexpr int capture_count = Lambda::capture_count();
    if constexpr (capture_count > 0) {
      serialize_helper<output_archive, Lambda, 0, capture_count>(ar, m_lambda);
    }
  }

  auto identifier() -> std::string override
  {
    return Dispatcher::template meta_serializer<Config>::identifier(
        function_identifier<Lambda, Args...>().str());
  }

  __attribute((entry)) __attribute((
      meta(Dispatcher::template meta_serializer<Config>::template serialize<
           function_identifier<Lambda, Args...>().size() + 1>(
          function_identifier<Lambda, Args...>())))) static auto
  main(int argc, char* argv[]) -> int
  {
    return Dispatcher::template main<Lambda,
                                     receivable_lambda<Lambda, Res, Args...>,
                                     Res,
                                     Args...>(argc, argv);
  }

private:
  Lambda m_lambda;
};

template<class Dispatcher, class Config = typename Dispatcher::default_config>
struct lambda_task_factory
{
  template<class Lambda>
  static auto create(Lambda l)
  {
    using fn_type =
        typename detail::deduce_function<decltype(&Lambda::operator())>::type;
    std::unique_ptr<task_base<Dispatcher>> task_ptr =
        std::make_unique<lambda_task<Dispatcher, Config, Lambda, fn_type>>(l);
    task<Dispatcher, fn_type> t(std::move(task_ptr));
    return t;
  }
};

}  // namespace cppless
