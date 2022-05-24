#include <iostream>
#include <vector>

#include "../../include/benchmark.hpp"

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/common.hpp>

auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("serialization");
  benchmark::parse_args(program, argc, argv);

  auto something = std::vector<unsigned long long> {};
  const auto size = 1000000;
  for (unsigned int i = 0; i < size; i++) {
    something.push_back(i);
  }

  benchmark::benchmark("encode / binary") = [&](auto body)
  {
    body = [&]
    {
      auto encoded = cppless::json_binary_archive::serialize(something);
      benchmark::do_not_optimize(encoded);
    };
  };
  benchmark::benchmark("decode / binary") = [&](auto body)
  {
    auto encoded = cppless::json_binary_archive::serialize(something);
    body = [&]
    {
      auto decoded = cppless::json_binary_archive::deserialize<
          std::vector<unsigned long long>>(encoded);
      benchmark::do_not_optimize(decoded);
    };
  };

  benchmark::benchmark("encode / json") = [&](auto body)
  {
    body = [&]
    {
      auto encoded = cppless::json_structured_archive::serialize(something);
      benchmark::do_not_optimize(encoded);
    };
  };
  benchmark::benchmark("decode / json") = [&](auto body)
  {
    auto encoded = cppless::json_structured_archive::serialize(something);
    body = [&]
    {
      auto decoded = cppless::json_structured_archive::deserialize<
          std::vector<unsigned long long>>(encoded);
      benchmark::do_not_optimize(decoded);
    };
  };
}