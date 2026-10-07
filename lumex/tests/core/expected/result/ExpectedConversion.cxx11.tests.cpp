// Constructors and assignments of expected<T, E> and expected<void, E> that
// std::expected has beyond the basic ones: the converting constructors from an
// expected of other types, the constructors from an unexpected that are
// implicit when the error converts, the explicit value constructor, the
// initializer-list overloads, the assignment from a value or an unexpected,
// and the constraints and noexcept specifications that make the standard
// traits tell the truth. The source is compiled into every suite of the module
// (C++11, C++17 and C++20).

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

/// Throws from its constructor for a negative argument.
struct throwing_t
{
  int value;

  throwing_t (int v) : value (v)
  {
    if (v < 0)
      throw std::runtime_error ("negative");
  }
};

/// Constructible (and convertible) from anything, an `expected` included.
struct wraps_t
{
  bool whole;

  wraps_t () : whole (false) {}
  template <typename X> wraps_t (X const &) : whole (true) {}
};

/// A move can throw.
struct throwing_move_t
{
  throwing_move_t () {}
  throwing_move_t (throwing_move_t const &) {}
  throwing_move_t (throwing_move_t &&) noexcept (false) {}
  throwing_move_t &
  operator= (throwing_move_t const &)
  {
    return *this;
  }
  throwing_move_t &
  operator= (throwing_move_t &&) noexcept (false)
  {
    return *this;
  }
};

expected<int, std::string>
fail_with_text ()
{
  return unexpected<std::string> (std::string ("text"));
}

expected<int, std::string>
fail_with_literal ()
{
  return unexpected<char const *> ("literal");
}

expected<void, std::string>
fail_void ()
{
  return unexpected<std::string> (std::string ("void text"));
}

} // namespace

// === converting constructors =============================================

TEST (ExpectedConversionTest, ConvertingCopy_ValueType_IsImplicit)
{
  expected<int, int> const source (kValue);

  expected<long, int> const uut = source;

  static_assert (std::is_convertible<expected<int, int> const &,
                                     expected<long, int>>::value,
                 "int converts to long, so the expected converts implicitly");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value (), static_cast<long> (kValue));
  EXPECT_EQ (source.value (), kValue) << "the source is not changed";
}

TEST (ExpectedConversionTest, ConvertingCopy_ErrorType_IsImplicit)
{
  expected<int, int> const source (unexpect, kError);

  expected<int, long> const uut = source;

  static_assert (std::is_convertible<expected<int, int> const &,
                                     expected<int, long>>::value,
                 "int converts to long, so the expected converts implicitly");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), static_cast<long> (kError));
}

TEST (ExpectedConversionTest, ConvertingCopy_LValueAndConstValue_BothConvert)
{
  expected<int, int> mutable_source (kValue);
  expected<int, int> const const_source (kValue);

  expected<double, int> from_mutable (mutable_source);
  expected<double, int> from_const (const_source);

  EXPECT_EQ (from_mutable.value (), static_cast<double> (kValue));
  EXPECT_EQ (from_const.value (), static_cast<double> (kValue));
}

TEST (ExpectedConversionTest, ConvertingMove_ValueIsMovedOut)
{
  expected<std::unique_ptr<int>, int> source (
      std::unique_ptr<int> (new int (kValue)));

  expected<std::shared_ptr<int>, int> uut = std::move (source);

  static_assert (
      std::is_convertible<expected<std::unique_ptr<int>, int>,
                          expected<std::shared_ptr<int>, int>>::value,
      "unique_ptr moves into shared_ptr, so the expected converts");
  static_assert (
      !std::is_convertible<expected<std::unique_ptr<int>, int> &,
                           expected<std::shared_ptr<int>, int>>::value,
      "an lvalue would need a copy of the unique_ptr");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (*uut.value (), kValue);
  EXPECT_EQ (source.value (), nullptr) << "the value was moved from";
}

TEST (ExpectedConversionTest, ConvertingMove_ErrorIsMovedOut)
{
  expected<int, std::unique_ptr<int>> source (
      unexpect, std::unique_ptr<int> (new int (kError)));

  expected<int, std::shared_ptr<int>> uut = std::move (source);

  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (*uut.error (), kError);
  EXPECT_EQ (source.error (), nullptr) << "the error was moved from";
}

