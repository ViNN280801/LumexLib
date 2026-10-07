// The comparison operators of expected<T, E> and expected<void, E> next to
// std::expected: == and != between expected objects of different types, with a
// value (on either side) and with an unexpected (on either side). Before C++20
// the != and the reversed == are written out; from C++20 the compiler rewrites
// them from ==. The source is compiled into every suite of the module (C++11,
// C++17 and C++20), so both ways must give the same answers.

#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedParitySupport.hpp"

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;
using namespace expected_parity;

namespace
{

constexpr int kValue = 7;
constexpr int kError = 42;

} // namespace

// === expected with expected ==============================================

TEST (ExpectedComparisonTest, Equal_SameTypes_ComparesStateAndContents)
{
  expected<int, int> const value (kValue);
  expected<int, int> const same_value (kValue);
  expected<int, int> const other_value (kValue + 1);
  expected<int, int> const error (unexpect, kError);
  expected<int, int> const same_error (unexpect, kError);
  expected<int, int> const other_error (unexpect, kError + 1);

  EXPECT_TRUE (value == same_value);
  EXPECT_FALSE (value == other_value);
  EXPECT_TRUE (error == same_error);
  EXPECT_FALSE (error == other_error);
  EXPECT_FALSE (value == error);
  EXPECT_FALSE (error == value);
}

TEST (ExpectedComparisonTest, NotEqual_SameTypes_IsTheNegationOfEqual)
{
  expected<int, int> const value (kValue);
  expected<int, int> const same_value (kValue);
  expected<int, int> const other_value (kValue + 1);
  expected<int, int> const error (unexpect, kError);
  expected<int, int> const other_error (unexpect, kError + 1);

  EXPECT_FALSE (value != same_value);
  EXPECT_TRUE (value != other_value);
  EXPECT_TRUE (value != error);
  EXPECT_TRUE (error != value);
  EXPECT_TRUE (error != other_error);
  EXPECT_FALSE (error != error);
}

TEST (ExpectedComparisonTest, Equal_DifferentTypes_ComparesAsTheContentsDo)
{
  expected<int, int> const value (kValue);
  expected<long, long> const long_value (kValue);
  expected<long, long> const long_other (kValue + 1);
  expected<int, int> const error (unexpect, kError);
  expected<long, long> const long_error (unexpect, kError);

  EXPECT_TRUE (value == long_value);
  EXPECT_TRUE (long_value == value);
  EXPECT_TRUE (value != long_other);
  EXPECT_TRUE (error == long_error);
  EXPECT_TRUE (long_error == error);
  EXPECT_TRUE (value != long_error);
}

TEST (ExpectedComparisonTest, Equal_StringAndLiteralContents_Compare)
{
  expected<std::string, std::string> const text (std::string ("a"));
  expected<char const *, char const *> const literal ("a");

  EXPECT_TRUE (text == literal);
  EXPECT_TRUE (literal == text);
}

TEST (ExpectedComparisonTest, Equal_Void_ComparesStateAndError)
{
  expected<void, int> const success;
  expected<void, int> const other_success;
  expected<void, long> const long_success;
  expected<void, int> const error (unexpect, kError);
  expected<void, long> const long_error (unexpect, kError);
  expected<void, long> const long_other (unexpect, kError + 1);

  EXPECT_TRUE (success == other_success);
  EXPECT_TRUE (success == long_success);
  EXPECT_FALSE (success == error);
  EXPECT_TRUE (error == long_error);
  EXPECT_TRUE (error != long_other);
  EXPECT_TRUE (success != long_error);
  EXPECT_FALSE (success != long_success);
}

TEST (ExpectedComparisonTest,
      Equal_ExpectedOfExpected_ComparesTheInnerWithTheValue)
{
  expected<expected<int, int>, int> const outer{ expected<int, int> (kValue) };
  expected<int, int> const inner (kValue);

  EXPECT_TRUE (outer == inner) << "the inner expected is compared with an int";
  EXPECT_TRUE (outer == kValue);
}

// === expected with a value ===============================================

TEST (ExpectedComparisonTest, EqualToValue_EitherSide_OnlyAValueCanBeEqual)
{
  expected<int, int> const value (kValue);
  expected<int, int> const error (unexpect, kValue);

  EXPECT_TRUE (value == kValue);
  EXPECT_TRUE (kValue == value);
  EXPECT_FALSE (value == kValue + 1);
  EXPECT_FALSE (kValue + 1 == value);
  EXPECT_FALSE (error == kValue) << "an error is never equal to a value";
  EXPECT_FALSE (kValue == error);
}

