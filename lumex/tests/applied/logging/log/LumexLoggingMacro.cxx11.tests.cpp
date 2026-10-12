// LumexLoggingMacro.cxx11.tests.cpp
// LumexLoggingMacro.hpp provides LUMEX_FUNCTION_NAME through LumexMacros.hpp,
// so the logging macros and the rest of the library name a function the same
// way. This translation unit includes only the logging macro header.
#include <string>

#include <gtest/gtest.h>

#include "lumex/applied/logging/log/LumexLoggingMacro.hpp"

namespace
{
std::string
logging_function_name_probe ()
{
  return LUMEX_FUNCTION_NAME;
}
} // namespace

TEST (LumexLoggingMacroTest,
      GivenTheLoggingMacroHeader_WhenFunctionNameIsUsed_ThenTheNameIsInIt)
{
  // Act
  std::string const name = logging_function_name_probe ();

  // Assert
  EXPECT_NE (std::string::npos, name.find ("logging_function_name_probe"))
      << name;
}

TEST (
    LumexLoggingMacroTest,
    GivenTheLoggingMacroHeader_WhenFunctionNameIsChosen_ThenTheCompilerDecides)
{
#if defined(_MSC_VER)
  char const *const expected = "__FUNCSIG__";
#elif defined(__GNUC__) || defined(__clang__)
  char const *const expected = "__PRETTY_FUNCTION__";
#else
  char const *const expected = "__func__";
#endif

  EXPECT_STREQ (expected, LUMEX_STRINGIZE (LUMEX_FUNCTION_NAME));
}
