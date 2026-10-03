#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

// Monadic operations whose function changes the value type or the error type.
// The source is compiled into every suite of the module (C++17 and C++20), so
// the SFINAE branch and the concepts branch of Expected.hpp and
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

/// Builds the error text of the or_else functions: "<category>:<error>".
std::string
describe (char const *category, int error)
{
  return std::string (category) + ":" + std::to_string (error);
}

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
