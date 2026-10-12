// What expected adds in a constant expression at C++20: every operation that
// starts or ends the lifetime of an alternative of its union, because a
// constant expression may now change the active member of a union (P1330),
// call std::construct_at (P0784), end an object by a constexpr destructor
// (P0784) and contain a try block (P1002). So a type that is not trivial
// works: its copy, move and converting constructors, the assignments of all
// kinds (the strong guarantee included), emplace and swap run at compile time,
// and so does the destructor. The static assertions are the test; the TEST
// bodies repeat the calls at run time and compare. A compiler that calls its
// mode C++20 but lacks those features (GCC 8 with -std=c++2a) skips the file.

#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

#if LUMEX_FEATURE_CONSTEXPR >= 201907L && LUMEX_HAS_STD_CONSTEXPR_DYNAMIC_ALLOC

namespace
{
using lumex::core::expected::error::unexpected;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::in_place;
using lumex::core::expected::result::unexpect;

/// A literal type that is not trivial: it has a destructor, a copy and a
/// move of its own, all constexpr and noexcept.
struct box_t
{
  int value;

  constexpr box_t (int v) noexcept : value (v) {}
  constexpr box_t (box_t const &other) noexcept : value (other.value) {}
  constexpr box_t (box_t &&other) noexcept : value (other.value)
  {
    other.value = -1;
  }
  constexpr box_t &
  operator= (box_t const &other) noexcept
  {
    value = other.value;
    return *this;
  }
  constexpr box_t &
  operator= (box_t &&other) noexcept
  {
    value = other.value;
    other.value = -1;
    return *this;
  }
  constexpr ~box_t () {}
};

/// The same, but building one may throw: reinit-expected has to take its
/// second branch, a temporary, to keep the old contents.
struct fragile_t
{
  int value;

  constexpr fragile_t (int v) : value (v) {}
  constexpr fragile_t (fragile_t const &other) : value (other.value) {}
  constexpr fragile_t (fragile_t &&other) noexcept : value (other.value) {}
  constexpr fragile_t &
  operator= (fragile_t const &other)
  {
    value = other.value;
    return *this;
  }
  constexpr fragile_t &
  operator= (fragile_t &&other) noexcept
  {
    value = other.value;
    return *this;
  }
  constexpr ~fragile_t () {}
};

using eb_t = expected<box_t, int>;
using be_t = expected<int, box_t>;
using vb_t = expected<void, box_t>;
using ei_t = expected<int, int>;

static_assert (!std::is_trivially_copyable<eb_t>::value,
               "the value has a destructor of its own");

constexpr int
copy_and_move_construct ()
{
  eb_t original (box_t (4));
  eb_t copied (original);
  eb_t moved (std::move (original));
  eb_t failed (unexpect, 9);
  eb_t copied_error (failed);
  return copied->value + moved->value + copied_error.error ()
         + (original->value == -1 ? 100 : 0);
}

constexpr int
converting_construct ()
{
  expected<int, int> narrow (5);
  expected<long, long> wide (narrow);
  expected<long, long> moved (expected<int, int> (unexpect, 6));
  return static_cast<int> (*wide) + static_cast<int> (moved.error ());
}

constexpr int
assign_all_ways ()
{
  eb_t uut (box_t (1));
  eb_t other (box_t (2));
  uut = other;                // value <- value
  int sum = uut->value;       // 2
  uut = eb_t (unexpect, 10);  // error <- move of an error
  sum += uut.error ();        // 12
  uut = other;                // error -> value, copy
  sum += uut->value;          // 14
  uut = unexpected<int> (20); // value -> error
  sum += uut.error ();        // 34
  uut = box_t (3);            // error -> value, from a value
  sum += uut->value;          // 37
  uut = box_t (4);            // value <- value, from a value
  sum += uut->value;          // 41
  return sum;
}

constexpr int
assign_with_a_throwing_constructor ()
{
  expected<fragile_t, int> uut (unexpect, 5);
  uut = fragile_t (6); // the temporary branch of reinit-expected
  int sum = uut->value;
  uut = unexpected<int> (7); // the move of E is nothrow, a plain int
  return sum + uut.error ();
}

constexpr int
emplace_and_swap ()
{
  eb_t uut (unexpect, 3);
  uut.emplace (8);
  int sum = uut->value; // 8
  eb_t other (unexpect, 4);
  uut.swap (other);    // value with error
  sum += uut.error (); // 12
  sum += other->value; // 20
  swap (uut, other);   // error with value, the free function
  sum += uut->value;   // 28
  uut.emplace_error (30);
  return sum;
}

constexpr int
swap_the_other_way ()
{
  be_t a (4);
  be_t b (unexpect, box_t (5));
  a.swap (b);
  int sum = a.error ().value * 10 + *b;
  a.swap (b);
  return sum + *a;
}

constexpr int
void_operations ()
{
  vb_t uut;
  uut = vb_t (unexpect, box_t (3));
  int sum = uut.error ().value;
  vb_t other;
  uut.swap (other);
  sum += other.error ().value;
  other.emplace ();
  uut = unexpected<box_t> (box_t (4));
  sum += uut.error ().value;
  uut.emplace_error (5);
  vb_t copied (uut);
  return sum + copied.error ().value + (other.has_value () ? 1000 : 0);
}

constexpr int
trivial_types_too ()
{
  ei_t uut (1);
  uut.emplace (2);
  uut = 3;
  uut = unexpected<int> (4);
  ei_t other (5);
  uut.swap (other);
  return uut.value () * 10 + other.error ();
}

static_assert (copy_and_move_construct () == 4 + 4 + 9 + 100,
               "copy and move of a type that is not trivial");
static_assert (converting_construct () == 11, "converting constructors");
static_assert (assign_all_ways () == 41, "every assignment");
static_assert (assign_with_a_throwing_constructor () == 13,
               "reinit-expected with a constructor that may throw");
static_assert (emplace_and_swap () == 28, "emplace, swap and the free swap");
static_assert (swap_the_other_way () == 58, "swap with the error alternative");
static_assert (void_operations () == 3 + 3 + 4 + 5 + 1000,
               "the same for expected<void, E>");
static_assert (trivial_types_too () == 54, "trivial types in the same way");
} // namespace

TEST (ExpectedConstexprCxx20Test, ConstantResults_EqualTheRunTimeOnes)
{
  EXPECT_EQ (copy_and_move_construct (), 4 + 4 + 9 + 100);
  EXPECT_EQ (converting_construct (), 11);
  EXPECT_EQ (assign_all_ways (), 41);
  EXPECT_EQ (assign_with_a_throwing_constructor (), 13);
  EXPECT_EQ (emplace_and_swap (), 28);
  EXPECT_EQ (swap_the_other_way (), 58);
  EXPECT_EQ (void_operations (), 3 + 3 + 4 + 5 + 1000);
  EXPECT_EQ (trivial_types_too (), 54);
}

#else

TEST (ExpectedConstexprCxx20Test, ConstantResults_EqualTheRunTimeOnes)
{
  GTEST_SKIP () << "the compiler has no constexpr lifetime of union members "
                   "(P0784, P1330, P1002)";
}

#endif