TEST (ExpectedConversionTest, ConvertingConstructor_Void_BothStates)
{
  expected<void, int> const success;
  expected<void, int> const failure (unexpect, kError);

  expected<void, long> const converted_success = success;
  expected<void, long> const converted_failure = failure;

  static_assert (std::is_convertible<expected<void, int> const &,
                                     expected<void, long>>::value,
                 "the error converts, so expected<void, E> converts");
  EXPECT_TRUE (converted_success.has_value ());
  ASSERT_FALSE (converted_failure.has_value ());
  EXPECT_EQ (converted_failure.error (), static_cast<long> (kError));
}

TEST (ExpectedConversionTest, ConvertingConstructor_VoidMove_ErrorIsMovedOut)
{
  expected<void, std::unique_ptr<int>> source (
      unexpect, std::unique_ptr<int> (new int (kError)));

  expected<void, std::shared_ptr<int>> uut = std::move (source);

  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (*uut.error (), kError);
  EXPECT_EQ (source.error (), nullptr) << "the error was moved from";
}

TEST (ExpectedConversionTest, ConvertingConstructor_VoidAndValue_DoNotConvert)
{
  static_assert (
      !std::is_constructible<expected<void, int>, expected<int, int>>::value,
      "an expected with a value type is not an expected<void, E>");
  static_assert (
      !std::is_constructible<expected<int, int>, expected<void, int>>::value,
      "expected<void, E> has no value to convert");
  SUCCEED ();
}

TEST (ExpectedConversionTest,
      ConvertingConstructor_ExplicitOnlyConversion_NeedsAnExplicitCall)
{
  expected<int, int> const source (kValue);

  expected<explicit_int_t, int> const uut (source);

  static_assert (std::is_constructible<expected<explicit_int_t, int>,
                                       expected<int, int> const &>::value,
                 "explicit_int_t is constructible from int");
  static_assert (!std::is_convertible<expected<int, int> const &,
                                      expected<explicit_int_t, int>>::value,
                 "but not implicitly, so neither is the expected");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value ().value, kValue);
}

TEST (ExpectedConversionTest,
      ConvertingConstructor_ExplicitErrorConversion_NeedsAnExplicitCall)
{
  expected<int, int> source (unexpect, kError);

  expected<int, explicit_int_t> const uut (std::move (source));

  static_assert (!std::is_convertible<expected<int, int>,
                                      expected<int, explicit_int_t>>::value,
                 "explicit_int_t is not implicitly constructible from int");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().value, kError);
}

TEST (ExpectedConversionTest,
      ConvertingConstructor_NoConversion_IsNotConstructible)
{
  static_assert (!std::is_constructible<expected<std::string, int>,
                                        expected<int, int>>::value,
                 "a string cannot be made from an int");
  static_assert (!std::is_constructible<expected<int, int>,
                                        expected<int, std::string>>::value,
                 "an int cannot be made from a string");
  SUCCEED ();
}

TEST (ExpectedConversionTest,
      ConvertingConstructor_BoolValue_ConvertsTheValueInsideNotTheState)
{
  expected<int, int> const zero (0);
  expected<int, int> const failure (unexpect, kError);

  expected<bool, int> const from_zero = zero;
  expected<bool, int> const from_failure = failure;

  static_assert (
      std::is_convertible<expected<int, int>, expected<bool, int>>::value,
      "the value converts from int to bool, so the expected does");
  ASSERT_TRUE (from_zero.has_value ()) << "a value stays a value";
  EXPECT_FALSE (from_zero.value ()) << "the value 0 became false";
  ASSERT_FALSE (from_failure.has_value ());
  EXPECT_EQ (from_failure.error (), kError);
}

TEST (ExpectedConversionTest,
      ValueConstructor_BoolValue_DoesNotTurnAnExpectedIntoABool)
{
  static_assert (!std::is_constructible<expected<bool, int>,
                                        expected<std::string, int>>::value,
                 "a string is not a bool, and the expected must not become "
                 "one through operator bool");
  static_assert (std::is_constructible<expected<bool, int>, int>::value,
                 "a plain int still makes a bool");
  static_assert (std::is_constructible<expected<bool, int>, bool>::value,
                 "so does a bool");
  SUCCEED ();
}

TEST (ExpectedConversionTest,
      ConvertingConstructor_ValueThatTakesTheWholeExpected_WinsOverTheValue)
{
  expected<int, int> const failure (unexpect, kError);

  expected<wraps_t, int> const uut = failure;

  ASSERT_TRUE (uut.has_value ()) << "the expected itself was wrapped";
  EXPECT_TRUE (uut.value ().whole);
}

