// LumexExceptionMacros.tests.cpp
// LUMEX_DEFINE_EXCEPTION / LUMEX_DEFINE_EXCEPTION_WITH_BODY on any base with
// string constructors (std::runtime_error here), independent of the
// exceptions module.
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/macros/LumexExceptionMacros.hpp"

namespace exception_macros_test
{
LUMEX_DEFINE_EXCEPTION (PlainError, std::runtime_error)

LUMEX_DEFINE_EXCEPTION_WITH_BODY (
    CodedError, std::runtime_error,
    CodedError (std::string const &message,
                int code) : std::runtime_error (message),
    _code (code) {}

    int code () const { return _code; }

    private : int _code
    = 0;)

LUMEX_DEFINE_EXCEPTION (DerivedError, PlainError)
} // namespace exception_macros_test

using exception_macros_test::CodedError;
using exception_macros_test::DerivedError;
using exception_macros_test::PlainError;

TEST (LumexExceptionMacrosTest,
      GivenDefinedException_WhenBuilt_ThenMessageKept)
{
  EXPECT_STREQ (PlainError ("literal").what (), "literal");
  std::string const text = "lvalue";
  EXPECT_STREQ (PlainError (text).what (), "lvalue");
  std::string moved = "rvalue";
  EXPECT_STREQ (PlainError (std::move (moved)).what (), "rvalue");
}

TEST (LumexExceptionMacrosTest,
      GivenDefinedException_WhenThrown_ThenCaughtAsBase)
{
  EXPECT_TRUE ((std::is_base_of<std::runtime_error, PlainError>::value));
  EXPECT_THROW (throw PlainError ("x"), std::runtime_error);
  EXPECT_THROW (throw DerivedError ("x"), PlainError);
  EXPECT_THROW (throw DerivedError ("x"), std::exception);
}

TEST (LumexExceptionMacrosTest, GivenBody_WhenExtraMembers_ThenAvailable)
{
  CodedError const coded (std::string ("with code"), 7);
  EXPECT_STREQ (coded.what (), "with code");
  EXPECT_EQ (coded.code (), 7);
  // The macro constructors still exist next to the extra ones.
  CodedError const plain ("plain");
  EXPECT_EQ (plain.code (), 0);
}

TEST (LumexExceptionMacrosTest, GivenMessageTypes_WhenConvertible_ThenImplicit)
{
  EXPECT_TRUE ((std::is_convertible<char const *, PlainError>::value));
  EXPECT_TRUE ((std::is_convertible<std::string, PlainError>::value));
  EXPECT_FALSE ((std::is_default_constructible<PlainError>::value));
}
