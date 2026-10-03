// WindowsSEHTranslator.tests.cpp
// Both branches of WindowsSEHTranslator.hpp. On Windows SET_SEH_TRANSLATOR
// installs seh_translator, an ordinary function exported by
// lumex::exceptions: this file is another translation unit than its
// definition, so it links only while the definition is not `inline`. On other
// platforms the macro is empty and the function is not declared. The
// CMake case cmake.source_seh_translator_branches reads both branches from
// the sources on any host.
#include <gtest/gtest.h>

#include "lumex/core/exceptions/crash/WindowsSEHTranslator.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

#if defined(LUMEX_OS_WINDOWS)

TEST (
    WindowsSEHTranslatorTest,
    GivenWindows_WhenSetSehTranslatorRuns_ThenTheExportedTranslatorIsInstalled)
{
  // Act
  SET_SEH_TRANSLATOR
  _se_translator_function const installed = _set_se_translator (nullptr);

  // Assert
  EXPECT_EQ (&seh_translator, installed);
}

#else

// A variable of the same name at global scope compiles only while the header
// declares no function seh_translator outside Windows.
int const seh_translator = 0;

TEST (WindowsSEHTranslatorTest,
      GivenNotWindows_WhenSetSehTranslatorIsStringized_ThenItIsEmpty)
{
  EXPECT_STREQ ("", LUMEX_STRINGIZE (SET_SEH_TRANSLATOR));
}

TEST (
    WindowsSEHTranslatorTest,
    GivenNotWindows_WhenSetSehTranslatorPrecedesAStatement_ThenOnlyTheStatementRuns)
{
  // Arrange
  int calls = 0;

  // Act
  SET_SEH_TRANSLATOR
  ++calls;

  // Assert
  EXPECT_EQ (1, calls);
}

TEST (WindowsSEHTranslatorTest,
      GivenNotWindows_WhenTheNameIsLookedUp_ThenItIsNotTheTranslatorFunction)
{
  EXPECT_EQ (0, seh_translator);
}

#endif