TEST (ExpectedConversionTest, NestedExpected_CopyMoveAndAssign_KeepEveryState)
{
  using nested_t = expected<expected<int, int>, int>;
  nested_t inner_value{ expected<int, int> (kValue) };
  nested_t inner_error{ expected<int, int> (unexpect, kError) };
  nested_t outer_error (unexpect, kError + 1);

  nested_t copy_value (inner_value);
  nested_t copy_inner_error (inner_error);
  nested_t copy_outer_error (outer_error);
  nested_t assigned (outer_error);
  assigned = inner_error;

  EXPECT_EQ (copy_value.value ().value (), kValue);
  ASSERT_TRUE (copy_inner_error.has_value ());
  EXPECT_EQ (copy_inner_error.value ().error (), kError);
  ASSERT_FALSE (copy_outer_error.has_value ());
  EXPECT_EQ (copy_outer_error.error (), kError + 1);
  ASSERT_TRUE (assigned.has_value ());
  EXPECT_EQ (assigned.value ().error (), kError);
}

TEST (ExpectedConversionTest, ConstValueType_ConstructsObservesAndTransforms)
{
  expected<int const, int> const uut (kValue);
  expected<std::string const, int> const text{ std::string ("t") };
  expected<int const, int> const copy (uut);
  expected<int const, int> const failure (unexpect, kError);

  auto next = uut.transform ([] (int value) { return value + 1; });
  auto kept = failure.transform_error ([] (int error) { return error + 1; });

  static_assert (std::is_same<decltype (uut.value ()), int const &>::value,
                 "the value is constant");
  static_assert (std::is_same<decltype (next), expected<int, int>>::value,
                 "transform builds a plain int");
  EXPECT_EQ (uut.value (), kValue);
  EXPECT_EQ (*copy, kValue);
  EXPECT_EQ (text->size (), 1u);
  EXPECT_EQ (next.value (), kValue + 1);
  ASSERT_FALSE (kept.has_value ());
  EXPECT_EQ (kept.error (), kError + 1);
}

// === unexpected ==========================================================

TEST (ExpectedConversionTest,
      Unexpected_ReturnedFromAFunction_ConvertsImplicitly)
{
  expected<int, std::string> const from_string = fail_with_text ();
  expected<int, std::string> const from_literal = fail_with_literal ();
  expected<void, std::string> const from_void = fail_void ();

  ASSERT_FALSE (from_string.has_value ());
  EXPECT_EQ (from_string.error (), "text");
  ASSERT_FALSE (from_literal.has_value ());
  EXPECT_EQ (from_literal.error (), "literal");
  ASSERT_FALSE (from_void.has_value ());
  EXPECT_EQ (from_void.error (), "void text");
}

TEST (ExpectedConversionTest, Unexpected_NamedObject_ConvertsByCopyAndByMove)
{
  unexpected<tracked_t> named{ tracked_t (kError) };

  expected<int, tracked_t> const copied = named;
  expected<int, tracked_t> const moved = std::move (named);

  EXPECT_EQ (copied.error ().id, kError);
  EXPECT_GE (copied.error ().copies, 1);
  EXPECT_EQ (moved.error ().id, kError);
  EXPECT_EQ (moved.error ().copies, 0);
  EXPECT_EQ (named.error ().id, -1) << "the named unexpected was moved from";
}

TEST (ExpectedConversionTest, Unexpected_ConvertibleErrorType_IsImplicit)
{
  static_assert (std::is_convertible<unexpected<char const *>,
                                     expected<int, std::string>>::value,
                 "const char * converts to std::string");
  static_assert (std::is_convertible<unexpected<int> const &,
                                     expected<void, long>>::value,
                 "int converts to long");
  SUCCEED ();
}

TEST (ExpectedConversionTest,
      Unexpected_ExplicitOnlyErrorType_NeedsAnExplicitCall)
{
  expected<int, explicit_int_t> const uut{ unexpected<int> (kError) };
  expected<void, explicit_int_t> const void_uut{ unexpected<int> (kError) };

  static_assert (std::is_constructible<expected<int, explicit_int_t>,
                                       unexpected<int>>::value,
                 "explicit_int_t is constructible from int");
  static_assert (!std::is_convertible<unexpected<int>,
                                      expected<int, explicit_int_t>>::value,
                 "but not implicitly");
  static_assert (!std::is_convertible<unexpected<int>,
                                      expected<void, explicit_int_t>>::value,
                 "but not implicitly");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().value, kError);
  ASSERT_FALSE (void_uut.has_value ());
  EXPECT_EQ (void_uut.error ().value, kError);
}

