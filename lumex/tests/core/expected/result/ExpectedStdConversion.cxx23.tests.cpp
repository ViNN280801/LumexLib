// The implicit conversions between expected<T, E> of this library and
// std::expected<T, E> of the standard library, for exactly the same T and E
// (ExpectedDetail.hpp: LUMEX_EXPECTED_HAS_STD_CONVERSION, C++23). The tests
// pin: what converts (both ways, implicitly, value and error, `void`, a
// move-only type), what does not (any other T or E, which the constraints
// leave out), what is copied and what is moved, the exception specification,
// the choice of an overload that has both, and that the converted object
// answers every question as the std::expected it came from or went to.

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

#if __has_include(<expected>)
#include <expected>
#endif

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedParitySupport.hpp"

#if defined(__cpp_lib_expected)

namespace
{
namespace lx = lumex::core::expected;
using namespace expected_parity;

template <typename T, typename E> using lex = lx::result::expected<T, E>;
template <typename T, typename E> using sex = std::expected<T, E>;

// ---------------------------------------------------------------------
// What converts, at compile time
// ---------------------------------------------------------------------

static_assert (LUMEX_EXPECTED_HAS_STD_CONVERSION == 1,
               "the conversions exist where std::expected does");

static_assert (
    std::is_convertible<sex<int, unsigned>, lex<int, unsigned>>::value,
    "std to lumex, implicit");
static_assert (
    std::is_convertible<lex<int, unsigned>, sex<int, unsigned>>::value,
    "lumex to std, implicit");
static_assert (
    std::is_convertible<sex<int, unsigned> &, lex<int, unsigned>>::value,
    "std lvalue to lumex");
static_assert (
    std::is_convertible<sex<int, unsigned> const &, lex<int, unsigned>>::value,
    "std const lvalue to lumex");
static_assert (
    std::is_convertible<lex<int, unsigned> &, sex<int, unsigned>>::value,
    "lumex lvalue to std");
static_assert (
    std::is_convertible<lex<int, unsigned> const &, sex<int, unsigned>>::value,
    "lumex const lvalue to std");
static_assert (std::is_convertible<sex<std::string, std::string>,
                                   lex<std::string, std::string>>::value,
               "strings, std to lumex");
static_assert (std::is_convertible<lex<std::string, std::string>,
                                   sex<std::string, std::string>>::value,
               "strings, lumex to std");
static_assert (std::is_convertible<sex<void, int>, lex<void, int>>::value,
               "void, std to lumex");
static_assert (std::is_convertible<lex<void, int>, sex<void, int>>::value,
               "void, lumex to std");
static_assert (
    std::is_convertible<sex<void, int> const &, lex<void, int>>::value,
    "void, std const lvalue to lumex");
static_assert (
    std::is_convertible<lex<void, int> const &, sex<void, int>>::value,
    "void, lumex const lvalue to std");

// The same T and E and no other.
static_assert (!std::is_convertible<sex<long, int>, lex<int, int>>::value,
               "another T does not convert (std to lumex)");
static_assert (!std::is_convertible<sex<int, long>, lex<int, int>>::value,
               "another E does not convert (std to lumex)");
static_assert (!std::is_convertible<lex<long, int>, sex<int, int>>::value,
               "another T does not convert (lumex to std)");
static_assert (!std::is_convertible<lex<int, long>, sex<int, int>>::value,
               "another E does not convert (lumex to std)");
static_assert (!std::is_constructible<lex<int, int>, sex<long, int>>::value,
               "not even explicitly: another T");
static_assert (!std::is_constructible<lex<int, int>, sex<int, long>>::value,
               "not even explicitly: another E");
static_assert (!std::is_constructible<sex<int, int>, lex<long, int>>::value,
               "std::expected is not constructible from another T either");
static_assert (!std::is_convertible<sex<void, int>, lex<int, int>>::value,
               "void to a value type");
static_assert (!std::is_convertible<lex<void, int>, sex<int, int>>::value,
               "a value type to void");
static_assert (!std::is_convertible<int, lex<void, int>>::value,
               "an unrelated type still does not convert to the void one");

// A move-only type converts from an rvalue only.
using move_only_lumex = lex<std::unique_ptr<int>, int>;
using move_only_std = sex<std::unique_ptr<int>, int>;
static_assert (std::is_convertible<move_only_std, move_only_lumex>::value,
               "move-only: std rvalue to lumex");
static_assert (std::is_convertible<move_only_lumex, move_only_std>::value,
               "move-only: lumex rvalue to std");
static_assert (!std::is_convertible<move_only_std &, move_only_lumex>::value,
               "move-only: no copy from a std lvalue");
static_assert (!std::is_convertible<move_only_lumex &, move_only_std>::value,
               "move-only: no copy from a lumex lvalue");
static_assert (
    !std::is_convertible<move_only_std const &, move_only_lumex>::value,
    "move-only: no copy from a std const lvalue");
static_assert (
    !std::is_convertible<move_only_lumex const &, move_only_std>::value,
    "move-only: no copy from a lumex const lvalue");

// Exception specification: noexcept when the copy (the move) of T and E is.
static_assert (
    std::is_nothrow_convertible<lex<int, unsigned>, sex<int, unsigned>>::value,
    "lumex to std, trivial types");
static_assert (std::is_nothrow_constructible<lex<int, unsigned>,
                                             sex<int, unsigned>>::value,
               "std to lumex, trivial types");
// The conversions below are asked for with `static_cast`: cl.exe cannot name
// a specialization of a conversion function template as
// `.operator sex<...> ()`.
static_assert (noexcept (static_cast<sex<std::string, int>> (
                   std::declval<lex<std::string, int> &&> ())),
               "moving a string out is noexcept");
static_assert (!noexcept (static_cast<sex<std::string, int>> (
                   std::declval<lex<std::string, int> const &> ())),
               "copying a string is not");
static_assert (noexcept (lex<std::string, int> (
                   std::declval<sex<std::string, int> &&> ())),
               "moving a string in is noexcept");
static_assert (!noexcept (lex<std::string, int> (
                   std::declval<sex<std::string, int> const &> ())),
               "copying a string in is not");
static_assert (noexcept (static_cast<sex<void, int>> (
                   std::declval<lex<void, int> const &> ())),
               "void with a trivial error: noexcept");
static_assert (!noexcept (static_cast<sex<void, std::string>> (
                   std::declval<lex<void, std::string> const &> ())),
               "void with a string error: copying is not");

// Constant expressions (C++23).
constexpr sex<int, int> constant_std = lex<int, int> (5);
static_assert (constant_std.has_value () && *constant_std == 5,
               "lumex value to std in a constant expression");
constexpr lex<int, int> constant_lumex = sex<int, int> (std::unexpect, 6);
static_assert (!constant_lumex.has_value () && constant_lumex.error () == 6,
               "std error to lumex in a constant expression");
constexpr sex<void, int> constant_void_std = lex<void, int> ();
static_assert (constant_void_std.has_value (),
               "void in a constant expression");

// ---------------------------------------------------------------------
// Functions that take one of the two
// ---------------------------------------------------------------------

int
takes_std (sex<int, int> const &x)
{
  return x.has_value () ? *x : -x.error ();
}

int
takes_lumex (lex<int, int> const &x)
{
  return x.has_value () ? *x : -x.error ();
}

// Both overloads exist: a lumex argument and a std argument each find theirs.
int
overloaded (lex<int, int> const &)
{
  return 1;
}

int
overloaded (sex<int, int> const &)
{
  return 2;
}

int
overloaded_by_value (lex<std::string, int>)
{
  return 1;
}

int
overloaded_by_value (sex<std::string, int>)
{
  return 2;
}

int
void_overloaded (lex<void, int> const &)
{
  return 1;
}

int
void_overloaded (sex<void, int> const &)
{
  return 2;
}
} // namespace

