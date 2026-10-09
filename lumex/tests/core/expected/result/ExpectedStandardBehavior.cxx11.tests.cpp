// Points of [expected.object] and [expected.void] that no other file of the
// directory pins: has_error (), the constraint of emplace (), a value type
// that overloads operator&, and a cv-qualified value type. The tests compile
// from C++11, so every expected suite runs them.

#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"
#include "lumex/tests/core/expected/ExpectedMembersSupport.hpp"

namespace
{
using lumex::core::expected::result::expected;
using lumex::core::expected::result::in_place;
using lumex::core::expected::result::unexpect;
namespace traits = lumex::core::utility::traits;

/// `uut.emplace (args...)` is well-formed.
template <typename Uut, typename Arguments, typename = void>
struct can_emplace : std::false_type
{
};

template <typename Uut, typename... Arguments>
struct can_emplace<
    Uut, void (Arguments...),
    traits::meta::void_t<decltype (std::declval<Uut &> ().emplace (
        std::declval<Arguments> ()...))>> : std::true_type
{
};

/// Constructible from an `int` without throwing.
struct nothrow_from_int_t
{
  int value;
  nothrow_from_int_t (int v) noexcept : value (v) {}
};

/// Constructible from an `int`, and the construction may throw.
struct throwing_from_int_t
{
  int value;
  throwing_from_int_t (int v) : value (v) {}
};

/// Built from a list without throwing / possibly throwing.
struct nothrow_list_t
{
  int count;
  nothrow_list_t (std::initializer_list<int> list) noexcept
      : count (static_cast<int> (list.size ()))
  {
  }
};

struct throwing_list_t
{
  int count;
  throwing_list_t (std::initializer_list<int> list)
      : count (static_cast<int> (list.size ()))
  {
  }
};

/// Overloads the unary operator&, which must never be called by expected.
struct hostile_t
{
  int value;

  explicit hostile_t (int v = 0) : value (v) {}
  hostile_t (hostile_t const &) = default;
  hostile_t &operator= (hostile_t const &) = default;

  hostile_t *
  operator& () const
  {
    ADD_FAILURE () << "operator& of the contained value was called";
    return nullptr;
  }
};
} // namespace

// === has_error ============================================================

TEST (ExpectedStandardBehaviorTest, HasError_IsTheNegationOfHasValue)
{
  expected<int, int> const value (in_place, 1);
  expected<int, int> const error (unexpect, 2);
  expected<void, int> const success;
  expected<void, int> const failure (unexpect, 3);

  EXPECT_FALSE (value.has_error ());
  EXPECT_TRUE (error.has_error ());
  EXPECT_FALSE (success.has_error ());
  EXPECT_TRUE (failure.has_error ());
  static_assert (noexcept (value.has_error ()), "has_error is noexcept");
  static_assert (noexcept (success.has_error ()), "has_error is noexcept");
}

// === the constraint of emplace ============================================

TEST (ExpectedStandardBehaviorTest, Emplace_IsOfferedForANothrowConstruction)
{
  static_assert (
      can_emplace<expected<nothrow_from_int_t, int>, void (int)>::value,
      "a noexcept constructor");
  static_assert (can_emplace<expected<int, int>, void (int)>::value, "int");
  static_assert (can_emplace<expected<int, int>, void ()>::value,
                 "no arguments");
  static_assert (can_emplace<expected<nothrow_list_t, int>,
                             void (std::initializer_list<int>)>::value,
                 "a list and a noexcept constructor");
  SUCCEED ();
}

TEST (ExpectedStandardBehaviorTest,
      Emplace_IsNotOfferedWhenTheConstructionMayThrow)
{
  static_assert (
      !can_emplace<expected<throwing_from_int_t, int>, void (int)>::value,
      "a constructor that may throw");
  static_assert (
      !can_emplace<expected<std::string, int>, void (char const *)>::value,
      "std::string from a pointer may allocate");
  static_assert (
      !can_emplace<expected<std::vector<int>, int>, void (std::size_t)>::value,
      "std::vector may allocate");
  static_assert (!can_emplace<expected<throwing_list_t, int>,
                              void (std::initializer_list<int>)>::value,
                 "a list and a constructor that may throw");
  static_assert (!can_emplace<expected<int, int>, void (std::string)>::value,
                 "not constructible at all");
  SUCCEED ();
}