TEST (ExpectedConversionTest, Unexpected_UnrelatedErrorType_IsNotConstructible)
{
  static_assert (!std::is_constructible<expected<int, std::string>,
                                        unexpected<int>>::value,
                 "a string cannot be made from an int");
  static_assert (!std::is_constructible<expected<void, std::string>,
                                        unexpected<int>>::value,
                 "a string cannot be made from an int");
  SUCCEED ();
}

// === the value constructor ===============================================

TEST (ExpectedConversionTest,
      ValueConstructor_ExplicitOnlyValue_NeedsAnExplicitCall)
{
  expected<explicit_int_t, int> const uut (kValue);

  static_assert (
      std::is_constructible<expected<explicit_int_t, int>, int>::value,
      "explicit_int_t is constructible from int");
  static_assert (
      !std::is_convertible<int, expected<explicit_int_t, int>>::value,
      "but not implicitly, so the expected is not either");
  static_assert (std::is_convertible<int, expected<long, int>>::value,
                 "a convertible value gives an implicit conversion");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value ().value, kValue);
}

// === assignment ==========================================================

TEST (ExpectedConversionTest, AssignValue_FromEachState_HoldsTheValue)
{
  expected<tracked_t, int> from_value (in_place, 1);
  expected<tracked_t, int> from_error (unexpect, kError);

  from_value = tracked_t (kValue);
  from_error = tracked_t (kValue + 1);

  ASSERT_TRUE (from_value.has_value ());
  EXPECT_EQ (from_value.value ().id, kValue);
  ASSERT_TRUE (from_error.has_value ());
  EXPECT_EQ (from_error.value ().id, kValue + 1);
}

TEST (ExpectedConversionTest, AssignValue_ConvertibleType_ConvertsFirst)
{
  expected<std::string, int> uut (unexpect, kError);

  uut = "text";

  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value (), "text");
}

TEST (ExpectedConversionTest, AssignValue_LValue_CopiesIt)
{
  tracked_t const source (kValue);
  expected<tracked_t, int> uut (unexpect, kError);

  uut = source;

  EXPECT_EQ (uut.value ().id, kValue);
  EXPECT_GE (uut.value ().copies, 1);
  EXPECT_EQ (source.id, kValue);
}

TEST (ExpectedConversionTest, AssignUnexpected_FromEachState_HoldsTheError)
{
  expected<int, tracked_t> from_value (kValue);
  expected<int, tracked_t> from_error (unexpect, kError);

  from_value = unexpected<tracked_t> (tracked_t (1));
  from_error = unexpected<tracked_t> (tracked_t (2));

  ASSERT_FALSE (from_value.has_value ());
  EXPECT_EQ (from_value.error ().id, 1);
  ASSERT_FALSE (from_error.has_value ());
  EXPECT_EQ (from_error.error ().id, 2);
}

TEST (ExpectedConversionTest, AssignUnexpected_LValue_CopiesTheError)
{
  unexpected<tracked_t> const source{ tracked_t (kError) };
  expected<int, tracked_t> uut (kValue);

  uut = source;

  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kError);
  EXPECT_GE (uut.error ().copies, 1);
  EXPECT_EQ (source.error ().id, kError);
}

TEST (ExpectedConversionTest, AssignUnexpected_Void_HoldsTheError)
{
  expected<void, tracked_t> from_success;
  expected<void, tracked_t> from_error (unexpect, kError);
  unexpected<tracked_t> const source{ tracked_t (3) };

  from_success = unexpected<tracked_t> (tracked_t (1));
  from_error = source;

  ASSERT_FALSE (from_success.has_value ());
  EXPECT_EQ (from_success.error ().id, 1);
  ASSERT_FALSE (from_error.has_value ());
  EXPECT_EQ (from_error.error ().id, 3);
}

TEST (ExpectedConversionTest,
      AssignValue_ConstructionThrows_KeepsTheOldContents)
{
  expected<throwing_t, int> from_value (in_place, kValue);
  expected<throwing_t, int> from_error (unexpect, kError);

  EXPECT_THROW (from_value = -1, std::runtime_error);
  EXPECT_THROW (from_error = -1, std::runtime_error);

  ASSERT_TRUE (from_value.has_value ());
  EXPECT_EQ (from_value.value ().value, kValue);
  ASSERT_FALSE (from_error.has_value ());
  EXPECT_EQ (from_error.error (), kError);
}

