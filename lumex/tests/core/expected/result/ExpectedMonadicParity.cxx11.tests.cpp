// The function argument of and_then, transform, or_else and transform_error,
// as std::expected takes it: a forwarding reference, called as by std::invoke.
// A function object keeps its value category, one that cannot be copied is not
// copied, and a pointer to a member function or to a data member works. The
// source is compiled into every suite of the module (C++11, C++17 and C++20),
// so std::invoke (C++17) and the C++11 stand-in must agree.

#include <functional>
#include <memory>
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

/// Answers 1 to 4 by the way the call operator was reached.
struct category_function_t
{
  int
  operator() (int) &
  {
    return 1;
  }
  int
  operator() (int) const &
  {
    return 2;
  }
  int
  operator() (int) &&
  {
    return 3;
  }
  int
  operator() (int) const &&
  {
    return 4;
  }
};

/// The same, returning an expected, for and_then.
struct category_expected_function_t
{
  expected<int, int>
  operator() (int) &
  {
    return expected<int, int> (1);
  }
  expected<int, int>
  operator() (int) const &
  {
    return expected<int, int> (2);
  }
  expected<int, int>
  operator() (int) &&
  {
    return expected<int, int> (3);
  }
  expected<int, int>
  operator() (int) const &&
  {
    return expected<int, int> (4);
  }
};

/// The same for or_else (it takes the error).
struct category_error_function_t
{
  expected<int, int>
  operator() (int) &
  {
    return expected<int, int> (1);
  }
  expected<int, int>
  operator() (int) const &
  {
    return expected<int, int> (2);
  }
  expected<int, int>
  operator() (int) &&
  {
    return expected<int, int> (3);
  }
  expected<int, int>
  operator() (int) const &&
  {
    return expected<int, int> (4);
  }
};

/// A function object that cannot be copied.
struct non_copyable_function_t
{
  int *calls;

  explicit non_copyable_function_t (int *counter) : calls (counter) {}
  non_copyable_function_t (non_copyable_function_t &&) = default;
  non_copyable_function_t (non_copyable_function_t const &) = delete;
  non_copyable_function_t &operator= (non_copyable_function_t const &)
      = delete;

  int
  operator() (int value) const
  {
    ++*calls;
    return value + 1;
  }
};

struct widget_t
{
  int counter;
  expected<int, int> field;

  widget_t () : counter (0), field (11) {}

  int
  size () const
  {
    return 5;
  }

  int
  bump ()
  {
    return ++counter;
  }

  expected<int, int>
  make () const
  {
    return expected<int, int> (3);
  }
};

int
twice (int value)
{
  return 2 * value;
}

} // namespace

// === the function keeps its value category ===============================

TEST (ExpectedMonadicParityTest,
      Transform_FunctionObject_KeepsItsValueCategory)
{
  expected<int, int> const uut (kValue);
  category_function_t function;
  category_function_t const const_function;

  EXPECT_EQ (uut.transform (function).value (), 1);
  EXPECT_EQ (uut.transform (const_function).value (), 2);
  EXPECT_EQ (uut.transform (category_function_t ()).value (), 3);
  EXPECT_EQ (uut.transform (std::move (const_function)).value (), 4);
}

TEST (ExpectedMonadicParityTest, AndThen_FunctionObject_KeepsItsValueCategory)
{
  expected<int, int> const uut (kValue);
  category_expected_function_t function;
  category_expected_function_t const const_function;

  EXPECT_EQ (uut.and_then (function).value (), 1);
  EXPECT_EQ (uut.and_then (const_function).value (), 2);
  EXPECT_EQ (uut.and_then (category_expected_function_t ()).value (), 3);
  EXPECT_EQ (uut.and_then (std::move (const_function)).value (), 4);
}

TEST (ExpectedMonadicParityTest, OrElse_FunctionObject_KeepsItsValueCategory)
{
  expected<int, int> const uut (unexpect, kError);
  category_error_function_t function;
  category_error_function_t const const_function;

  EXPECT_EQ (uut.or_else (function).value (), 1);
  EXPECT_EQ (uut.or_else (const_function).value (), 2);
  EXPECT_EQ (uut.or_else (category_error_function_t ()).value (), 3);
  EXPECT_EQ (uut.or_else (std::move (const_function)).value (), 4);
}

