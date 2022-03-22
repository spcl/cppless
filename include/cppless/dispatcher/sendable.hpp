#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cppless/cconv/cereal.hpp>
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

template<class Archive>
class sendable_lambda_base
{
public:
  virtual auto serialize(Archive& ar) -> void = 0;
  virtual auto identifier() -> std::string = 0;
  virtual ~sendable_lambda_base() = default;
};

template<class Lambda, class Res, class... Args>
class receivable_lambda
{
public:
  template<class Archive>
  auto serialize(Archive& ar) -> void
  {
    constexpr int capture_count = Lambda::capture_count();
    serialize_helper<Archive, Lambda, 0, capture_count>(ar, m_lambda);
  }
  Lambda m_lambda;
};

template<class Archive, class Lambda, class Res, class... Args>
class sendable_lambda : public sendable_lambda_base<Archive>
{
public:
  explicit sendable_lambda(const Lambda& l)
      : m_lambda(l)
  {
  }

  auto serialize(Archive& ar) -> void override
  {
    constexpr int capture_count = Lambda::capture_count();
    serialize_helper<Archive, Lambda, 0, capture_count>(ar, m_lambda);
  }

  auto identifier() -> std::string override
  {
    return function_identifier<Lambda, Args...>().str();
  }

  __attribute((entry))
  __attribute((meta(function_identifier<Lambda, Args...>()))) static auto
  main(int /*argc*/, char* /*argv*/[]) -> int
  {
    using recv = receivable_lambda<Lambda, Res, Args...>;
    union uninitialized_recv
    {
      recv m_self;
      std::aligned_storage_t<sizeof(recv), alignof(recv)> m_data;
      // Constructor
      uninitialized_recv()
          : m_data {}
      {
      }
      // Destructor
      ~uninitialized_recv()
      {
        m_self.~recv();
      }
    };
    cereal::JSONInputArchive iar(std::cin);
    uninitialized_recv u;
    std::tuple<Args...> s_args;
    task_data<recv, Args...> t_data {u.m_self, s_args};
    iar(t_data);
    Res res = std::apply(u.m_self.m_lambda, s_args);
    cereal::JSONOutputArchive oar(std::cout);
    oar(res);

    return 0;
  }

  Lambda m_lambda;
};

template<class Archive, class Res, class... Args>
class sendable_task
{
public:
  template<class Lambda>
  sendable_task(const Lambda& l)
      : m_base(
          std::make_unique<sendable_lambda<Archive, Lambda, Res, Args...>>(l))
  {
  }

  inline void serialize(Archive& ar)
  {
    m_base->serialize(ar);
  }

  [[nodiscard]] auto identifier() const -> std::string
  {
    return m_base->identifier();
  }

private:
  std::unique_ptr<sendable_lambda_base<Archive>> m_base;
};

}  // namespace cppless
