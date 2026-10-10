// LUMEX_EXPECTED_HAS_EXCEPTIONS (ExpectedDetail.hpp): the library reads the
// compiler's switch (`__cpp_exceptions`, `__EXCEPTIONS`, `_CPPUNWIND`). These
// suites are built with exceptions, so the macro must be 1 and value () of an
// error must throw bad_expected_access, with the error in it. The build
// without exceptions is tested by cmake.consumer_expected_no_exceptions_*
// (lumex/tests/cmake/consumer/expected_no_exceptions).

#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define EXPECTED_TESTS_COMPILER_HAS_EXCEPTIONS 1
#else
#define EXPECTED_TESTS_COMPILER_HAS_EXCEPTIONS 0
#endif

static_assert (LUMEX_EXPECTED_HAS_EXCEPTIONS
                   == EXPECTED_TESTS_COMPILER_HAS_EXCEPTIONS,
               "the library follows the compiler's exception switch");
static_assert (LUMEX_EXPECTED_HAS_EXCEPTIONS == 1,
               "these suites are built with exceptions");

namespace
{
using lumex::core::expected::error::bad_expected_access;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::unexpect;
} // namespace

TEST (ExpectedExceptionsSwitchTest, ValueOfAnError_ThrowsBadExpectedAccess)
{
  expected<int, std::string> bad (unexpect, "boom");
  try
    {
      static_cast<void> (bad.value ());
      FAIL () << "value () did not throw";
    }
  catch (bad_expected_access<std::string> const &caught)
    {
      EXPECT_EQ ("boom", caught.error ());
    }
}

TEST (ExpectedExceptionsSwitchTest, VoidValueOfAnError_ThrowsBadExpectedAccess)
{
  expected<void, int> bad (unexpect, 4);
  EXPECT_THROW (bad.value (), bad_expected_access<int>);
  EXPECT_THROW (std::move (bad).value (), bad_expected_access<int>);
}
