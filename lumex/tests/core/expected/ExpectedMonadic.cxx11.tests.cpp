#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

// Monadic operations whose function changes the value type or the error type.
// The source is compiled into every suite of the module (C++11, C++17 and
// C++20), so the SFINAE branch and the concepts branch of Expected.hpp and
// ExpectedVoid.hpp must give the same types and the same results. Each
// function below accepts only the value category of the overload it is meant
// for, so a call that passes the argument in another category does not
// compile.

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

namespace
{

constexpr int kValue = 7;
constexpr int kError = 42;

/// Builds the text the functions return: "<category>:<argument>".
std::string
describe (char const *category, int argument)
{
  return std::string (category) + ":" + std::to_string (argument);
}

/// The new error type of transform_error; not convertible to or from int.
struct error_info_t
{
  std::string category;
  int code;
};

} // namespace

// === or_else of Expected<T, E> with another error type ===================

TEST (ExpectedMonadicTest, OrElseLValue_NewErrorType_ErrorPathCallsFunction)
{
  using ResultType = Expected<int, std::string>;
  auto func = [] (int &error)
    { return ResultType (unexpect, describe ("lvalue", error)); };

  Expected<int, int> uut (unexpect, kError);
  auto result = uut.or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), "lvalue:42");
}

TEST (ExpectedMonadicTest, OrElseLValue_NewErrorType_SuccessPathKeepsValue)
{
  using ResultType = Expected<int, std::string>;
  int calls = 0;
  auto func = [&calls] (int &error)
    {
      ++calls;
      return ResultType (unexpect, describe ("lvalue", error));
    };

  Expected<int, int> uut (kValue);
  auto result = uut.or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, kValue);
  EXPECT_EQ (calls, 0);
}

TEST (ExpectedMonadicTest,
      OrElseConstLValue_NewErrorType_ErrorPathCallsFunction)
{
  using ResultType = Expected<int, std::string>;
  auto func = [] (int const &error)
    { return ResultType (unexpect, describe ("const_lvalue", error)); };

  Expected<int, int> const uut (unexpect, kError);
  auto result = uut.or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), "const_lvalue:42");
}

TEST (ExpectedMonadicTest,
      OrElseConstLValue_NewErrorType_SuccessPathKeepsValue)
{
  using ResultType = Expected<int, std::string>;
  int calls = 0;
  auto func = [&calls] (int const &error)
    {
      ++calls;
      return ResultType (unexpect, describe ("const_lvalue", error));
    };

  Expected<int, int> const uut (kValue);
  auto result = uut.or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, kValue);
  EXPECT_EQ (calls, 0);
}

TEST (ExpectedMonadicTest, OrElseRValue_NewErrorType_ErrorPathCallsFunction)
{
  using ResultType = Expected<int, std::string>;
  auto func = [] (int &&error)
    { return ResultType (unexpect, describe ("rvalue", error)); };

  Expected<int, int> uut (unexpect, kError);
  auto result = std::move (uut).or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), "rvalue:42");
}

TEST (ExpectedMonadicTest, OrElseRValue_NewErrorType_SuccessPathKeepsValue)
{
  using ResultType = Expected<std::string, std::string>;
  int calls = 0;
  auto func = [&calls] (int &&error)
    {
      ++calls;
      return ResultType (unexpect, describe ("rvalue", error));
    };

  Expected<std::string, int> uut (std::string ("value"));
  auto result = std::move (uut).or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, "value");
  EXPECT_EQ (calls, 0);
}

TEST (ExpectedMonadicTest,
      OrElseConstRValue_NewErrorType_ErrorPathCallsFunction)
{
  using ResultType = Expected<int, std::string>;
  auto func = [] (int const &&error)
    { return ResultType (unexpect, describe ("const_rvalue", error)); };

  Expected<int, int> const uut (unexpect, kError);
  auto result = std::move (uut).or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), "const_rvalue:42");
}

TEST (ExpectedMonadicTest,
      OrElseConstRValue_NewErrorType_SuccessPathKeepsValue)
{
  using ResultType = Expected<int, std::string>;
  int calls = 0;
  auto func = [&calls] (int const &&error)
    {
      ++calls;
      return ResultType (unexpect, describe ("const_rvalue", error));
    };

  Expected<int, int> const uut (kValue);
  auto result = std::move (uut).or_else (func);

  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "or_else must return the function's result type");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, kValue);
  EXPECT_EQ (calls, 0);
}

