// transform and transform_error of expected<T, E> and expected<void, E> with a
// function whose result is `void` or an `expected`, as std::expected allows
// them (the result is expected<void, E> and expected<expected<U, G>, E>). The
// source is compiled into every suite of the module (C++11, C++17 and C++20),
// so the SFINAE branch and the concepts branch must give the same types and
// the same results. Each function below accepts only the value category of the
// overload it is meant for, so a call that passes the argument in another
// category does not compile. The error and value types are tracked_t, which
// tells a copy from a move.

#include <memory>
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

using both_t = expected<tracked_t, tracked_t>;
using void_both_t = expected<void, tracked_t>;

} // namespace

// === expected<T, E> =====================================================

TEST (ExpectedTransformTest,
      VoidResult_LValue_ValueCallsFunctionAndGivesVoidSuccess)
{
  int calls = 0;
  int seen = 0;
  both_t uut (in_place, kValue);

  auto result = uut.transform (
      [&calls, &seen] (tracked_t &value)
        {
          ++calls;
          seen = value.id;
        });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (seen, kValue);
}

TEST (ExpectedTransformTest, VoidResult_LValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  both_t uut (unexpect, kError);

  auto result = uut.transform ([&calls] (tracked_t &) { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedTransformTest, ExpectedResult_LValue_ValueIsWrappedNotFlattened)
{
  int calls = 0;
  both_t uut (in_place, kValue);

  auto result = uut.transform (
      [&calls] (tracked_t &value)
        {
          ++calls;
          return inner_t (std::to_string (value.id));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "7");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest,
      ExpectedResult_LValue_InnerErrorStaysInsideOuterSuccess)
{
  both_t uut (in_place, kValue);

  auto result = uut.transform ([] (tracked_t &)
                                 { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedTransformTest,
      ExpectedResult_LValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  both_t uut (unexpect, kError);

  auto result = uut.transform (
      [&calls] (tracked_t &)
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedTransformTest,
      TransformErrorExpectedResult_LValue_ErrorIsWrapped)
{
  int calls = 0;
  both_t uut (unexpect, kError);

  auto result = uut.transform_error (
      [&calls] (tracked_t &error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest, TransformErrorExpectedResult_LValue_ValueIsKept)
{
  int calls = 0;
  both_t uut (in_place, kValue);

  auto result = uut.transform_error (
      [&calls] (tracked_t &)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.value ().id, kValue);
  EXPECT_GE (result.value ().copies, 1);
  EXPECT_EQ (uut.value ().id, kValue) << "the value must be copied";
}

TEST (ExpectedTransformTest,
      VoidResult_ConstLValue_ValueCallsFunctionAndGivesVoidSuccess)
{
  int calls = 0;
  int seen = 0;
  const both_t uut (in_place, kValue);

  auto result = uut.transform (
      [&calls, &seen] (tracked_t const &value)
        {
          ++calls;
          seen = value.id;
        });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (seen, kValue);
}

TEST (ExpectedTransformTest,
      VoidResult_ConstLValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const both_t uut (unexpect, kError);

  auto result = uut.transform ([&calls] (tracked_t const &) { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedTransformTest,
      ExpectedResult_ConstLValue_ValueIsWrappedNotFlattened)
{
  int calls = 0;
  const both_t uut (in_place, kValue);

  auto result = uut.transform (
      [&calls] (tracked_t const &value)
        {
          ++calls;
          return inner_t (std::to_string (value.id));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "7");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest,
      ExpectedResult_ConstLValue_InnerErrorStaysInsideOuterSuccess)
{
  const both_t uut (in_place, kValue);

  auto result = uut.transform ([] (tracked_t const &)
                                 { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedTransformTest,
      ExpectedResult_ConstLValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const both_t uut (unexpect, kError);

  auto result = uut.transform (
      [&calls] (tracked_t const &)
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedTransformTest,
      TransformErrorExpectedResult_ConstLValue_ErrorIsWrapped)
{
  int calls = 0;
  const both_t uut (unexpect, kError);

  auto result = uut.transform_error (
      [&calls] (tracked_t const &error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest,
      TransformErrorExpectedResult_ConstLValue_ValueIsKept)
{
  int calls = 0;
  const both_t uut (in_place, kValue);

  auto result = uut.transform_error (
      [&calls] (tracked_t const &)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.value ().id, kValue);
  EXPECT_GE (result.value ().copies, 1);
  EXPECT_EQ (uut.value ().id, kValue) << "the value must be copied";
}

TEST (ExpectedTransformTest,
      VoidResult_RValue_ValueCallsFunctionAndGivesVoidSuccess)
{
  int calls = 0;
  int seen = 0;
  both_t uut (in_place, kValue);

  auto result = std::move (uut).transform (
      [&calls, &seen] (tracked_t &&value)
        {
          ++calls;
          seen = value.id;
        });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (seen, kValue);
}

TEST (ExpectedTransformTest, VoidResult_RValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  both_t uut (unexpect, kError);

  auto result
      = std::move (uut).transform ([&calls] (tracked_t &&) { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_EQ (result.error ().copies, 0);
  EXPECT_GE (result.error ().moves, 1);
  EXPECT_EQ (uut.error ().id, -1) << "the error must be moved from";
}

TEST (ExpectedTransformTest, ExpectedResult_RValue_ValueIsWrappedNotFlattened)
{
  int calls = 0;
  both_t uut (in_place, kValue);

  auto result = std::move (uut).transform (
      [&calls] (tracked_t &&value)
        {
          ++calls;
          return inner_t (std::to_string (value.id));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "7");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest,
      ExpectedResult_RValue_InnerErrorStaysInsideOuterSuccess)
{
  both_t uut (in_place, kValue);

  auto result = std::move (uut).transform (
      [] (tracked_t &&) { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedTransformTest,
      ExpectedResult_RValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  both_t uut (unexpect, kError);

  auto result = std::move (uut).transform (
      [&calls] (tracked_t &&)
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_EQ (result.error ().copies, 0);
  EXPECT_GE (result.error ().moves, 1);
  EXPECT_EQ (uut.error ().id, -1) << "the error must be moved from";
}

TEST (ExpectedTransformTest,
      TransformErrorExpectedResult_RValue_ErrorIsWrapped)
{
  int calls = 0;
  both_t uut (unexpect, kError);

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t &&error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest, TransformErrorExpectedResult_RValue_ValueIsKept)
{
  int calls = 0;
  both_t uut (in_place, kValue);

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t &&)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.value ().id, kValue);
  EXPECT_EQ (result.value ().copies, 0);
  EXPECT_GE (result.value ().moves, 1);
  EXPECT_EQ (uut.value ().id, -1) << "the value must be moved from";
}

TEST (ExpectedTransformTest,
      VoidResult_ConstRValue_ValueCallsFunctionAndGivesVoidSuccess)
{
  int calls = 0;
  int seen = 0;
  const both_t uut (in_place, kValue);

  auto result = std::move (uut).transform (
      [&calls, &seen] (tracked_t const &&value)
        {
          ++calls;
          seen = value.id;
        });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (seen, kValue);
}

TEST (ExpectedTransformTest,
      VoidResult_ConstRValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const both_t uut (unexpect, kError);

  auto result
      = std::move (uut).transform ([&calls] (tracked_t const &&) { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedTransformTest,
      ExpectedResult_ConstRValue_ValueIsWrappedNotFlattened)
{
  int calls = 0;
  const both_t uut (in_place, kValue);

  auto result = std::move (uut).transform (
      [&calls] (tracked_t const &&value)
        {
          ++calls;
          return inner_t (std::to_string (value.id));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "7");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest,
      ExpectedResult_ConstRValue_InnerErrorStaysInsideOuterSuccess)
{
  const both_t uut (in_place, kValue);

  auto result = std::move (uut).transform (
      [] (tracked_t const &&) { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedTransformTest,
      ExpectedResult_ConstRValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const both_t uut (unexpect, kError);

  auto result = std::move (uut).transform (
      [&calls] (tracked_t const &&)
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedTransformTest,
      TransformErrorExpectedResult_ConstRValue_ErrorIsWrapped)
{
  int calls = 0;
  const both_t uut (unexpect, kError);

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t const &&error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedTransformTest,
      TransformErrorExpectedResult_ConstRValue_ValueIsKept)
{
  int calls = 0;
  const both_t uut (in_place, kValue);

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t const &&)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<tracked_t, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.value ().id, kValue);
  EXPECT_GE (result.value ().copies, 1);
  EXPECT_EQ (uut.value ().id, kValue) << "the value must be copied";
}

// === expected<void, E> ===================================================

TEST (ExpectedVoidTransformTest, VoidResult_LValue_SuccessCallsFunction)
{
  int calls = 0;
  void_both_t uut;

  auto result = uut.transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      VoidResult_LValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  void_both_t uut (unexpect, kError);

  auto result = uut.transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_LValue_SuccessIsWrappedNotFlattened)
{
  int calls = 0;
  void_both_t uut;

  auto result = uut.transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("inner"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "inner");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_LValue_InnerErrorStaysInsideOuterSuccess)
{
  void_both_t uut;

  auto result = uut.transform ([] { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_LValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  void_both_t uut (unexpect, kError);

  auto result = uut.transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_LValue_ErrorIsWrapped)
{
  int calls = 0;
  void_both_t uut (unexpect, kError);

  auto result = uut.transform_error (
      [&calls] (tracked_t &error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_LValue_SuccessIsKept)
{
  int calls = 0;
  void_both_t uut;

  auto result = uut.transform_error (
      [&calls] (tracked_t &)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
}

TEST (ExpectedVoidTransformTest, VoidResult_ConstLValue_SuccessCallsFunction)
{
  int calls = 0;
  const void_both_t uut;

  auto result = uut.transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      VoidResult_ConstLValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const void_both_t uut (unexpect, kError);

  auto result = uut.transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_ConstLValue_SuccessIsWrappedNotFlattened)
{
  int calls = 0;
  const void_both_t uut;

  auto result = uut.transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("inner"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "inner");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_ConstLValue_InnerErrorStaysInsideOuterSuccess)
{
  const void_both_t uut;

  auto result = uut.transform ([] { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_ConstLValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const void_both_t uut (unexpect, kError);

  auto result = uut.transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_ConstLValue_ErrorIsWrapped)
{
  int calls = 0;
  const void_both_t uut (unexpect, kError);

  auto result = uut.transform_error (
      [&calls] (tracked_t const &error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_ConstLValue_SuccessIsKept)
{
  int calls = 0;
  const void_both_t uut;

  auto result = uut.transform_error (
      [&calls] (tracked_t const &)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
}

TEST (ExpectedVoidTransformTest, VoidResult_RValue_SuccessCallsFunction)
{
  int calls = 0;
  void_both_t uut;

  auto result = std::move (uut).transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      VoidResult_RValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  void_both_t uut (unexpect, kError);

  auto result = std::move (uut).transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_EQ (result.error ().copies, 0);
  EXPECT_GE (result.error ().moves, 1);
  EXPECT_EQ (uut.error ().id, -1) << "the error must be moved from";
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_RValue_SuccessIsWrappedNotFlattened)
{
  int calls = 0;
  void_both_t uut;

  auto result = std::move (uut).transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("inner"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "inner");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_RValue_InnerErrorStaysInsideOuterSuccess)
{
  void_both_t uut;

  auto result
      = std::move (uut).transform ([] { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_RValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  void_both_t uut (unexpect, kError);

  auto result = std::move (uut).transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_EQ (result.error ().copies, 0);
  EXPECT_GE (result.error ().moves, 1);
  EXPECT_EQ (uut.error ().id, -1) << "the error must be moved from";
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_RValue_ErrorIsWrapped)
{
  int calls = 0;
  void_both_t uut (unexpect, kError);

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t &&error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_RValue_SuccessIsKept)
{
  int calls = 0;
  void_both_t uut;

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t &&)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
}

TEST (ExpectedVoidTransformTest, VoidResult_ConstRValue_SuccessCallsFunction)
{
  int calls = 0;
  const void_both_t uut;

  auto result = std::move (uut).transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      VoidResult_ConstRValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const void_both_t uut (unexpect, kError);

  auto result = std::move (uut).transform ([&calls] { ++calls; });

  static_assert (std::is_same<decltype (result), void_both_t>::value,
                 "a function that returns void gives expected<void, E>");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_ConstRValue_SuccessIsWrappedNotFlattened)
{
  int calls = 0;
  const void_both_t uut;

  auto result = std::move (uut).transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("inner"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), "inner");
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_ConstRValue_InnerErrorStaysInsideOuterSuccess)
{
  const void_both_t uut;

  auto result
      = std::move (uut).transform ([] { return inner_t (unexpect, kError); });

  ASSERT_TRUE (result.has_value ()) << "the outer expected holds a value";
  ASSERT_FALSE (result.value ().has_value ());
  EXPECT_EQ (result.value ().error (), kError);
}

TEST (ExpectedVoidTransformTest,
      ExpectedResult_ConstRValue_ErrorSkipsFunctionAndKeepsError)
{
  int calls = 0;
  const void_both_t uut (unexpect, kError);

  auto result = std::move (uut).transform (
      [&calls]
        {
          ++calls;
          return inner_t (std::string ("x"));
        });

  static_assert (
      std::is_same<decltype (result), expected<inner_t, tracked_t>>::value,
      "a function that returns an expected gives an expected of it");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (result.error ().id, kError);
  EXPECT_GE (result.error ().copies, 1);
  EXPECT_EQ (uut.error ().id, kError) << "the error must be copied";
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_ConstRValue_ErrorIsWrapped)
{
  int calls = 0;
  const void_both_t uut (unexpect, kError);

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t const &&error)
        {
          ++calls;
          return expected<int, int> (error.id + 1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_FALSE (result.has_value ());
  ASSERT_TRUE (result.error ().has_value ());
  EXPECT_EQ (result.error ().value (), kError + 1);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedVoidTransformTest,
      TransformErrorExpectedResult_ConstRValue_SuccessIsKept)
{
  int calls = 0;
  const void_both_t uut;

  auto result = std::move (uut).transform_error (
      [&calls] (tracked_t const &&)
        {
          ++calls;
          return expected<int, int> (1);
        });

  static_assert (
      std::is_same<decltype (result),
                   expected<void, expected<int, int>>>::value,
      "a function that returns an expected gives an expected error");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (calls, 0);
}

// === other result types ==================================================

TEST (ExpectedTransformTest, OtherResults_PairVectorAndSmartPointerAreValues)
{
  expected<int, std::string> const uut (kValue);

  auto pair_result = uut.transform (
      [] (int const &value) { return std::make_pair (value, value + 1); });
  auto vector_result = uut.transform (
      [] (int const &value) { return std::vector<int> (3, value); });
  auto text_result = uut.transform ([] (int const &value)
                                      { return std::to_string (value); });

  static_assert (
      std::is_same<decltype (pair_result),
                   expected<std::pair<int, int>, std::string>>::value,
      "the result type is whatever the function returns");
  static_assert (std::is_same<decltype (vector_result),
                              expected<std::vector<int>, std::string>>::value,
                 "the result type is whatever the function returns");
  EXPECT_EQ (pair_result.value ().second, kValue + 1);
  EXPECT_EQ (vector_result.value ().size (), 3u);
  EXPECT_EQ (text_result.value (), "7");
}

TEST (ExpectedTransformTest, OtherResults_MoveOnlyResultIsMovedIntoTheExpected)
{
  expected<int, std::string> uut (kValue);

  auto result = std::move (uut).transform (
      [] (int &&value) { return std::unique_ptr<int> (new int (value)); });

  static_assert (
      std::is_same<decltype (result),
                   expected<std::unique_ptr<int>, std::string>>::value,
      "the result type is whatever the function returns");
  ASSERT_TRUE (result.has_value ());
  ASSERT_NE (result.value (), nullptr);
  EXPECT_EQ (*result.value (), kValue);
}

TEST (ExpectedTransformTest, VoidAndExpectedResults_ChainThroughAndThen)
{
  int sum = 0;
  expected<int, std::string> uut (kValue);

  auto result = uut.transform ([&sum] (int value) { sum += value; })
                    .and_then (
                        [&sum]
                          {
                            sum += 1;
                            return expected<int, std::string> (sum);
                          })
                    .transform ([] (int value)
                                  { return expected<int, int> (value * 2); });

  static_assert (
      std::is_same<decltype (result),
                   expected<expected<int, int>, std::string>>::value,
      "the chain keeps the error type and nests the expected result");
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result.value ().has_value ());
  EXPECT_EQ (result.value ().value (), (kValue + 1) * 2);
}

TEST (ExpectedTransformTest, VoidResult_ErrorPassesThroughTheWholeChain)
{
  int calls = 0;
  expected<int, std::string> uut (unexpect, std::string ("early"));

  auto result = uut.transform ([&calls] (int) { ++calls; })
                    .and_then (
                        [&calls]
                          {
                            ++calls;
                            return expected<void, std::string> ();
                          });

  static_assert (
      std::is_same<decltype (result), expected<void, std::string>>::value,
      "void result, then and_then with void");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), "early");
  EXPECT_EQ (calls, 0);
}

// === what stays rejected =================================================

namespace
{

struct void_function_t
{
  void
  operator() (int const &) const
  {
  }
};

struct void_error_function_t
{
  void
  operator() (tracked_t const &) const
  {
  }
};

struct value_function_t
{
  int
  operator() (int const &) const
  {
    return 1;
  }
};

struct two_argument_function_t
{
  int
  operator() (int, int) const
  {
    return 1;
  }
};

struct plain_result_function_t
{
  int
  operator() (int const &) const
  {
    return 1;
  }
};

} // namespace

TEST (ExpectedTransformTest, Rejected_FunctionThatCannotBeCalledWithTheValue)
{
  static_assert (can_transform<expected<int, int> &, value_function_t>::value,
                 "a callable function is accepted");
  static_assert (
      !can_transform<expected<int, int> &, two_argument_function_t>::value,
      "a function with the wrong arity is rejected");
  static_assert (!can_transform<expected<int, int> &, int>::value,
                 "something that is not callable is rejected");
  static_assert (
      !can_transform<expected<void, int> &, value_function_t>::value,
      "expected<void, E> calls the function without arguments");
  SUCCEED ();
}

TEST (ExpectedTransformTest, Rejected_TransformErrorWithAVoidFunction)
{
  static_assert (!can_transform_error<expected<tracked_t, tracked_t> &,
                                      void_error_function_t>::value,
                 "void cannot be an error type: transform_error rejects it");
  static_assert (!can_transform_error<expected<void, tracked_t> &,
                                      void_error_function_t>::value,
                 "void cannot be an error type: transform_error rejects it");
  static_assert (can_transform<expected<int, int> &, void_function_t>::value,
                 "transform accepts a function that returns void");
  SUCCEED ();
}

TEST (ExpectedTransformTest, Rejected_AndThenAndOrElseNeedAnExpectedResult)
{
  static_assert (
      !can_and_then<expected<int, int> &, plain_result_function_t>::value,
      "and_then needs a function that returns an expected");
  static_assert (
      !can_or_else<expected<int, int> &, plain_result_function_t>::value,
      "or_else needs a function that returns an expected");
  static_assert (!can_and_then<expected<void, int> &, void_function_t>::value,
                 "and_then needs a function that returns an expected");
  SUCCEED ();
}

TEST (ExpectedTransformTest, Rejected_ErrorThatCannotBeCopiedOnAnLValue)
{
  using move_error_t = expected<int, move_only_t>;
  static_assert (!can_transform<move_error_t &, value_function_t>::value,
                 "transform on an lvalue needs a copyable error");
  static_assert (can_transform<move_error_t &&, value_function_t>::value,
                 "transform on an rvalue moves the error");
  SUCCEED ();
}