TEST (ExpectedConversionTest,
      Assign_ExpectedItself_UsesTheCopyAndMoveAssignment)
{
  expected<int, int> source (kValue);
  expected<int, int> uut (unexpect, kError);

  uut = source;

  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value (), kValue);
  static_assert (
      !std::is_assignable<expected<int, int> &,
                          expected<int, std::string>>::value,
      "another expected is not an int, and not an expected<int, int>");
}

// === initializer lists ===================================================

TEST (ExpectedConversionTest,
      InitializerList_InPlaceValue_BuildsTheValueFromTheList)
{
  expected<std::vector<int>, int> const uut (in_place, { 1, 2, 3 });
  expected<std::vector<int>, int> const with_argument (in_place, { 1, 2, 3 },
                                                       std::allocator<int> ());

  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value (), (std::vector<int>{ 1, 2, 3 }));
  EXPECT_EQ (with_argument.value (), (std::vector<int>{ 1, 2, 3 }));
}

TEST (ExpectedConversionTest,
      InitializerList_Unexpect_BuildsTheErrorFromTheList)
{
  expected<int, std::vector<int>> const uut (unexpect, { 4, 5 });
  expected<void, std::vector<int>> const void_uut (unexpect, { 6, 7 });

  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), (std::vector<int>{ 4, 5 }));
  ASSERT_FALSE (void_uut.has_value ());
  EXPECT_EQ (void_uut.error (), (std::vector<int>{ 6, 7 }));
}

TEST (ExpectedConversionTest, InitializerList_Emplace_ReplacesTheContents)
{
  expected<std::vector<int>, int> from_value (in_place, { 1 });
  expected<std::vector<int>, int> from_error (unexpect, kError);

  std::vector<int> &first = from_value.emplace ({ 7, 8 });
  std::vector<int> &second = from_error.emplace ({ 9 });

  EXPECT_EQ (first, (std::vector<int>{ 7, 8 }));
  EXPECT_EQ (second, (std::vector<int>{ 9 }));
  ASSERT_TRUE (from_error.has_value ());
  EXPECT_EQ (from_error.value (), (std::vector<int>{ 9 }));
}

TEST (ExpectedConversionTest,
      InitializerList_Unexpected_BuildsTheErrorFromTheList)
{
  unexpected<std::vector<int>> const uut (in_place, { 1, 2 });

  EXPECT_EQ (uut.error (), (std::vector<int>{ 1, 2 }));
}

// === constraints and noexcept ============================================

TEST (ExpectedConversionTest,
      DefaultConstructor_OnlyForDefaultConstructibleValue)
{
  static_assert (std::is_default_constructible<expected<int, int>>::value,
                 "int is default-constructible");
  static_assert (std::is_default_constructible<expected<void, int>>::value,
                 "expected<void, E> is always default-constructible");
  static_assert (
      !std::is_default_constructible<expected<no_default_t, int>>::value,
      "no_default_t has no default constructor, so neither has the expected");
  SUCCEED ();
}

TEST (ExpectedConversionTest,
      InPlaceConstructors_OnlyForConstructibleArguments)
{
  static_assert (
      std::is_constructible<expected<int, int>, in_place_tag, int>::value,
      "an int is made from an int");
  static_assert (!std::is_constructible<expected<int, int>, in_place_tag,
                                        std::string>::value,
                 "an int is not made from a string");
  static_assert (!std::is_constructible<expected<int, int>, unexpect_t,
                                        std::string>::value,
                 "an int error is not made from a string");
  static_assert (
      std::is_constructible<expected<void, int>, in_place_tag>::value,
      "expected<void, E> takes the in_place tag alone");
  static_assert (!std::is_constructible<expected<void, int>, unexpect_t,
                                        std::string>::value,
                 "an int error is not made from a string");
  SUCCEED ();
}