// ---------------------------------------------------------------------
// Value and error, both ways
// ---------------------------------------------------------------------

TEST (ExpectedStdConversionTest, StdValue_BecomesALumexValue)
{
  lex<int, unsigned> const converted = sex<int, unsigned> (std::in_place, 7);
  ASSERT_TRUE (converted.has_value ());
  EXPECT_EQ (7, converted.value ());
}

TEST (ExpectedStdConversionTest, StdError_BecomesALumexError)
{
  lex<int, unsigned> const converted = sex<int, unsigned> (std::unexpect, 9u);
  ASSERT_FALSE (converted.has_value ());
  EXPECT_EQ (9u, converted.error ());
}

TEST (ExpectedStdConversionTest, LumexValue_BecomesAStdValue)
{
  sex<int, unsigned> const converted
      = lex<int, unsigned> (lx::result::in_place, 7);
  ASSERT_TRUE (converted.has_value ());
  EXPECT_EQ (7, converted.value ());
}

TEST (ExpectedStdConversionTest, LumexError_BecomesAStdError)
{
  sex<int, unsigned> const converted
      = lex<int, unsigned> (lx::result::unexpect, 9u);
  ASSERT_FALSE (converted.has_value ());
  EXPECT_EQ (9u, converted.error ());
}

TEST (ExpectedStdConversionTest, Strings_RoundTripBothWays)
{
  std::string const long_text (80, 'x');
  sex<std::string, std::string> const std_value (std::in_place, long_text);
  sex<std::string, std::string> const std_error (std::unexpect,
                                                 "err " + long_text);

  lex<std::string, std::string> const lumex_value = std_value;
  lex<std::string, std::string> const lumex_error = std_error;
  ASSERT_TRUE (lumex_value.has_value ());
  EXPECT_EQ (long_text, lumex_value.value ());
  ASSERT_FALSE (lumex_error.has_value ());
  EXPECT_EQ ("err " + long_text, lumex_error.error ());

  sex<std::string, std::string> const back_value = lumex_value;
  sex<std::string, std::string> const back_error = lumex_error;
  EXPECT_EQ (std_value, back_value);
  EXPECT_EQ (std_error, back_error);
}

