#include <iostream>
#include <sstream>
#include <vector>

#include "../../include/benchmark.hpp"

#include <argparse/argparse.hpp>
#include <boost/ut.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/common.hpp>

class some_data
{
public:
  unsigned int x;
  unsigned int y;
  std::string q;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(x, y, q);
  }
};

auto main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("serialization");
  benchmark::parse_args(program, argc, argv);

  auto something = std::vector<some_data> {};
  const auto size = 1000000;
  for (unsigned int i = 0; i < size; i++) {
    something.push_back({.x = i, .y = i - 1, .q = "hello" + std::to_string(i)});
  }

  benchmark::benchmark("encode / binary") = [&](auto body)
  {
    body = [&]
    {
      std::stringstream ss;
      {
        cereal::BinaryOutputArchive oar(ss);
        oar(something);
      }
      auto encoded = ss.str();
      benchmark::do_not_optimize(encoded);
    };
  };
  benchmark::benchmark("decode / binary") = [&](auto body)
  {
    std::stringstream ss;
    {
      cereal::BinaryOutputArchive oar(ss);
      oar(something);
    }
    auto encoded = ss.str();
    body = [&]
    {
      std::stringstream ss(encoded);
      std::vector<some_data> decoded;
      {
        cereal::BinaryInputArchive iar(ss);
        iar(decoded);
      }
      benchmark::do_not_optimize(decoded);
    };
  };

  benchmark::benchmark("encode / binary_json") = [&](auto body)
  {
    body = [&]
    {
      auto encoded = cppless::json_binary_archive::serialize(something);
      benchmark::do_not_optimize(encoded);
    };
  };
  benchmark::benchmark("decode / binary_json") = [&](auto body)
  {
    auto encoded = cppless::json_binary_archive::serialize(something);
    body = [&]
    {
      auto decoded =
          cppless::json_binary_archive::deserialize<std::vector<some_data>>(
              encoded);
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
      auto decoded =
          cppless::json_structured_archive::deserialize<std::vector<some_data>>(
              encoded);
      benchmark::do_not_optimize(decoded);
    };
  };
}