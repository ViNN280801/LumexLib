// Built with -fno-exceptions (see CMakeLists.txt): the same operations as
// main.cpp, nothing throws here.
#include <utility>

#include "lumex/core/expected/Expected"

#include "shared.hpp"

int
lib_exercise ()
{
  int bad = 0;
  {
    value_thrower_t x (lx::unexpect, 5);
    thrower_t source (7);
    x = std::move (source);
    bad += x.has_value () && x.value ().v == 7 ? 0 : 1;
    value_thrower_t y (lx::unexpect, 6);
    y = x;
    bad += y.has_value () && y.value ().v == 7 ? 0 : 1;
    value_thrower_t z (lx::unexpect, 8);
    z = std::move (y);
    bad += z.has_value () && z.value ().v == 7 ? 0 : 1;
    value_thrower_t a (lx::in_place, 3);
    value_thrower_t b (lx::unexpect, 4);
    a.swap (b);
    bad += !a.has_value () && a.error () == 4 && b.value ().v == 3 ? 0 : 1;
    b.swap (a);
    bad += a.has_value () && a.value ().v == 3 && !b.has_value () ? 0 : 1;
    swap (a, b);
    bad += !a.has_value () && a.error () == 4 && b.value ().v == 3 ? 0 : 1;
    swap (b, a);
    bad += a.has_value () && a.value ().v == 3 && !b.has_value () ? 0 : 1;
  }
  {
    error_thrower_t x (1);
    x.emplace_error (9);
    bad += !x.has_value () && x.error ().v == 9 ? 0 : 1;
    x.emplace_error (10);
    bad += !x.has_value () && x.error ().v == 10 ? 0 : 1;
    error_thrower_t y (2);
    error_thrower_t w (lx::unexpect, thrower_t (6));
    y.swap (w);
    bad += !y.has_value () && y.error ().v == 6 && w.value () == 2 ? 0 : 1;
    w.swap (y);
    bad += y.has_value () && y.value () == 2 && !w.has_value () ? 0 : 1;
    swap (y, w);
    bad += !y.has_value () && y.error ().v == 6 && w.value () == 2 ? 0 : 1;
    swap (w, y);
    bad += y.has_value () && y.value () == 2 && !w.has_value () ? 0 : 1;
    // An error over a value: the copy and the move assignment, the assignment
    // of an unexpected.
    error_thrower_t source (lx::unexpect, thrower_t (6));
    error_thrower_t target (3);
    target = source;
    bad += !target.has_value () && target.error ().v == 6 ? 0 : 1;
    error_thrower_t target2 (3);
    target2 = std::move (source);
    bad += !target2.has_value () && target2.error ().v == 6 ? 0 : 1;
    unexpected_thrower_t const bad_value (lx::in_place, 8);
    error_thrower_t target3 (3);
    target3 = bad_value;
    bad += !target3.has_value () && target3.error ().v == 8 ? 0 : 1;
    error_thrower_t target4 (3);
    target4 = unexpected_thrower_t (lx::in_place, 9);
    bad += !target4.has_value () && target4.error ().v == 9 ? 0 : 1;
  }
  {
    void_thrower_t x;
    x.emplace_error (9);
    bad += !x.has_value () && x.error ().v == 9 ? 0 : 1;
    void_thrower_t y;
    x.swap (y);
    bad += x.has_value () && !y.has_value () && y.error ().v == 9 ? 0 : 1;
    y.swap (x);
    bad += !x.has_value () && y.has_value () && x.error ().v == 9 ? 0 : 1;
    swap (x, y);
    bad += x.has_value () && !y.has_value () && y.error ().v == 9 ? 0 : 1;
    swap (y, x);
    bad += !x.has_value () && y.has_value () && x.error ().v == 9 ? 0 : 1;
  }
  return bad;
}