TEST (ExpectedStdConversionTest, RoundTrip_GivesTheSameObject)
{
  for (int v = -3; v <= 3; ++v)
    {
      sex<int, int> const from_value (std::in_place, v);
      sex<int, int> const from_error (std::unexpect, v);
      sex<int, int> const back_value = lex<int, int> (from_value);
      sex<int, int> const back_error = lex<int, int> (from_error);
      EXPECT_EQ (from_value, back_value) << v;
      EXPECT_EQ (from_error, back_error) << v;

      lex<int, int> const lumex_value (lx::result::in_place, v);
      lex<int, int> const lumex_error (lx::result::unexpect, v);
      lex<int, int> const again_value = sex<int, int> (lumex_value);
      lex<int, int> const again_error = sex<int, int> (lumex_error);
      EXPECT_TRUE (lumex_value == again_value) << v;
      EXPECT_TRUE (lumex_error == again_error) << v;
    }
}

TEST (ExpectedStdConversionTest, BoolValue_KeepsTheStateAndIsNotCollapsed)
{
  // A bool value must not turn into "has a value" through the bool
  // conversion of the std::expected (LWG 3836): the state is converted.
  sex<bool, int> const std_false (std::in_place, false);
  sex<bool, int> const std_error (std::unexpect, 3);
  lex<bool, int> const lumex_false = std_false;
  lex<bool, int> const lumex_error = std_error;
  ASSERT_TRUE (lumex_false.has_value ());
  EXPECT_FALSE (lumex_false.value ());
  ASSERT_FALSE (lumex_error.has_value ());
  EXPECT_EQ (3, lumex_error.error ());
  // Direct initialization from an lvalue that is not const, and from an
  // rvalue: the value constructor must not take them (std::expected has an
  // explicit operator bool, so bool is constructible from it).
  lex<bool, int> const direct (std_error);
  ASSERT_FALSE (direct.has_value ());
  sex<bool, int> mutable_error (std::unexpect, 5);
  lex<bool, int> const from_lvalue (mutable_error);
  ASSERT_FALSE (from_lvalue.has_value ());
  EXPECT_EQ (5, from_lvalue.error ());
  lex<bool, int> const from_rvalue (sex<bool, int> (std::unexpect, 6));
  ASSERT_FALSE (from_rvalue.has_value ());
  EXPECT_EQ (6, from_rvalue.error ());
  lex<bool, int> const lumex_true (lx::result::in_place, true);
  sex<bool, int> const std_true = lumex_true;
  ASSERT_TRUE (std_true.has_value ());
  EXPECT_TRUE (std_true.value ());
}