TEST (ExpectedStandardBehaviorTest, Emplace_OverAnErrorCannotThrow)
{
  expected<nothrow_from_int_t, int> uut (unexpect, 1);
  static_assert (noexcept (uut.emplace (2)), "emplace is noexcept");

  nothrow_from_int_t &built = uut.emplace (2);

  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (built.value, 2);
  EXPECT_EQ (&built, &*uut);
}

// === a value type that overloads operator& ================================

TEST (ExpectedStandardBehaviorTest, OverloadedAddressOperator_IsNeverUsed)
{
  expected<hostile_t, hostile_t> uut (in_place, 1);
  expected<hostile_t, hostile_t> other (unexpect, 2);

  EXPECT_EQ (uut->value, 1);
  EXPECT_EQ ((*uut).value, 1);
  uut = other;
  EXPECT_EQ (uut.error ().value, 2);
  other = expected<hostile_t, hostile_t> (in_place, 3);
  uut.swap (other);
  EXPECT_EQ (uut->value, 3);
  EXPECT_EQ (other.error ().value, 2);
  uut.emplace_error (4);
  EXPECT_EQ (uut.error ().value, 4);
  expected<hostile_t, hostile_t> const copy (uut);
  EXPECT_EQ (copy.error ().value, 4);
  expected<hostile_t, int> converted (in_place, 5);
  EXPECT_EQ (converted->value, 5);
  expected<hostile_t, int> const assigned = converted;
  EXPECT_EQ (assigned->value, 5);
}

// === a cv-qualified value type ============================================

TEST (ExpectedStandardBehaviorTest,
      ConstValue_IsStoredWithoutConstAndObservedWithIt)
{
  expected<int const, int> uut (in_place, 1);

  static_assert (std::is_same<decltype (*uut), int const &>::value,
                 "operator* gives the constant value");
  static_assert (std::is_same<decltype (uut.value ()), int const &>::value,
                 "value gives the constant value");
  static_assert (
      std::is_same<decltype (std::move (uut).value ()), int const &&>::value,
      "an rvalue gives a constant rvalue");
  EXPECT_EQ (*uut, 1);
  EXPECT_EQ (uut.value_or (2), 1);
}

TEST (ExpectedStandardBehaviorTest,
      ConstValue_CanBeCopiedAndMovedButNotAssigned)
{
  using const_string_t = expected<std::string const, int>;

  static_assert (std::is_copy_constructible<const_string_t>::value, "copy");
  static_assert (std::is_move_constructible<const_string_t>::value, "move");
  static_assert (!std::is_copy_assignable<const_string_t>::value,
                 "a const value cannot be assigned");
  static_assert (!std::is_move_assignable<const_string_t>::value,
                 "a const value cannot be assigned");

  const_string_t const original (in_place, "text");
  const_string_t const copy (original);
  const_string_t const moved (const_string_t (in_place, "other"));

  EXPECT_EQ (*copy, "text");
  EXPECT_EQ (*moved, "other");
}

TEST (ExpectedStandardBehaviorTest,
      ConstValue_IsReplacedByEmplaceAndSwapIsNotOffered)
{
  expected<int const, int> uut (unexpect, 1);

  int const &built = uut.emplace (7);

  EXPECT_EQ (built, 7);
  EXPECT_EQ (*uut, 7);
  static_assert (
      !expected_members::has_member_swap<expected<int const, int>>::value,
      "a constant value cannot be swapped");
}

// === the explicit conversion to bool =====================================

TEST (ExpectedStandardBehaviorTest, ConversionToBool_IsExplicit)
{
  static_assert (!std::is_convertible<expected<int, int>, bool>::value,
                 "explicit");
  static_assert (std::is_constructible<bool, expected<int, int>>::value,
                 "but available");
  static_assert (!std::is_convertible<expected<void, int>, bool>::value,
                 "explicit");
  expected<int, int> const value (in_place, 1);
  EXPECT_TRUE (static_cast<bool> (value));
  EXPECT_TRUE (value ? true : false);
}
