// A type whose constructors may throw, used by both units. The text of every
// inline function is the same in both: the throw goes through a function that
// only main.cpp (the unit with exceptions) defines. The counters are defined
// in main.cpp.
#ifndef LUMEX_TESTS_CMAKE_EXPECTED_MIXED_STRONG_GUARANTEE_SHARED_HPP
#define LUMEX_TESTS_CMAKE_EXPECTED_MIXED_STRONG_GUARANTEE_SHARED_HPP

#include "lumex/core/expected/Expected"

// Throws. Defined in main.cpp.
[[noreturn]] void shared_fail ();

// The live objects, the objects destroyed twice, and how many constructions
// are left before one throws (0: none does).
extern int g_live;
extern int g_destroyed_twice;
extern int g_countdown;

inline void
maybe_fail ()
{
  if (g_countdown > 0 && --g_countdown == 0)
    shared_fail ();
}

// Not nothrow constructible, not nothrow movable: the operations that put the
// old contents back take their guarded paths.
struct thrower_t
{
  int v;
  int state;

  explicit thrower_t (int x) noexcept (false) : v (x), state (1)
  {
    maybe_fail ();
    ++g_live;
  }
  thrower_t (thrower_t const &other) noexcept (false) : v (other.v), state (1)
  {
    maybe_fail ();
    ++g_live;
  }
  thrower_t (thrower_t &&other) noexcept (false) : v (other.v), state (1)
  {
    maybe_fail ();
    ++g_live;
  }
  thrower_t &
  operator= (thrower_t const &other) noexcept (false)
  {
    v = other.v;
    return *this;
  }
  thrower_t &
  operator= (thrower_t &&other) noexcept (false)
  {
    v = other.v;
    return *this;
  }
  ~thrower_t ()
  {
    if (state != 1)
      ++g_destroyed_twice;
    state = 2;
    --g_live;
  }
};

namespace lx = lumex::core::expected::result;
namespace le = lumex::core::expected::error;

using value_thrower_t = lx::expected<thrower_t, int>;
using error_thrower_t = lx::expected<int, thrower_t>;
using void_thrower_t = lx::expected<void, thrower_t>;
using unexpected_thrower_t = le::unexpected<thrower_t>;

// Every operation without a throw, from the unit without exceptions (the free
// swap is called unqualified, found by argument-dependent lookup): the return
// value is the number of results that were not as expected.
int lib_exercise ();

#endif
