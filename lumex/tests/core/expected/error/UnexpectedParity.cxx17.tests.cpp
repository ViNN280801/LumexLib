// The deduction guide of unexpected, as std::unexpected has it: constructing
// an unexpected from a value deduces unexpected<decltype (value)>. The C++17
// and C++20 suites run this file; the C++11 suite cannot.

#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

using namespace lumex::core::expected::error;

namespace
{

constexpr int kError = 42;

} // namespace

TEST (UnexpectedParityTest, DeductionGuide_Unexpected_DeducesTheErrorType)
{
  unexpected from_int (kError);
  unexpected from_string (std::string ("text"));
  unexpected from_literal ("literal");

  static_assert (std::is_same<decltype (from_int), unexpected<int>>::value,
                 "the error type is the type of the argument");
  static_assert (
      std::is_same<decltype (from_string), unexpected<std::string>>::value,
      "the error type is the type of the argument");
  static_assert (
      std::is_same<decltype (from_literal), unexpected<char const *>>::value,
      "an array decays, as in std::unexpected");
  EXPECT_EQ (from_int.error (), kError);
  EXPECT_EQ (from_string.error (), "text");
}