TEST (ExpectedMonadicParityTest,
      TransformError_FunctionObject_KeepsItsValueCategory)
{
  expected<int, int> const uut (unexpect, kError);
  category_function_t function;
  category_function_t const const_function;

  EXPECT_EQ (uut.transform_error (function).error (), 1);
  EXPECT_EQ (uut.transform_error (const_function).error (), 2);
  EXPECT_EQ (uut.transform_error (category_function_t ()).error (), 3);
  EXPECT_EQ (uut.transform_error (std::move (const_function)).error (), 4);
}

TEST (ExpectedMonadicParityTest,
      VoidSpecialization_FunctionObject_KeepsItsValueCategory)
{
  struct no_argument_function_t
  {
    int
    operator() () &
    {
      return 1;
    }
    int
    operator() () const &
    {
      return 2;
    }
    int
    operator() () &&
    {
      return 3;
    }
    int
    operator() () const &&
    {
      return 4;
    }
  };
  expected<void, int> const uut;
  no_argument_function_t function;
  no_argument_function_t const const_function;

  EXPECT_EQ (uut.transform (function).value (), 1);
  EXPECT_EQ (uut.transform (const_function).value (), 2);
  EXPECT_EQ (uut.transform (no_argument_function_t ()).value (), 3);
  EXPECT_EQ (uut.transform (std::move (const_function)).value (), 4);
}

// === the function is not copied ==========================================

TEST (ExpectedMonadicParityTest, Transform_NonCopyableFunction_IsNotCopied)
{
  int calls = 0;
  non_copyable_function_t function (&calls);
  expected<int, int> const uut (kValue);

  auto by_reference = uut.transform (function);
  auto by_move = uut.transform (std::move (function));

  EXPECT_EQ (by_reference.value (), kValue + 1);
  EXPECT_EQ (by_move.value (), kValue + 1);
  EXPECT_EQ (calls, 2);
}

TEST (ExpectedMonadicParityTest, AllOperations_NonCopyableFunction_AreAccepted)
{
  int calls = 0;
  non_copyable_function_t function (&calls);
  expected<int, int> value_uut (kValue);
  expected<int, int> error_uut (unexpect, kError);

  EXPECT_EQ (value_uut.transform (function).value (), kValue + 1);
  EXPECT_EQ (error_uut.transform_error (function).error (), kError + 1);
  EXPECT_EQ (calls, 2);
}

// === pointers to members =================================================

TEST (ExpectedMonadicParityTest,
      Transform_PointerToConstMemberFunction_CallsItOnTheValue)
{
  expected<widget_t, int> const uut{ widget_t () };

  auto result = uut.transform (&widget_t::size);

  static_assert (std::is_same<decltype (result), expected<int, int>>::value,
                 "the result is that of the member function");
  EXPECT_EQ (result.value (), 5);
}

TEST (ExpectedMonadicParityTest,
      Transform_PointerToMemberFunction_CanChangeTheValue)
{
  expected<widget_t, int> uut{ widget_t () };

  auto first = uut.transform (&widget_t::bump);
  auto second = uut.transform (&widget_t::bump);

  EXPECT_EQ (first.value (), 1);
  EXPECT_EQ (second.value (), 2);
  EXPECT_EQ (uut.value ().counter, 2);
}

TEST (ExpectedMonadicParityTest,
      Transform_PointerToMemberFunction_WorksThroughAPointer)
{
  widget_t widget;
  expected<widget_t *, int> const uut (&widget);

  auto result = uut.transform (&widget_t::size);

  EXPECT_EQ (result.value (), 5);
}

TEST (ExpectedMonadicParityTest,
      Transform_PointerToMemberFunction_WorksThroughASmartPointer)
{
  expected<std::unique_ptr<widget_t>, int> const uut{
    std::unique_ptr<widget_t> (new widget_t ())
  };

  auto result = uut.transform (&widget_t::size);

  EXPECT_EQ (result.value (), 5);
}

TEST (ExpectedMonadicParityTest,
      Transform_PointerToMemberFunction_WorksThroughAReferenceWrapper)
{
  widget_t widget;
  expected<std::reference_wrapper<widget_t>, int> uut{ std::ref (widget) };

  auto result = uut.transform (&widget_t::bump);

  EXPECT_EQ (result.value (), 1);
  EXPECT_EQ (widget.counter, 1) << "the call reached the wrapped object";
}