TEST (ExpectedStdConversionTest, ConstValueType_Converts)
{
  sex<int, int> const std_value (std::in_place, 4);
  lex<int, int> const converted (std_value);
  EXPECT_EQ (4, converted.value ());
}

// ---------------------------------------------------------------------
// void
// ---------------------------------------------------------------------

TEST (ExpectedStdConversionTest, VoidSuccess_ConvertsBothWays)
{
  lex<void, int> const from_std = sex<void, int> ();
  EXPECT_TRUE (from_std.has_value ());
  sex<void, int> const to_std = lex<void, int> ();
  EXPECT_TRUE (to_std.has_value ());
}

TEST (ExpectedStdConversionTest, VoidError_ConvertsBothWays)
{
  lex<void, int> const from_std = sex<void, int> (std::unexpect, 8);
  ASSERT_FALSE (from_std.has_value ());
  EXPECT_EQ (8, from_std.error ());
  sex<void, int> const to_std = lex<void, int> (lx::result::unexpect, 8);
  ASSERT_FALSE (to_std.has_value ());
  EXPECT_EQ (8, to_std.error ());
}

TEST (ExpectedStdConversionTest, VoidWithAStringError_RoundTrips)
{
  std::string const text (60, 'e');
  sex<void, std::string> const std_error (std::unexpect, text);
  lex<void, std::string> const lumex_error = std_error;
  ASSERT_FALSE (lumex_error.has_value ());
  EXPECT_EQ (text, lumex_error.error ());
  sex<void, std::string> const back = lumex_error;
  EXPECT_EQ (std_error, back);
  sex<void, std::string> const std_success;
  lex<void, std::string> const lumex_success = std_success;
  EXPECT_TRUE (lumex_success.has_value ());
}

// ---------------------------------------------------------------------
// Copy and move
// ---------------------------------------------------------------------

TEST (ExpectedStdConversionTest, MoveOnlyValue_MovesBothWays)
{
  move_only_std from_std (std::in_place, std::make_unique<int> (11));
  move_only_lumex lumex_value = std::move (from_std);
  ASSERT_TRUE (lumex_value.has_value ());
  EXPECT_EQ (11, *lumex_value.value ());
  move_only_std back = std::move (lumex_value);
  ASSERT_TRUE (back.has_value ());
  EXPECT_EQ (11, **back);
}

TEST (ExpectedStdConversionTest, MoveOnlyError_MovesBothWays)
{
  sex<int, std::unique_ptr<int>> from_std (std::unexpect,
                                           std::make_unique<int> (12));
  lex<int, std::unique_ptr<int>> lumex_error = std::move (from_std);
  ASSERT_FALSE (lumex_error.has_value ());
  EXPECT_EQ (12, *lumex_error.error ());
  sex<int, std::unique_ptr<int>> back = std::move (lumex_error);
  ASSERT_FALSE (back.has_value ());
  EXPECT_EQ (12, *back.error ());
}

TEST (ExpectedStdConversionTest, FromAStdLvalue_CopiesOnce)
{
  sex<tracked_t, tracked_t> const std_value (std::in_place, tracked_t (1));
  sex<tracked_t, tracked_t> const std_error (std::unexpect, tracked_t (2));
  int const base_copies = std_value->copies;
  lex<tracked_t, tracked_t> const lumex_value = std_value;
  lex<tracked_t, tracked_t> const lumex_error = std_error;
  EXPECT_EQ (base_copies + 1, lumex_value->copies);
  EXPECT_EQ (1, lumex_value->id);
  EXPECT_EQ (std_value->moves, lumex_value->moves);
  EXPECT_EQ (std_error.error ().copies + 1, lumex_error.error ().copies);
  EXPECT_EQ (2, lumex_error.error ().id);
  EXPECT_EQ (1, std_value->id) << "the source is left alone";
}

TEST (ExpectedStdConversionTest, FromAStdRvalue_MovesAndDoesNotCopy)
{
  sex<tracked_t, tracked_t> std_value (std::in_place, tracked_t (1));
  sex<tracked_t, tracked_t> std_error (std::unexpect, tracked_t (2));
  int const value_copies = std_value->copies;
  int const value_moves = std_value->moves;
  int const error_copies = std_error.error ().copies;
  int const error_moves = std_error.error ().moves;
  lex<tracked_t, tracked_t> const lumex_value = std::move (std_value);
  lex<tracked_t, tracked_t> const lumex_error = std::move (std_error);
  EXPECT_EQ (value_copies, lumex_value->copies);
  EXPECT_EQ (value_moves + 1, lumex_value->moves);
  EXPECT_EQ (error_copies, lumex_error.error ().copies);
  EXPECT_EQ (error_moves + 1, lumex_error.error ().moves);
  EXPECT_EQ (-1, std_value->id) << "the value was moved from";
}