TEST (ExpectedComparisonTest, NotEqualToValue_EitherSide_IsTheNegation)
{
  expected<int, int> const value (kValue);
  expected<int, int> const error (unexpect, kValue);

  EXPECT_FALSE (value != kValue);
  EXPECT_FALSE (kValue != value);
  EXPECT_TRUE (value != kValue + 1);
  EXPECT_TRUE (kValue + 1 != value);
  EXPECT_TRUE (error != kValue);
  EXPECT_TRUE (kValue != error);
}

TEST (ExpectedComparisonTest,
      EqualToValue_ConvertibleValue_ComparesAsTheTypesDo)
{
  expected<std::string, int> const text (std::string ("text"));
  expected<double, int> const number (2.0);

  EXPECT_TRUE (text == "text");
  EXPECT_TRUE ("text" == text);
  EXPECT_TRUE (text != "other");
  EXPECT_TRUE (number == 2);
  EXPECT_TRUE (2 == number);
}

// === expected with an unexpected =========================================

TEST (ExpectedComparisonTest,
      EqualToUnexpected_EitherSide_OnlyAnErrorCanBeEqual)
{
  expected<int, int> const value (kError);
  expected<int, int> const error (unexpect, kError);

  EXPECT_TRUE (error == unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) == error);
  EXPECT_FALSE (error == unexpected<int> (kError + 1));
  EXPECT_FALSE (unexpected<int> (kError + 1) == error);
  EXPECT_FALSE (value == unexpected<int> (kError))
      << "a value is never equal to an unexpected";
  EXPECT_FALSE (unexpected<int> (kError) == value);
}

TEST (ExpectedComparisonTest, NotEqualToUnexpected_EitherSide_IsTheNegation)
{
  expected<int, int> const value (kError);
  expected<int, int> const error (unexpect, kError);

  EXPECT_FALSE (error != unexpected<int> (kError));
  EXPECT_FALSE (unexpected<int> (kError) != error);
  EXPECT_TRUE (error != unexpected<int> (kError + 1));
  EXPECT_TRUE (unexpected<int> (kError + 1) != error);
  EXPECT_TRUE (value != unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) != value);
}

TEST (ExpectedComparisonTest,
      EqualToUnexpected_OtherErrorType_ComparesAsTheTypesDo)
{
  expected<int, std::string> const error (unexpect, std::string ("bad"));

  EXPECT_TRUE (error == unexpected<char const *> ("bad"));
  EXPECT_TRUE (unexpected<char const *> ("bad") == error);
  EXPECT_TRUE (error != unexpected<char const *> ("other"));
}

TEST (ExpectedComparisonTest, EqualToUnexpected_Void_ComparesTheError)
{
  expected<void, int> const success;
  expected<void, int> const error (unexpect, kError);

  EXPECT_TRUE (error == unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) == error);
  EXPECT_FALSE (success == unexpected<int> (kError));
  EXPECT_TRUE (success != unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) != success);
}

// === what is not comparable ==============================================

TEST (ExpectedComparisonTest, NotComparable_UnrelatedTypes_AreRejected)
{
  using int_expected_t = expected<int, int>;
  using void_expected_t = expected<void, int>;

  static_assert (can_compare<int_expected_t, int_expected_t>::value,
                 "same types compare");
  static_assert (can_compare<int_expected_t, int>::value, "a value compares");
  static_assert (can_compare<int_expected_t, unexpected<int>>::value,
                 "an unexpected compares");
  static_assert (!can_compare<int_expected_t, std::string>::value,
                 "an int and a string do not compare");
  static_assert (
      !can_compare<int_expected_t, expected<std::string, int>>::value,
      "the values cannot be compared");
  static_assert (
      !can_compare<int_expected_t, expected<int, std::string>>::value,
      "the errors cannot be compared");
  static_assert (!can_compare<int_expected_t, unexpected<std::string>>::value,
                 "the errors cannot be compared");
  static_assert (!can_compare<void_expected_t, int>::value,
                 "expected<void, E> has no value to compare");
  static_assert (
      !can_compare<void_expected_t, int_expected_t>::value,
      "expected<void, E> does not compare with one that has a value");
  static_assert (
      !can_compare<int_expected_t, void_expected_t>::value,
      "expected<void, E> does not compare with one that has a value");
  static_assert (can_compare<void_expected_t, unexpected<int>>::value,
                 "expected<void, E> compares with an unexpected");
  SUCCEED ();
}

// === the gtest macros ====================================================

TEST (ExpectedComparisonTest, GoogleTestMacros_UseTheOperators)
{
  expected<int, int> const value (kValue);
  expected<int, int> const same (kValue);
  expected<int, int> const other (kValue + 1);

  EXPECT_EQ (value, same);
  EXPECT_NE (value, other);
  EXPECT_EQ (value, kValue);
  EXPECT_NE (other, kValue);
}