// === or_else of Expected<void, E> with another error type ================

TEST (ExpectedMonadicTest, VoidOrElse_NewErrorType_BothPathsInEveryCategory)
{
  using ResultType = Expected<void, std::string>;
  auto lvalue = [] (int &error)
    { return ResultType (unexpect, describe ("lvalue", error)); };
  auto const_lvalue = [] (int const &error)
    { return ResultType (unexpect, describe ("const_lvalue", error)); };
  auto rvalue = [] (int &&error)
    { return ResultType (unexpect, describe ("rvalue", error)); };
  auto const_rvalue = [] (int const &&error)
    { return ResultType (unexpect, describe ("const_rvalue", error)); };

  Expected<void, int> failed (unexpect, kError);
  Expected<void, int> const const_failed (unexpect, kError);
  Expected<void, int> succeeded;
  Expected<void, int> const const_succeeded;

  EXPECT_EQ (failed.or_else (lvalue).error (), "lvalue:42");
  EXPECT_EQ (const_failed.or_else (const_lvalue).error (), "const_lvalue:42");
  EXPECT_EQ (std::move (const_failed).or_else (const_rvalue).error (),
             "const_rvalue:42");
  EXPECT_EQ (std::move (failed).or_else (rvalue).error (), "rvalue:42");

  EXPECT_TRUE (succeeded.or_else (lvalue).has_value ());
  EXPECT_TRUE (const_succeeded.or_else (const_lvalue).has_value ());
  EXPECT_TRUE (
      std::move (const_succeeded).or_else (const_rvalue).has_value ());
  EXPECT_TRUE (std::move (succeeded).or_else (rvalue).has_value ());
}

// === transform of Expected<T, E> to another value type ===================

TEST (ExpectedMonadicTest, TransformLValue_IntToString_BothPaths)
{
  using ResultType = Expected<std::string, int>;
  int calls = 0;
  auto func = [&calls] (int &value)
    {
      ++calls;
      return describe ("lvalue", value);
    };

  Expected<int, int> succeeded (kValue);
  auto result_s = succeeded.transform (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, "lvalue:7");

  Expected<int, int> failed (unexpect, kError);
  auto result_e = failed.transform (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), kError);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedMonadicTest, TransformConstLValue_IntToString_BothPaths)
{
  using ResultType = Expected<std::string, int>;
  int calls = 0;
  auto func = [&calls] (int const &value)
    {
      ++calls;
      return describe ("const_lvalue", value);
    };

  Expected<int, int> const succeeded (kValue);
  auto result_s = succeeded.transform (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, "const_lvalue:7");

  Expected<int, int> const failed (unexpect, kError);
  auto result_e = failed.transform (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), kError);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedMonadicTest, TransformRValue_IntToString_BothPaths)
{
  using ResultType = Expected<std::string, int>;
  int calls = 0;
  auto func = [&calls] (int &&value)
    {
      ++calls;
      return describe ("rvalue", value);
    };

  Expected<int, int> succeeded (kValue);
  auto result_s = std::move (succeeded).transform (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, "rvalue:7");

  Expected<int, int> failed (unexpect, kError);
  auto result_e = std::move (failed).transform (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), kError);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedMonadicTest, TransformConstRValue_IntToString_BothPaths)
{
  using ResultType = Expected<std::string, int>;
  int calls = 0;
  auto func = [&calls] (int const &&value)
    {
      ++calls;
      return describe ("const_rvalue", value);
    };

  Expected<int, int> const succeeded (kValue);
  auto result_s = std::move (succeeded).transform (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, "const_rvalue:7");

  Expected<int, int> const failed (unexpect, kError);
  auto result_e = std::move (failed).transform (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), kError);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedMonadicTest, TransformRValue_StringToSize_MovesTheValueIn)
{
  using ResultType = Expected<std::size_t, int>;
  auto func = [] (std::string &&value)
    {
      std::string const taken (std::move (value));
      return taken.size ();
    };

  Expected<std::string, int> succeeded (std::string ("eleven char"));
  auto result = std::move (succeeded).transform (func);
  static_assert (std::is_same<decltype (result), ResultType>::value,
                 "transform must return Expected<U, E>");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, 11u);
}