TEST (ExpectedStdConversionTest, FromALumexLvalue_CopiesOnce)
{
  lex<tracked_t, tracked_t> const lumex_value (lx::result::in_place,
                                               tracked_t (1));
  lex<tracked_t, tracked_t> const lumex_error (lx::result::unexpect,
                                               tracked_t (2));
  sex<tracked_t, tracked_t> const std_value = lumex_value;
  sex<tracked_t, tracked_t> const std_error = lumex_error;
  EXPECT_EQ (lumex_value->copies + 1, std_value->copies);
  EXPECT_EQ (lumex_value->moves, std_value->moves);
  EXPECT_EQ (lumex_error.error ().copies + 1, std_error.error ().copies);
  EXPECT_EQ (1, lumex_value->id) << "the source is left alone";
}

TEST (ExpectedStdConversionTest, FromALumexRvalue_MovesAndDoesNotCopy)
{
  lex<tracked_t, tracked_t> lumex_value (lx::result::in_place, tracked_t (1));
  lex<tracked_t, tracked_t> lumex_error (lx::result::unexpect, tracked_t (2));
  int const value_copies = lumex_value->copies;
  int const value_moves = lumex_value->moves;
  int const error_copies = lumex_error.error ().copies;
  int const error_moves = lumex_error.error ().moves;
  sex<tracked_t, tracked_t> const std_value = std::move (lumex_value);
  sex<tracked_t, tracked_t> const std_error = std::move (lumex_error);
  EXPECT_EQ (value_copies, std_value->copies);
  EXPECT_EQ (value_moves + 1, std_value->moves);
  EXPECT_EQ (error_copies, std_error.error ().copies);
  EXPECT_EQ (error_moves + 1, std_error.error ().moves);
}

// ---------------------------------------------------------------------
// Overloads
// ---------------------------------------------------------------------

TEST (ExpectedStdConversionTest, Overload_OfBothTypes_PicksTheExactOne)
{
  lex<int, int> const lumex_value (lx::result::in_place, 1);
  lex<int, int> lumex_mutable (lx::result::in_place, 1);
  sex<int, int> const std_value (std::in_place, 1);
  sex<int, int> std_mutable (std::in_place, 1);
  EXPECT_EQ (1, overloaded (lumex_value));
  EXPECT_EQ (1, overloaded (lumex_mutable));
  EXPECT_EQ (1, overloaded (lex<int, int> (lx::result::in_place, 1)));
  EXPECT_EQ (2, overloaded (std_value));
  EXPECT_EQ (2, overloaded (std_mutable));
  EXPECT_EQ (2, overloaded (sex<int, int> (std::in_place, 1)));
}

TEST (ExpectedStdConversionTest, OverloadByValue_OfBothTypes_PicksTheExactOne)
{
  lex<std::string, int> lumex_value (lx::result::in_place, "a");
  sex<std::string, int> std_value (std::in_place, "a");
  EXPECT_EQ (1, overloaded_by_value (lumex_value));
  EXPECT_EQ (1, overloaded_by_value (std::move (lumex_value)));
  EXPECT_EQ (2, overloaded_by_value (std_value));
  EXPECT_EQ (2, overloaded_by_value (std::move (std_value)));
}

TEST (ExpectedStdConversionTest, VoidOverload_OfBothTypes_PicksTheExactOne)
{
  lex<void, int> const lumex_value;
  sex<void, int> const std_value;
  EXPECT_EQ (1, void_overloaded (lumex_value));
  EXPECT_EQ (2, void_overloaded (std_value));
}

