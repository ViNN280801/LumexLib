// What expected adds in a constant expression at C++14, where a constexpr
// function may have several statements and change local objects (N3652) and a
// constexpr member function is no longer implicitly const: the observers and
// the monadic operations on an object that is not const, the observers of an
// rvalue, and the observers of expected<void, E> that return void. The
// operations that change the active alternative of the union (copy, move and
// converting construction of types that are not trivial, assignment, emplace,
// swap) are not constant expressions before C++20; ExpectedConstexpr.cxx20
// checks them. The static assertions are the test; the TEST bodies repeat the
// calls at run time and compare.

#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

namespace
{
using lumex::core::expected::error::unexpected;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::in_place;
using lumex::core::expected::result::unexpect;

constexpr int kValue = 7;
constexpr int kError = 42;

using ex_t = expected<int, int>;
using ev_t = expected<void, int>;

struct add_one_t
{
  constexpr int
  operator() (int value) const
  {
    return value + 1;
  }
};

struct twice_t
{
  constexpr ex_t
  operator() (int value) const
  {
    return ex_t (value * 2);
  }
};

struct recover_t
{
  constexpr ex_t
  operator() (int) const
  {
    return ex_t (kValue);
  }
};

struct code_plus_t
{
  constexpr int
  operator() (int code) const
  {
    return code + 1000;
  }
};

/// Writes through operator*, value () and operator->.
constexpr int
write_through_observers ()
{
  expected<std::pair<int, int>, int> uut (in_place, 1, 2);
  uut->first = 10;
  (*uut).second = 20;
  uut.value ().first += 1;
  return uut->first + uut->second;
}

constexpr int
move_the_value_out ()
{
  ex_t uut (kValue);
  return static_cast<ex_t &&> (uut).value () + *static_cast<ex_t &&> (uut);
}

constexpr int
read_the_error_of_an_object_that_is_not_const ()
{
  ex_t uut (unexpect, kError);
  uut.error () += 1;
  return uut.error () + static_cast<ex_t &&> (uut).error ();
}

constexpr int
non_const_monadic ()
{
  ex_t uut (kValue);
  return uut.transform (add_one_t ()).value ()
         + uut.and_then (twice_t ()).value ()
         + static_cast<ex_t &&> (uut).transform (add_one_t ()).value ();
}

constexpr int
recover_from_an_error ()
{
  ex_t uut (unexpect, kError);
  return uut.or_else (recover_t ()).value ()
         + uut.transform_error (code_plus_t ()).error ();
}

constexpr int
value_or_of_an_rvalue ()
{
  return ex_t (unexpect, kError).value_or (5) + ex_t (kValue).value_or (5);
}

constexpr int
error_or_of_an_rvalue ()
{
  return ex_t (kValue).error_or (5) + ex_t (unexpect, kError).error_or (5);
}

constexpr int
void_observers ()
{
  ev_t ok;
  *ok;
  ok.value ();
  ev_t failed (unexpect, kError);
  failed.error () += 1;
  return failed.error () + static_cast<ev_t &&> (failed).error ();
}

constexpr bool
void_value_of_a_success ()
{
  static_cast<ev_t &&> (ev_t ()).value ();
  return true;
}

static_assert (write_through_observers () == 31, "the observers write");
static_assert (move_the_value_out () == 2 * kValue, "value () && and *&&");
static_assert (read_the_error_of_an_object_that_is_not_const ()
                   == 2 * (kError + 1),
               "error () on an object that is not const");
static_assert (non_const_monadic ()
                   == (kValue + 1) + 2 * kValue + (kValue + 1),
               "the monadic operations on an object that is not const");
static_assert (recover_from_an_error () == kValue + kError + 1000,
               "or_else and transform_error");
static_assert (value_or_of_an_rvalue () == 5 + kValue, "value_or &&");
static_assert (error_or_of_an_rvalue () == 5 + kError, "error_or &&");
static_assert (void_observers () == 2 * (kError + 1), "the void observers");
static_assert (void_value_of_a_success (), "value () && of a void success");
} // namespace

TEST (ExpectedConstexprCxx14Test, ConstantResults_EqualTheRunTimeOnes)
{
  EXPECT_EQ (write_through_observers (), 31);
  EXPECT_EQ (move_the_value_out (), 2 * kValue);
  EXPECT_EQ (read_the_error_of_an_object_that_is_not_const (),
             2 * (kError + 1));
  EXPECT_EQ (non_const_monadic (), (kValue + 1) + 2 * kValue + (kValue + 1));
  EXPECT_EQ (recover_from_an_error (), kValue + kError + 1000);
  EXPECT_EQ (value_or_of_an_rvalue (), 5 + kValue);
  EXPECT_EQ (error_or_of_an_rvalue (), 5 + kError);
  EXPECT_EQ (void_observers (), 2 * (kError + 1));
  EXPECT_TRUE (void_value_of_a_success ());
}