TEST (ExpectedMonadicParityTest,
      AndThen_PointerToMemberFunction_ReturnsItsExpected)
{
  expected<widget_t, int> const uut{ widget_t () };

  auto result = uut.and_then (&widget_t::make);

  static_assert (std::is_same<decltype (result), expected<int, int>>::value,
                 "the result is the expected of the member function");
  EXPECT_EQ (result.value (), 3);
}

TEST (ExpectedMonadicParityTest,
      AndThen_PointerToDataMember_ReturnsACopyOfTheMember)
{
  expected<widget_t, int> const uut{ widget_t () };

  auto result = uut.and_then (&widget_t::field);

  static_assert (
      std::is_same<decltype (result), expected<int, int>>::value,
      "the result of and_then is a plain expected, not a reference");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 11);
}

// === other things that can be called =====================================

TEST (ExpectedMonadicParityTest,
      Transform_FunctionAndFunctionPointer_AreCalled)
{
  expected<int, int> const uut (kValue);

  EXPECT_EQ (uut.transform (twice).value (), 2 * kValue);
  EXPECT_EQ (uut.transform (&twice).value (), 2 * kValue);
}

TEST (ExpectedMonadicParityTest, Transform_StdFunction_IsCalled)
{
  std::function<int (int)> const function
      = [] (int value) { return value + 3; };
  expected<int, int> const uut (kValue);

  EXPECT_EQ (uut.transform (function).value (), kValue + 3);
}

// === the type of the result ==============================================

TEST (ExpectedMonadicParityTest, Transform_ConstResult_DropsTheConst)
{
  expected<int, int> const uut (kValue);

  auto result = uut.transform ([] (int) -> std::string const
                                 { return std::string ("text"); });

  static_assert (
      std::is_same<decltype (result), expected<std::string, int>>::value,
      "the value type is the result type without const");
  EXPECT_EQ (result.value (), "text");
}

TEST (ExpectedMonadicParityTest,
      AndThen_ConstResultAndReferenceResult_GiveAPlainExpected)
{
  expected<int, int> const uut (kValue);
  expected<int, int> stored (1);

  auto from_const = uut.and_then ([] (int) -> expected<int, int> const
                                    { return expected<int, int> (2); });
  auto from_reference = uut.and_then ([&stored] (int) -> expected<int, int> &
                                        { return stored; });

  static_assert (
      std::is_same<decltype (from_const), expected<int, int>>::value,
      "the result is the expected without const");
  static_assert (
      std::is_same<decltype (from_reference), expected<int, int>>::value,
      "the result is the expected without the reference");
  EXPECT_EQ (from_const.value (), 2);
  EXPECT_EQ (from_reference.value (), 1);
}

TEST (ExpectedMonadicParityTest, OrElse_ConstResult_GivesAPlainExpected)
{
  expected<int, int> const uut (unexpect, kError);

  auto result = uut.or_else ([] (int) -> expected<int, std::string> const
                               { return expected<int, std::string> (5); });

  static_assert (
      std::is_same<decltype (result), expected<int, std::string>>::value,
      "the result is the expected without const");
  EXPECT_EQ (result.value (), 5);
}

TEST (ExpectedMonadicParityTest, TransformError_ConstResult_DropsTheConst)
{
  expected<int, int> const uut (unexpect, kError);

  auto result = uut.transform_error ([] (int) -> std::string const
                                       { return std::string ("failed"); });

  static_assert (
      std::is_same<decltype (result), expected<int, std::string>>::value,
      "the error type is the result type without const");
  EXPECT_EQ (result.error (), "failed");
}

// === a result is built in place ==========================================

TEST (ExpectedMonadicParityTest, Transform_Result_IsBuiltInPlace)
{
  expected<int, int> const uut (kValue);

  auto result = uut.transform ([] (int value) { return tracked_t (value); });

  EXPECT_EQ (result.value ().id, kValue);
  EXPECT_EQ (result.value ().copies, 0) << "the result is never copied";
  EXPECT_EQ (result.value ().moves, 0) << "the result is built in place";
}

TEST (ExpectedMonadicParityTest, TransformError_Result_IsBuiltInPlace)
{
  expected<int, int> const uut (unexpect, kError);

  auto result
      = uut.transform_error ([] (int value) { return tracked_t (value); });

  EXPECT_EQ (result.error ().id, kError);
  EXPECT_EQ (result.error ().copies, 0) << "the result is never copied";
  EXPECT_EQ (result.error ().moves, 0) << "the result is built in place";
}