TEST (ExpectedStdConversionTest, FunctionOfTheOtherKind_AcceptsTheConversion)
{
  lex<int, int> const lumex_value (lx::result::in_place, 4);
  lex<int, int> const lumex_error (lx::result::unexpect, 5);
  sex<int, int> const std_value (std::in_place, 4);
  sex<int, int> const std_error (std::unexpect, 5);
  EXPECT_EQ (4, takes_std (lumex_value));
  EXPECT_EQ (-5, takes_std (lumex_error));
  EXPECT_EQ (4, takes_lumex (std_value));
  EXPECT_EQ (-5, takes_lumex (std_error));
}

// ---------------------------------------------------------------------
// The converted object against std::expected
// ---------------------------------------------------------------------

namespace
{
/// What a question to an `expected<int, int>` (of either library) answers.
struct observation
{
  bool has_value;
  int payload;
  int fallback;
  bool transformed_has_value;
  int transformed_payload;
  bool recovered_has_value;
  int recovered_payload;

  bool
  operator== (observation const &other) const
  {
    return has_value == other.has_value && payload == other.payload
           && fallback == other.fallback
           && transformed_has_value == other.transformed_has_value
           && transformed_payload == other.transformed_payload
           && recovered_has_value == other.recovered_has_value
           && recovered_payload == other.recovered_payload;
  }
};

template <typename X>
observation
observe (X const &x)
{
  observation r;
  r.has_value = x.has_value ();
  r.payload = x.has_value () ? x.value () : x.error ();
  r.fallback = x.value_or (-100);
  auto const transformed = x.transform ([] (int v) { return v * 2; });
  r.transformed_has_value = transformed.has_value ();
  r.transformed_payload
      = transformed.has_value () ? transformed.value () : transformed.error ();
  auto const recovered
      = x.or_else ([] (int e) { return X (e + 2 > 0 ? e + 2 : 0); });
  r.recovered_has_value = recovered.has_value ();
  r.recovered_payload
      = recovered.has_value () ? recovered.value () : recovered.error ();
  return r;
}
} // namespace

TEST (ExpectedStdConversionTest, Converted_AnswersAsTheStdExpectedItCameFrom)
{
  for (int v = -4; v <= 4; ++v)
    {
      sex<int, int> const std_value (std::in_place, v);
      sex<int, int> const std_error (std::unexpect, v);
      lex<int, int> const lumex_value = std_value;
      lex<int, int> const lumex_error = std_error;
      EXPECT_TRUE (observe (std_value) == observe (lumex_value)) << v;
      EXPECT_TRUE (observe (std_error) == observe (lumex_error)) << v;
      EXPECT_EQ (std_value == std_value, lumex_value == lumex_value);
    }
}

TEST (ExpectedStdConversionTest, Converted_AnswersAsTheLumexExpectedItCameFrom)
{
  for (int v = -4; v <= 4; ++v)
    {
      lex<int, int> const lumex_value (lx::result::in_place, v);
      lex<int, int> const lumex_error (lx::result::unexpect, v);
      sex<int, int> const std_value = lumex_value;
      sex<int, int> const std_error = lumex_error;
      EXPECT_TRUE (observe (lumex_value) == observe (std_value)) << v;
      EXPECT_TRUE (observe (lumex_error) == observe (std_error)) << v;
    }
}

TEST (ExpectedStdConversionTest, ComparisonOfTheTwo_AfterTheConversionAgrees)
{
  sex<int, int> const std_value (std::in_place, 5);
  sex<int, int> const std_other (std::in_place, 6);
  sex<int, int> const std_error (std::unexpect, 5);
  lex<int, int> const lumex_value = std_value;
  lex<int, int> const lumex_other = std_other;
  lex<int, int> const lumex_error = std_error;
  EXPECT_EQ (std_value == std_other, lumex_value == lumex_other);
  EXPECT_EQ (std_value == std_error, lumex_value == lumex_error);
  EXPECT_EQ (std_value == std_value, lumex_value == lumex_value);
  // Mixed: both are converted to one type by hand; a mixed == does not exist.
  EXPECT_TRUE ((lumex_value == lex<int, int> (std_value)));
  EXPECT_TRUE ((sex<int, int> (lumex_error) == std_error));
}

