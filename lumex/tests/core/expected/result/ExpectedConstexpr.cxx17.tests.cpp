// What expected adds in a constant expression at C++17: lambdas and pointers
// to member functions in the monadic operations (constexpr lambdas, P0170),
// and, as at C++14, the trivial copy and move assignment (a union is copied as
// a whole, also when the other alternative is active). Assigning a value or an
// unexpected, emplace and swap start the lifetime of an alternative and need
// C++20; ExpectedConstexpr.cxx20 checks them. The static assertions are the
// test; the TEST bodies repeat the calls at run time and compare.

#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

namespace
{
using lumex::core::expected::result::expected;
using lumex::core::expected::result::in_place;
using lumex::core::expected::result::unexpect;

constexpr int kValue = 7;
constexpr int kError = 42;

using ex_t = expected<int, int>;

struct point_t
{
  int x;
  int y;

  constexpr int
  sum () const
  {
    return x + y;
  }
  constexpr expected<int, int>
  checked_sum () const
  {
    return expected<int, int> (x + y);
  }
};

constexpr expected<point_t, int> point_ex (in_place, point_t{ 3, 4 });
constexpr ex_t value_ex (kValue);
constexpr ex_t error_ex (unexpect, kError);

constexpr ex_t transformed
    = value_ex.transform ([] (int value) { return value * 3; });
constexpr ex_t chained
    = value_ex.and_then ([] (int value) { return ex_t (value + 100); });
constexpr ex_t recovered
    = error_ex.or_else ([] (int code) { return ex_t (code - kError); });
constexpr ex_t mapped
    = error_ex.transform_error ([] (int code) { return code * 2; });
constexpr expected<int, int> sum_by_pointer
    = point_ex.transform (&point_t::sum);
constexpr expected<int, int> checked_by_pointer
    = point_ex.and_then (&point_t::checked_sum);

static_assert (*transformed == kValue * 3, "transform with a lambda");
static_assert (*chained == kValue + 100, "and_then with a lambda");
static_assert (*recovered == 0, "or_else with a lambda");
static_assert (mapped.error () == kError * 2, "transform_error with a lambda");
static_assert (*sum_by_pointer == 7, "transform with a member function");
static_assert (*checked_by_pointer == 7, "and_then with a member function");

/// The trivial copy and move assignment, between two values and from an error
/// to a value and back.
constexpr int
assign_trivially ()
{
  ex_t uut (1);
  uut = ex_t (kValue);
  int sum = *uut;
  uut = error_ex;
  sum += uut.error ();
  uut = value_ex;
  uut = ex_t (unexpect, 3);
  return sum + uut.error ();
}

static_assert (assign_trivially () == kValue + kError + 3,
               "the trivial assignments");

constexpr int
chain_of_lambdas ()
{
  ex_t uut (2);
  return uut.transform ([] (int value) { return value + 1; })
      .and_then ([] (int value) { return ex_t (value * 10); })
      .or_else ([] (int) { return ex_t (0); })
      .value ();
}

static_assert (chain_of_lambdas () == 30, "a chain on a temporary");
} // namespace

TEST (ExpectedConstexprCxx17Test, ConstantResults_EqualTheRunTimeOnes)
{
  EXPECT_EQ (
      value_ex.transform ([] (int value) { return value * 3; }).value (),
      *transformed);
  EXPECT_EQ (point_ex.transform (&point_t::sum).value (), *sum_by_pointer);
  EXPECT_EQ (assign_trivially (), kValue + kError + 3);
  EXPECT_EQ (chain_of_lambdas (), 30);
}