TEST (ExpectedConversionTest, Noexcept_FollowsTheConstructionOfTheContents)
{
  static_assert (
      std::is_nothrow_default_constructible<expected<int, int>>::value,
      "an int is made without throwing");
  static_assert (std::is_nothrow_constructible<expected<int, int>, int>::value,
                 "an int is made without throwing");
  static_assert (std::is_nothrow_constructible<expected<int, int>,
                                               in_place_tag, int>::value,
                 "an int is made without throwing");
  static_assert (std::is_nothrow_constructible<expected<int, int>, unexpect_t,
                                               int>::value,
                 "an int is made without throwing");
  static_assert (std::is_nothrow_constructible<expected<int, int>,
                                               unexpected<int>>::value,
                 "an int is made without throwing");
  static_assert (
      std::is_nothrow_constructible<expected<long, int>,
                                    expected<int, int> const &>::value,
      "a conversion of ints does not throw");
  static_assert (!std::is_nothrow_constructible<expected<std::string, int>,
                                                char const *>::value,
                 "a string allocates and may throw");
  SUCCEED ();
}

TEST (ExpectedConversionTest, SwapNoexcept_FollowsMoveAndSwapOfTheContents)
{
  using plain_t = expected<int, int>;
  using throwing_move_expected_t = expected<throwing_move_t, int>;
  using void_plain_t = expected<void, int>;
  using void_throwing_move_expected_t = expected<void, throwing_move_t>;

  static_assert (
      noexcept (std::declval<plain_t &> ().swap (std::declval<plain_t &> ())),
      "ints swap without throwing");
  static_assert (
      noexcept (swap (std::declval<plain_t &> (), std::declval<plain_t &> ())),
      "ints swap without throwing");
  static_assert (!noexcept (std::declval<throwing_move_expected_t &> ().swap (
                     std::declval<throwing_move_expected_t &> ())),
                 "a throwing move makes the swap throwing");
  static_assert (
      !noexcept (swap (std::declval<throwing_move_expected_t &> (),
                       std::declval<throwing_move_expected_t &> ())),
      "the free swap is noexcept exactly when the member swap is");
  static_assert (noexcept (std::declval<void_plain_t &> ().swap (
                     std::declval<void_plain_t &> ())),
                 "an int error swaps without throwing");
  static_assert (noexcept (swap (std::declval<void_plain_t &> (),
                                 std::declval<void_plain_t &> ())),
                 "an int error swaps without throwing");
  static_assert (
      !noexcept (swap (std::declval<void_throwing_move_expected_t &> (),
                       std::declval<void_throwing_move_expected_t &> ())),
      "a throwing move of the error makes the swap throwing");
  SUCCEED ();
}

TEST (ExpectedConversionTest, Swap_MemberAndFree_ExchangeEveryPairOfStates)
{
  expected<tracked_t, tracked_t> value_a (in_place, 1);
  expected<tracked_t, tracked_t> value_b (in_place, 2);
  expected<tracked_t, tracked_t> error_a (unexpect, 3);
  expected<tracked_t, tracked_t> error_b (unexpect, 4);

  value_a.swap (value_b);
  swap (error_a, error_b);
  value_b.swap (error_a);

  EXPECT_EQ (value_a.value ().id, 2);
  ASSERT_FALSE (value_b.has_value ()) << "value_b took the error of error_a";
  EXPECT_EQ (value_b.error ().id, 4);
  ASSERT_TRUE (error_a.has_value ());
  EXPECT_EQ (error_a.value ().id, 1);
  ASSERT_FALSE (error_b.has_value ());
  EXPECT_EQ (error_b.error ().id, 3);
}

TEST (ExpectedConversionTest,
      MoveAssignmentNoexcept_FollowsMoveAndMoveAssignOfTheContents)
{
  using throwing_move_expected_t = expected<throwing_move_t, int>;
  using throwing_error_expected_t = expected<int, throwing_move_t>;
  using void_throwing_expected_t = expected<void, throwing_move_t>;

  static_assert (std::is_nothrow_move_assignable<expected<int, int>>::value,
                 "ints move without throwing");
  static_assert (std::is_nothrow_move_assignable<expected<void, int>>::value,
                 "an int error moves without throwing");
  static_assert (std::is_nothrow_move_assignable<
                     expected<std::string, std::string>>::value,
                 "a string moves without throwing");
  static_assert (
      !std::is_nothrow_move_assignable<throwing_move_expected_t>::value,
      "a throwing move of the value makes the assignment throwing");
  static_assert (
      !std::is_nothrow_move_assignable<throwing_error_expected_t>::value,
      "a throwing move of the error makes the assignment throwing");
  static_assert (
      !std::is_nothrow_move_assignable<void_throwing_expected_t>::value,
      "a throwing move of the error makes the assignment throwing");
  SUCCEED ();
}
