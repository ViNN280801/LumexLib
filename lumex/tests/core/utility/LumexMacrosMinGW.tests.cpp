// LumexMacrosMinGW.tests.cpp
// LUMEX_FUNCTION_NAME as MinGW sees it: GCC or Clang with the Windows target
// macros _WIN32 and __MINGW32__. On a host that is not Windows this
// translation unit defines those two before it includes LumexMacros.hpp, its
// only Lumex header, and removes them right after, so GCC and Clang check
// that the choice follows the compiler and not the target: GCC has no
// __FUNCSIG__. On Windows the real target macros are already there and
// LumexMacros.tests.cpp covers the compiler in use.
#include <string>

#include <gtest/gtest.h>

#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
#define LUMEX_TESTS_SIMULATED_MINGW 1
#define _WIN32 1
#define __MINGW32__ 1
#endif

#include "lumex/core/utility/macros/LumexMacros.hpp"

#if defined(LUMEX_TESTS_SIMULATED_MINGW)
#undef _WIN32
#undef __MINGW32__

namespace
{
std::string
mingw_function_name_probe ()
{
  return LUMEX_FUNCTION_NAME;
}
} // namespace

TEST (LumexMacrosMinGWTest,
      GivenMinGWTargetMacros_WhenFunctionNameIsChosen_ThenItIsPrettyFunction)
{
  EXPECT_STREQ ("__PRETTY_FUNCTION__", LUMEX_STRINGIZE (LUMEX_FUNCTION_NAME));
}

TEST (LumexMacrosMinGWTest,
      GivenMinGWTargetMacros_WhenFunctionNameIsUsed_ThenTheNameIsInIt)
{
  // Act
  std::string const name = mingw_function_name_probe ();

  // Assert
  EXPECT_NE (std::string::npos, name.find ("mingw_function_name_probe"))
      << name;
}
#endif