TEST (ExpectedStdConversionTest, StdAlgorithms_TakeTheConvertedObject)
{
  std::vector<sex<int, int>> values;
  values.push_back (lex<int, int> (lx::result::in_place, 1));
  values.push_back (lex<int, int> (lx::result::unexpect, 2));
  values.emplace_back (lex<int, int> (lx::result::in_place, 3));
  ASSERT_EQ (3u, values.size ());
  EXPECT_EQ (1, values[0].value ());
  EXPECT_EQ (2, values[1].error ());
  EXPECT_EQ (3, values[2].value ());
  std::vector<lex<int, int>> back;
  for (auto const &x : values)
    back.push_back (x);
  ASSERT_EQ (3u, back.size ());
  EXPECT_TRUE (back[0].has_value () && back[0].value () == 1);
  EXPECT_TRUE (!back[1].has_value () && back[1].error () == 2);
}

// ---------------------------------------------------------------------
// What the conversions cannot do (they are pinned so that a change shows)
// ---------------------------------------------------------------------

namespace
{
int
ambiguous_overload (sex<int, int> const &)
{
  return 1;
}

int
ambiguous_overload (lex<long, int> const &)
{
  return 2;
}

template <typename X, typename = void>
struct overload_callable : std::false_type
{
};

template <typename X>
struct overload_callable<
    X, lumex::core::utility::traits::meta::void_t<
           decltype (ambiguous_overload (std::declval<X> ()))>>
    : std::true_type
{
};

// An lvalue of a move-only E converts from nowhere by copy.
using move_only_error_lumex = lex<int, std::unique_ptr<int>>;
using move_only_error_std = sex<int, std::unique_ptr<int>>;
static_assert (
    std::is_convertible<move_only_error_std, move_only_error_lumex>::value,
    "move-only E: std rvalue to lumex");
static_assert (
    std::is_convertible<move_only_error_lumex, move_only_error_std>::value,
    "move-only E: lumex rvalue to std");
static_assert (
    !std::is_convertible<move_only_error_lumex &, move_only_error_std>::value,
    "move-only E: no copy from a lumex lvalue");
static_assert (!std::is_convertible<move_only_error_lumex const &,
                                    move_only_error_std>::value,
               "move-only E: no copy from a lumex const lvalue");
static_assert (
    !std::is_convertible<move_only_error_std &, move_only_error_lumex>::value,
    "move-only E: no copy from a std lvalue");

// Two user-defined conversions are needed to reach an overload of another
// type, so the call is ambiguous (or not viable): the exact overloads work.
static_assert (!overload_callable<lex<int, int>>::value,
               "lumex<int, int> reaches both overloads by a conversion");
static_assert (overload_callable<sex<int, int>>::value, "exact");
static_assert (overload_callable<lex<long, int>>::value, "exact");
} // namespace

TEST (ExpectedStdConversionTest,
      BoolInTheErrorState_ConstructionLosesTheErrorCopyInitDoesNot)
{
  // `std::expected<bool, E> theirs (mine)` picks the constructor of
  // std::expected that takes a value, through the explicit operator bool of
  // the lumex object: the error becomes the value `false`. The library cannot
  // prevent it; copy initialization and to_std () convert the state.
  lex<bool, int> const mine (lx::result::unexpect, 3);
  sex<bool, int> const direct (mine);
  sex<bool, int> const braced{ mine };
  auto const casted = static_cast<sex<bool, int>> (mine);
  EXPECT_TRUE (direct.has_value ());
  EXPECT_TRUE (braced.has_value ());
  EXPECT_TRUE (casted.has_value ());

  sex<bool, int> const copied = mine;
  ASSERT_FALSE (copied.has_value ());
  EXPECT_EQ (3, copied.error ());
  auto const helper = mine.to_std ();
  ASSERT_FALSE (helper.has_value ());
  EXPECT_EQ (3, helper.error ());
  auto const moved = lex<bool, int> (lx::result::unexpect, 4).to_std ();
  ASSERT_FALSE (moved.has_value ());
  EXPECT_EQ (4, moved.error ());
}

