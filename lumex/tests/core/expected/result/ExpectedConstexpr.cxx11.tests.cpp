// What expected can do in a constant expression at C++11: construction from a
// value, in place, from an error and from an unexpected, the observers on a
// const object, the comparisons, the monadic operations on a const object with
// a function object, and the trivial copy and move. A C++11 constexpr member
// function is implicitly const, and a constexpr function has a single return
// statement, so everything else (the observers of an object that is not const,
// the assignments, emplace, swap) waits for C++14 and C++20; the files
// ExpectedConstexpr.cxx14, .cxx17 and .cxx20 check each step. The static
// assertions are the test: they fail the build, and the TEST bodies repeat the
// same calls at run time and compare the results with the constant ones.

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

/// A literal class with two members.
struct point_t
{
  int x;
  int y;

  constexpr point_t (int a, int b) : x (a), y (b) {}
};

using ep_t = expected<point_t, int>;

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

struct fail_t
{
  constexpr ex_t
  operator() (int code) const
  {
    return ex_t (unexpect, code + 1);
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

struct void_ok_t
{
  constexpr ev_t
  operator() () const
  {
    return ev_t ();
  }
};

struct nothing_t
{
  constexpr int
  operator() () const
  {
    return 5;
  }
};

constexpr ex_t value_ex (kValue);
constexpr ex_t error_ex (unexpect, kError);
constexpr ex_t in_place_ex (in_place, kValue);
constexpr ex_t from_unexpected = ex_t (unexpected<int> (kError));
constexpr ex_t default_ex{};
constexpr ep_t point_ex (in_place, 3, 4);
constexpr ev_t ok_ev;
constexpr ev_t in_place_ev (in_place);
constexpr ev_t bad_ev (unexpect, kError);
constexpr ev_t from_unexpected_ev = ev_t (unexpected<int> (kError));

// Construction.
static_assert (value_ex.has_value (), "from a value");
static_assert (in_place_ex.has_value (), "in place");
static_assert (!error_ex.has_value (), "from unexpect");
static_assert (!from_unexpected.has_value (), "from an unexpected");
static_assert (default_ex.has_value () && *default_ex == 0,
               "the default constructor value-initializes the value");
static_assert (point_ex->x == 3 && point_ex->y == 4,
               "in place, two arguments");
static_assert (ok_ev.has_value () && in_place_ev.has_value (), "void success");
static_assert (!bad_ev.has_value () && bad_ev.error () == kError,
               "void failure");
static_assert (!from_unexpected_ev.has_value (), "void from an unexpected");

// Observers on a const object.
static_assert (static_cast<bool> (value_ex), "operator bool");
static_assert (!static_cast<bool> (error_ex), "operator bool");
static_assert (error_ex.has_error () && !value_ex.has_error (), "has_error");
static_assert (*value_ex == kValue, "operator*");
static_assert (value_ex.value () == kValue, "value");
static_assert (error_ex.error () == kError, "error");
static_assert (from_unexpected.error () == kError, "error of an unexpected");
static_assert (value_ex.value_or (1) == kValue, "value_or of a value");
static_assert (error_ex.value_or (1) == 1, "value_or of an error");
static_assert (value_ex.error_or (1) == 1, "error_or of a value");
static_assert (error_ex.error_or (1) == kError, "error_or of an error");
static_assert (ok_ev.error_or (1) == 1 && bad_ev.error_or (1) == kError,
               "error_or of void");

// Comparisons.
static_assert (value_ex == ex_t (kValue), "== another expected");
static_assert (value_ex != error_ex, "!= another expected");
static_assert (error_ex == ex_t (unexpect, kError), "== errors");
static_assert (value_ex == kValue && !(value_ex == kError), "== a value");
static_assert (kValue == value_ex, "a value == expected");
static_assert (error_ex == unexpected<int> (kError), "== an unexpected");
static_assert (error_ex != unexpected<int> (kValue), "!= an unexpected");
static_assert (ok_ev == ev_t () && ok_ev != bad_ev, "void comparisons");
static_assert (bad_ev == unexpected<int> (kError), "void == unexpected");

// Trivial copy and move.
constexpr ex_t copied (value_ex);
constexpr ex_t moved = ex_t (ex_t (kValue));
constexpr ev_t copied_ev (bad_ev);
static_assert (*copied == kValue && *moved == kValue, "copy and move");
static_assert (!copied_ev.has_value () && copied_ev.error () == kError,
               "copy of a void failure");
static_assert (std::is_trivially_copyable<ex_t>::value
                   && std::is_trivially_copyable<ev_t>::value,
               "so the copy above is the trivial one");

// Monadic operations on a const object.
constexpr ex_t transformed = value_ex.transform (add_one_t ());
constexpr ex_t transformed_error = error_ex.transform (add_one_t ());
constexpr ex_t chained = value_ex.and_then (twice_t ());
constexpr ex_t chained_error = error_ex.and_then (twice_t ());
constexpr ex_t recovered = error_ex.or_else (recover_t ());
constexpr ex_t kept = value_ex.or_else (recover_t ());
constexpr expected<int, int> mapped_error
    = error_ex.transform_error (code_plus_t ());
constexpr ev_t void_chained = ok_ev.and_then (void_ok_t ());
constexpr expected<int, int> void_transformed = ok_ev.transform (nothing_t ());
static_assert (*transformed == kValue + 1 && !transformed_error.has_value (),
               "transform");
static_assert (*chained == kValue * 2 && !chained_error.has_value (),
               "and_then");
static_assert (*recovered == kValue && *kept == kValue, "or_else");
static_assert (mapped_error.error () == kError + 1000, "transform_error");
static_assert (void_chained.has_value (), "and_then of void");
static_assert (*void_transformed == 5, "transform of void");
constexpr ex_t failed = value_ex.and_then (fail_t ());
static_assert (!failed.has_value () && failed.error () == kValue + 1,
               "and_then with a function that fails");
} // namespace

TEST (ExpectedConstexprCxx11Test, ConstantResults_EqualTheRunTimeOnes)
{
  ex_t const value (kValue);
  ex_t const error (unexpect, kError);
  ev_t const failure (unexpect, kError);

  EXPECT_EQ (*value.transform (add_one_t ()), *transformed);
  EXPECT_EQ (*value.and_then (twice_t ()), *chained);
  EXPECT_EQ (*error.or_else (recover_t ()), *recovered);
  EXPECT_EQ (value.and_then (fail_t ()).error (), failed.error ());
  EXPECT_EQ (error.transform_error (code_plus_t ()).error (),
             mapped_error.error ());
  EXPECT_EQ (value.value_or (1), kValue);
  EXPECT_EQ (error.value_or (1), 1);
  EXPECT_EQ (failure.error_or (1), kError);
  EXPECT_TRUE (value == value_ex);
  EXPECT_TRUE (error == error_ex);
  EXPECT_FALSE (value == error);
}

TEST (ExpectedConstexprCxx11Test, Failures_AreNotConstantButWork)
{
  // The unevaluated branches of the const observers throw or abort; a
  // constant expression never reaches them, a run does.
  ex_t const error (unexpect, kError);

  EXPECT_THROW (static_cast<void> (error.value ()),
                lumex::core::expected::error::bad_expected_access<int>);
}
