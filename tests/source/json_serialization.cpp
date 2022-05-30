#include "./json_serialization.hpp"

#include <boost/ut.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/common.hpp>

void json_serialization_tests()
{
  using namespace boost::ut;

  "json_binary_archive"_test = []()
  {
    should("be invertible") = []
    {
      auto something = std::vector<unsigned int> {};
      const auto size = 10000;
      for (int i = 0; i < size; i++) {
        something.push_back(i);
      }
      auto encoded = cppless::json_binary_archive::serialize(something);

      std::vector<unsigned int> decoded;
      cppless::json_binary_archive::deserialize(encoded, decoded);

      expect(something == decoded);
    };

    should("encode to valid json") = []()
    {
      auto something = std::vector<unsigned int> {};
      const auto size = 10000;
      for (int i = 0; i < size; i++) {
        something.push_back(i);
      }
      auto encoded = cppless::json_binary_archive::serialize(something);
      expect(encoded.starts_with("\""));
      expect(encoded.ends_with("\""));
    };
  };

  "json_structured_archive"_test = []()
  {
    should("be invertible") = []
    {
      auto something = std::vector<unsigned int> {};
      const auto size = 10000;
      for (int i = 0; i < size; i++) {
        something.push_back(i);
      }
      auto encoded = cppless::json_structured_archive::serialize(something);
      std::vector<unsigned int> decoded;
      cppless::json_structured_archive::deserialize(encoded, decoded);

      expect(something == decoded);
    };
  };
}