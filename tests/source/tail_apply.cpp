#include "./tail_apply.hpp"

#include <boost/ut.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/utils/apply.hpp>

#include "cppless/utils/apply.hpp"

void tail_apply_tests()
{
  using namespace boost::ut;

  "tail_apply"_test = []
  {
    should("work for no arguments") = []
    {
      int x = 0;
      auto fn = [&] { x = 1; };
      cppless::tail_apply(fn);

      expect(x == 1_ul);
    };

    should("work for one arguments") = []
    {
      int x = 0;
      auto fn = [&](int y) { x = y; };
      cppless::tail_apply(fn, 1);

      expect(x == 1_ul);
    };

    should("swap two arguments") = []
    {
      int x = 0;
      auto fn = [&](int a, int b) { x = a - b; };
      cppless::tail_apply(fn, 2, 3);

      expect(x == 1_ul);
    };

    should("reorder arguments correctly") = []
    {
      int ax = 0;
      int bx = 0;
      int cx = 0;
      auto fn = [&](int a, int b, int c)
      {
        ax = a;
        bx = b;
        cx = c;
      };
      cppless::tail_apply(fn, 1, 2, 3);

      expect(ax == 3_ul);
      expect(bx == 1_ul);
      expect(cx == 2_ul);
    };
  };
}