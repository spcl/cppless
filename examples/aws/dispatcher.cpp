#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/details/helpers.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/utils/tracing.hpp>
#include <nlohmann/json.hpp>

#include "cppless/detail/deduction.hpp"

using dispatcher = cppless::aws_lambda_beast_dispatcher<>::from_env;

auto do_task(int i, cppless::tracing_span_ref span) -> int
{
  if (i <= 1) {
    return i;
  }
  {
    dispatcher aws;

    auto t0 = [=](int i)
    {
      cppless::tracing_span_container spans;
      auto root = spans.create_root("root").start();
      auto value = do_task(i, root);
      root.end();
      auto end = std::chrono::steady_clock::now();
      return std::make_tuple(value, spans, root.id(), end);
    };
    using return_type =
        cppless::detail::function_res<cppless::detail::deduce_function<
            decltype(&decltype(t0)::operator())>::type>::type;
    return_type a_data;
    return_type b_data;

    auto instance = aws.create_instance();

    auto a_span = span.create_child("a").start();
    auto b_span = span.create_child("b").start();

    int task_a = cppless::dispatch(instance,
                                   t0,
                                   a_data,
                                   std::make_tuple(i - 1),
                                   a_span.create_child("dispatch"));
    cppless::dispatch(instance,
                      t0,
                      b_data,
                      std::make_tuple(i - 2),
                      b_span.create_child("dispatch"));

    auto fst_id = instance.wait_one();
    auto fst_end = std::chrono::steady_clock::now();
    if (std::get<0>(fst_id) == task_a) {
      a_span.end();
    } else {
      b_span.end();
    }
    instance.wait_one();
    if (std::get<0>(fst_id) == task_a) {
      b_span.end();
    } else {
      a_span.end();
    }

    auto snd_end = std::chrono::steady_clock::now();
    auto a_end = std::get<0>(fst_id) == task_a ? fst_end : snd_end;
    auto b_end = std::get<0>(fst_id) == task_a ? snd_end : fst_end;

    int a_value = std::get<0>(a_data);
    std::chrono::duration<long long, std::nano> a_clk_diff =
        a_end - std::get<3>(a_data);
    a_span.create_child("remote").insert(
        std::get<1>(a_data), std::get<2>(a_data), a_clk_diff);

    int b_value = std::get<0>(b_data);
    std::chrono::duration<long long, std::nano> b_clk_diff =
        b_end - std::get<3>(b_data);
    b_span.create_child("remote").insert(
        std::get<1>(b_data), std::get<2>(b_data), b_clk_diff);

    return a_value + b_value;
  }
}

__attribute((weak)) auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  cppless::tracing_span_container spans;
  auto root = spans.create_root("root");

  std::cout << do_task(5, root) << std::endl;

  nlohmann::json j = spans;
  std::cout << j.dump(2) << std::endl;
}