namespace
{
template <typename X, typename = void> struct has_to_std : std::false_type
{
};

template <typename X>
struct has_to_std<X, lumex::core::utility::traits::meta::void_t<
                         decltype (std::declval<X> ().to_std ())>>
    : std::true_type
{
};

// to_std () on an lvalue needs copies; on an rvalue it needs moves.
static_assert (!has_to_std<lex<std::unique_ptr<int>, int> &>::value,
               "move-only T: no to_std () on an lvalue");
static_assert (!has_to_std<lex<std::unique_ptr<int>, int> const &>::value,
               "move-only T: no to_std () on a const lvalue");
static_assert (has_to_std<lex<std::unique_ptr<int>, int>>::value,
               "move-only T: to_std () on an rvalue");
static_assert (!has_to_std<lex<int, std::unique_ptr<int>> &>::value,
               "move-only E: no to_std () on an lvalue");
static_assert (has_to_std<lex<int, std::unique_ptr<int>>>::value,
               "move-only E: to_std () on an rvalue");
static_assert (!has_to_std<lex<void, std::unique_ptr<int>> &>::value,
               "void, move-only E: no to_std () on an lvalue");
static_assert (has_to_std<lex<void, std::unique_ptr<int>>>::value,
               "void, move-only E: to_std () on an rvalue");
} // namespace

TEST (ExpectedStdConversionTest,
      VoidToStd_MovesFromAnRvalueAndCopiesFromAnLvalue)
{
  lex<void, tracked_t> error (lx::result::unexpect, tracked_t (1));
  int const copies = error.error ().copies;
  int const moves = error.error ().moves;
  auto const copied = error.to_std ();
  ASSERT_FALSE (copied.has_value ());
  EXPECT_EQ (copies + 1, copied.error ().copies);
  EXPECT_EQ (moves, copied.error ().moves);
  EXPECT_EQ (1, error.error ().id) << "the source is left alone";
  auto const moved = std::move (error).to_std ();
  ASSERT_FALSE (moved.has_value ());
  EXPECT_EQ (copies, moved.error ().copies);
  EXPECT_EQ (moves + 1, moved.error ().moves);
  EXPECT_EQ (-1, error.error ().id) << "the error was moved from";

  lex<void, std::unique_ptr<int>> owner (lx::result::unexpect,
                                         std::make_unique<int> (9));
  auto const std_owner = std::move (owner).to_std ();
  ASSERT_FALSE (std_owner.has_value ());
  EXPECT_EQ (9, *std_owner.error ());
  lex<void, tracked_t> const success;
  EXPECT_TRUE (success.to_std ().has_value ());
}

TEST (ExpectedStdConversionTest, ToStd_ConvertsTheStateOfEveryKind)
{
  lex<int, std::string> const value (lx::result::in_place, 5);
  lex<int, std::string> const error (lx::result::unexpect, "e");
  EXPECT_EQ (5, value.to_std ().value ());
  EXPECT_EQ ("e", error.to_std ().error ());
  lex<void, int> const ok;
  lex<void, int> const bad (lx::result::unexpect, 2);
  EXPECT_TRUE (ok.to_std ().has_value ());
  EXPECT_EQ (2, bad.to_std ().error ());
  lex<std::unique_ptr<int>, int> movable (lx::result::in_place,
                                          std::make_unique<int> (7));
  EXPECT_EQ (7, *std::move (movable).to_std ().value ());
  static_assert (noexcept (std::declval<lex<int, int> const &> ().to_std ()),
                 "noexcept like the conversion");
  static_assert (
      !noexcept (std::declval<lex<int, std::string> const &> ().to_std ()),
      "copying a string is not");
  static_assert (
      std::is_same<decltype (value.to_std ()), sex<int, std::string>>::value,
      "the type");
}

TEST (ExpectedStdConversionTest,
      EqualityAcrossTheTwo_ComparesTheValueNotTheState)
{
  // std's operator== (expected, T2) accepts the lumex object as a T2: two
  // equal values compare equal, anything else (also two equal errors) does
  // not. Convert one side to compare the states.
  sex<int, int> const std_value (std::in_place, 1);
  sex<int, int> const std_error (std::unexpect, 1);
  lex<int, int> const lumex_value (lx::result::in_place, 1);
  lex<int, int> const lumex_error (lx::result::unexpect, 1);
  EXPECT_TRUE ((std_value == lumex_value));
  EXPECT_FALSE ((std_error == lumex_error));
  EXPECT_FALSE ((std_value == lumex_error));
  EXPECT_TRUE ((std_error == lumex_error.to_std ()));
}

#else

TEST (ExpectedStdConversionTest, StdExpected_IsNotAvailable)
{
  GTEST_SKIP () << "the standard library has no std::expected";
}

#endif
