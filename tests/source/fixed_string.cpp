#include "./fixed_string.hpp"

#include <boost/ut.hpp>
#include <cppless/utils/fixed_string.hpp>

void tail_apply_tests()
{
  using namespace boost::ut;

  "tail_apply"_test = []
  {
    should("work for no arguments") = [] {

    };
  };
}