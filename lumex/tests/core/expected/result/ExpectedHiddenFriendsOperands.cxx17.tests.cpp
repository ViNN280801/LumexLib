// The comparisons of expected<T, E> with a value, when the other operand is a
// class that wraps an expected or converts to one: std::optional<expected>, a
// user holder that has a hidden friend of its own against T. The friend of a
// value takes the expected as a deduced operand (not through a conversion of a
// non-deduced first parameter), so the constraint that asks whether the value
// type compares with the operand never depends on itself: GCC 13 refused the
// first two with "ambiguating new declaration of friend operator==" and Clang
// recursed without end through the conversion of a std::reference_wrapper. The
// source is compiled into every suite from C++17 (std::optional).

#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedParitySupport.hpp"

namespace
{
using expected_parity::can_compare;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::unexpect;

constexpr int kValue = 7;
constexpr int kError = 42;

using ex_t = expected<int, int>;

/// A holder with a hidden friend of its own against the type it holds. The
/// holder has the expected among its template arguments, so the friends of the
/// expected are candidates of every comparison that involves the holder.
template <typename T> struct holder_t
{
  T content;

  friend bool
  operator== (holder_t const &lhs, T const &rhs)
  {
    return lhs.content == rhs;
  }
};

TEST (ExpectedHiddenFriendsOperandsTest, Equal_OptionalOfExpected_WithAValue)
{
  std::optional<ex_t> const value{ ex_t (kValue) };
  std::optional<ex_t> const error{ ex_t (unexpect, kError) };
  std::optional<ex_t> const empty;

  static_assert (can_compare<std::optional<ex_t>, int>::value,
                 "the optional compares its expected with a value");
  static_assert (can_compare<int, std::optional<ex_t>>::value,
                 "and the other way");

  EXPECT_TRUE (value == kValue);
  EXPECT_FALSE (value == kValue + 1);
  EXPECT_FALSE (error == kError);
  EXPECT_FALSE (empty == kValue);
  EXPECT_TRUE (kValue == value);
  EXPECT_TRUE (value != kValue + 1);
  EXPECT_TRUE (kValue + 1 != value);
}

TEST (ExpectedHiddenFriendsOperandsTest,
      Equal_OptionalOfExpected_WithAnOptional)
{
  std::optional<ex_t> const value{ ex_t (kValue) };
  std::optional<ex_t> const same{ ex_t (kValue) };
  std::optional<ex_t> const other{ ex_t (kValue + 1) };
  std::optional<ex_t> const error{ ex_t (unexpect, kError) };
  std::optional<ex_t> const same_error{ ex_t (unexpect, kError) };

  static_assert (can_compare<std::optional<ex_t>, std::optional<ex_t>>::value,
                 "two optionals of expected compare");

  EXPECT_TRUE (value == same);
  EXPECT_FALSE (value == other);
  EXPECT_FALSE (value == error);
  EXPECT_TRUE (error == same_error);
  EXPECT_TRUE (value != other);
}

TEST (ExpectedHiddenFriendsOperandsTest,
      Equal_OptionalOfExpected_WithAnExpected)
{
  // The optional on one side, the expected on the other: the expected is a
  // value of the optional, and compares as an expected.
  std::optional<ex_t> const value{ ex_t (kValue) };

  EXPECT_TRUE (value == ex_t (kValue));
  EXPECT_FALSE (value == ex_t (kValue + 1));
  EXPECT_FALSE (value == ex_t (unexpect, kError));
}

TEST (ExpectedHiddenFriendsOperandsTest,
      Equal_HolderOfAnExpected_UsesItsOwnFriend)
{
  holder_t<ex_t> const value{ ex_t (kValue) };
  holder_t<ex_t> const error{ ex_t (unexpect, kError) };

  static_assert (can_compare<holder_t<ex_t>, ex_t>::value,
                 "the friend of the holder compares its expected");

  EXPECT_TRUE (value == ex_t (kValue));
  EXPECT_FALSE (value == ex_t (kValue + 1));
  EXPECT_FALSE (value == ex_t (unexpect, kError));
  EXPECT_TRUE (error == ex_t (unexpect, kError));
  EXPECT_FALSE (error == ex_t (kValue));
}

TEST (ExpectedHiddenFriendsOperandsTest,
      Equal_ExpectedOfAHolder_UsesTheFriendOfTheHolder)
{
  // The value type of the expected is a holder: the expected compares its
  // value with an int through the friend of the holder.
  expected<holder_t<int>, int> const value{ holder_t<int>{ kValue } };
  expected<holder_t<int>, int> const error (unexpect, kError);

  static_assert (can_compare<expected<holder_t<int>, int>, int>::value,
                 "the value type compares with an int through the holder");

  EXPECT_TRUE (value == kValue);
  EXPECT_FALSE (value == kValue + 1);
  EXPECT_FALSE (error == kValue);
}

} // namespace
