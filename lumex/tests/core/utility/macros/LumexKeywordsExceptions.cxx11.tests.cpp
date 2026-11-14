// LumexKeywordsExceptions.cxx11.tests.cpp
// LUMEX_COMPILER_HAS_EXCEPTIONS and LUMEX_HAS_EXCEPTIONS of LumexKeywords.hpp
// and the macros of the modules that default to them. The tests are built
// with exceptions and without a definition of any of the macros: the switch
// follows the compiler and every module follows the switch (the fixtures
// cmake.consumer_exceptions_switch_* define them).
#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/optional/opt/LumexOptional.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/string_view/LumexStringView"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

TEST (LumexKeywordsExceptionsTest,
      GivenTheTestBuild_WhenRead_ThenTheCompilerHasExceptions)
{
  EXPECT_EQ (LUMEX_COMPILER_HAS_EXCEPTIONS, 1);
}

TEST (LumexKeywordsExceptionsTest,
      GivenNoDefinition_WhenRead_ThenTheSwitchFollowsTheCompiler)
{
  EXPECT_EQ (LUMEX_HAS_EXCEPTIONS, LUMEX_COMPILER_HAS_EXCEPTIONS);
}

TEST (LumexKeywordsExceptionsTest,
      GivenNoDefinition_WhenRead_ThenEveryModuleFollowsTheSwitch)
{
  EXPECT_EQ (LUMEX_EXPECTED_HAS_EXCEPTIONS, LUMEX_HAS_EXCEPTIONS);
  EXPECT_EQ (LUMEX_OPTIONAL_HAS_EXCEPTIONS, LUMEX_HAS_EXCEPTIONS);
  EXPECT_EQ (LUMEX_SPAN_HAS_EXCEPTIONS, LUMEX_HAS_EXCEPTIONS);
  EXPECT_EQ (LUMEX_STRING_VIEW_HAS_EXCEPTIONS, LUMEX_HAS_EXCEPTIONS);
}

TEST (LumexKeywordsExceptionsTest,
      GivenNoDefinition_WhenRead_ThenSmartPtrAndExpectedKeepTheCompilersFact)
{
  EXPECT_EQ (LUMEX_SMART_PTR_HAS_EXCEPTIONS, LUMEX_COMPILER_HAS_EXCEPTIONS);
  EXPECT_EQ (LUMEX_EXPECTED_COMPILER_HAS_EXCEPTIONS,
             LUMEX_COMPILER_HAS_EXCEPTIONS);
}