// === transform_error of Expected<T, E> to another error type =============

TEST (ExpectedMonadicTest, TransformErrorLValue_IntToStruct_BothPaths)
{
  using ResultType = Expected<int, error_info_t>;
  int calls = 0;
  auto func = [&calls] (int &error)
    {
      ++calls;
      return error_info_t{ "lvalue", error };
    };

  Expected<int, int> failed (unexpect, kError);
  auto result_e = failed.transform_error (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error ().category, "lvalue");
  EXPECT_EQ (result_e.error ().code, kError);

  Expected<int, int> succeeded (kValue);
  auto result_s = succeeded.transform_error (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, kValue);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedMonadicTest, TransformErrorConstLValue_IntToStruct_BothPaths)
{
  using ResultType = Expected<int, error_info_t>;
  int calls = 0;
  auto func = [&calls] (int const &error)
    {
      ++calls;
      return error_info_t{ "const_lvalue", error };
    };

  Expected<int, int> const failed (unexpect, kError);
  auto result_e = failed.transform_error (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error ().category, "const_lvalue");
  EXPECT_EQ (result_e.error ().code, kError);

  Expected<int, int> const succeeded (kValue);
  auto result_s = succeeded.transform_error (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, kValue);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedMonadicTest, TransformErrorRValue_IntToStruct_BothPaths)
{
  using ResultType = Expected<int, error_info_t>;
  int calls = 0;
  auto func = [&calls] (int &&error)
    {
      ++calls;
      return error_info_t{ "rvalue", error };
    };

  Expected<int, int> failed (unexpect, kError);
  auto result_e = std::move (failed).transform_error (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error ().category, "rvalue");
  EXPECT_EQ (result_e.error ().code, kError);

  Expected<int, int> succeeded (kValue);
  auto result_s = std::move (succeeded).transform_error (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, kValue);
  EXPECT_EQ (calls, 1);
}

TEST (ExpectedMonadicTest, TransformErrorConstRValue_IntToStruct_BothPaths)
{
  using ResultType = Expected<int, error_info_t>;
  int calls = 0;
  auto func = [&calls] (int const &&error)
    {
      ++calls;
      return error_info_t{ "const_rvalue", error };
    };

  Expected<int, int> const failed (unexpect, kError);
  auto result_e = std::move (failed).transform_error (func);
  static_assert (std::is_same<decltype (result_e), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error ().category, "const_rvalue");
  EXPECT_EQ (result_e.error ().code, kError);

  Expected<int, int> const succeeded (kValue);
  auto result_s = std::move (succeeded).transform_error (func);
  static_assert (std::is_same<decltype (result_s), ResultType>::value,
                 "transform_error must return Expected<T, G>");
  ASSERT_TRUE (result_s.has_value ());
  EXPECT_EQ (*result_s, kValue);
  EXPECT_EQ (calls, 1);
}

// === transform of Expected<void, E> to a value ===========================

TEST (ExpectedMonadicTest, VoidTransform_VoidToString_BothPathsInEveryCategory)
{
  using ResultType = Expected<std::string, int>;
  int calls = 0;
  auto func = [&calls] ()
    {
      ++calls;
      return std::string ("made");
    };

  Expected<void, int> succeeded;
  Expected<void, int> const const_succeeded;
  Expected<void, int> failed (unexpect, kError);
  Expected<void, int> const const_failed (unexpect, kError);

  static_assert (
      std::is_same<decltype (succeeded.transform (func)), ResultType>::value,
      "transform must return Expected<U, E>");
  EXPECT_EQ (*succeeded.transform (func), "made");
  EXPECT_EQ (*const_succeeded.transform (func), "made");
  EXPECT_EQ (*std::move (const_succeeded).transform (func), "made");
  EXPECT_EQ (*std::move (succeeded).transform (func), "made");
  EXPECT_EQ (calls, 4);

  EXPECT_EQ (failed.transform (func).error (), kError);
  EXPECT_EQ (const_failed.transform (func).error (), kError);
  EXPECT_EQ (std::move (const_failed).transform (func).error (), kError);
  EXPECT_EQ (std::move (failed).transform (func).error (), kError);
  EXPECT_EQ (calls, 4);
}
