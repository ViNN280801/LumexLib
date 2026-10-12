// LumexAttributes.cxx11.tests.cpp
// The likely / unlikely macros of LumexAttributes.hpp in every standard.
// Every utility suite compiles this file: below C++20 the statement
// attributes are empty and the conditions go through __builtin_expect on GCC
// and Clang (plain elsewhere); from C++20 they are [[likely]] / [[unlikely]]
// and the conditions are plain. The CTest suffix .cxx<std> tells the
// standards apart.
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"

TEST (LumexAttributesTest,
      GivenATrueCondition_WhenLikelyCondWrapsIt_ThenTheBranchIsTaken)
{
  // Arrange
  int const value = 1;
  bool taken = false;

  // Act
  if (LUMEX_ATTRIBUTE_LIKELY_COND (value == 1))
    taken = true;

  // Assert
  EXPECT_TRUE (taken);
}

TEST (LumexAttributesTest,
      GivenAFalseCondition_WhenLikelyCondWrapsIt_ThenTheBranchIsSkipped)
{
  // Arrange
  int const value = 0;
  bool taken = false;

  // Act
  if (LUMEX_ATTRIBUTE_LIKELY_COND (value == 1))
    taken = true;

  // Assert
  EXPECT_FALSE (taken);
}

TEST (LumexAttributesTest,
      GivenATrueCondition_WhenUnlikelyCondWrapsIt_ThenTheBranchIsTaken)
{
  // Arrange
  int const value = 1;
  bool taken = false;

  // Act
  if (LUMEX_ATTRIBUTE_UNLIKELY_COND (value == 1))
    taken = true;

  // Assert
  EXPECT_TRUE (taken);
}

TEST (LumexAttributesTest,
      GivenAFalseCondition_WhenUnlikelyCondWrapsIt_ThenTheBranchIsSkipped)
{
  // Arrange
  int const value = 0;
  bool taken = false;

  // Act
  if (LUMEX_ATTRIBUTE_UNLIKELY_COND (value == 1))
    taken = true;

  // Assert
  EXPECT_FALSE (taken);
}

TEST (LumexAttributesTest,
      GivenAConditionWithASideEffect_WhenWrapped_ThenItIsEvaluatedOnce)
{
  // Arrange
  int likely_calls = 0;
  int unlikely_calls = 0;

  // Act
  if (LUMEX_ATTRIBUTE_LIKELY_COND (++likely_calls > 0))
    {
    }
  if (LUMEX_ATTRIBUTE_UNLIKELY_COND (++unlikely_calls > 0))
    {
    }

  // Assert
  EXPECT_EQ (1, likely_calls);
  EXPECT_EQ (1, unlikely_calls);
}

TEST (LumexAttributesTest,
      GivenAPointerCondition_WhenWrapped_ThenNullIsFalseAndNonNullIsTrue)
{
  // Arrange
  int target = 0;
  int const *const non_null = &target;
  int const *const null = nullptr;
  bool non_null_taken = false;
  bool null_taken = false;

  // Act
  if (LUMEX_ATTRIBUTE_LIKELY_COND (non_null))
    non_null_taken = true;
  if (LUMEX_ATTRIBUTE_UNLIKELY_COND (null))
    null_taken = true;

  // Assert
  EXPECT_TRUE (non_null_taken);
  EXPECT_FALSE (null_taken);
}

TEST (LumexAttributesTest,
      GivenLowPrecedenceOperators_WhenWrapped_ThenTheWholeConditionCounts)
{
  // Arrange
  bool const no = false;
  bool const yes = true;
  bool or_taken = false;
  bool ternary_taken = false;

  // Act
  if (LUMEX_ATTRIBUTE_UNLIKELY_COND (no || yes))
    or_taken = true;
  if (LUMEX_ATTRIBUTE_LIKELY_COND (no ? yes : no))
    ternary_taken = true;

  // Assert
  EXPECT_TRUE (or_taken);
  EXPECT_FALSE (ternary_taken);
}

TEST (LumexAttributesTest,
      GivenALoopCondition_WhenWrapped_ThenTheLoopRunsUntilItIsFalse)
{
  // Arrange
  int iterations = 0;

  // Act
  while (LUMEX_ATTRIBUTE_LIKELY_COND (iterations < 3))
    ++iterations;

  // Assert
  EXPECT_EQ (3, iterations);
}

TEST (
    LumexAttributesTest,
    GivenThisCompilerAndStandard_WhenCondMacrosExpand_ThenTheyTakeTheirBranch)
{
  // The text each macro call expands to.
  std::string const likely = LUMEX_STRINGIZE (LUMEX_ATTRIBUTE_LIKELY_COND (x));
  std::string const unlikely
      = LUMEX_STRINGIZE (LUMEX_ATTRIBUTE_UNLIKELY_COND (x));

#if __cplusplus < 202002L && (defined(__GNUC__) || defined(__clang__))
  EXPECT_NE (std::string::npos, likely.find ("__builtin_expect")) << likely;
  EXPECT_NE (std::string::npos, likely.find (", 1)")) << likely;
  EXPECT_NE (std::string::npos, unlikely.find ("__builtin_expect"))
      << unlikely;
  EXPECT_NE (std::string::npos, unlikely.find (", 0)")) << unlikely;
#else
  EXPECT_EQ ("(x)", likely);
  EXPECT_EQ ("(x)", unlikely);
#endif
}

TEST (
    LumexAttributesTest,
    GivenStatementAttributes_WhenTheyPrecedeIfAndElseBlocks_ThenEachBlockRuns)
{
  // Arrange
  int taken = 0;
  int skipped = 0;

  // Act
  for (int value = 0; value < 2; ++value)
    {
      if (value == 1)
        LUMEX_ATTRIBUTE_LIKELY { ++taken; }
      else
        LUMEX_ATTRIBUTE_UNLIKELY { ++skipped; }
    }

  // Assert
  EXPECT_EQ (1, taken);
  EXPECT_EQ (1, skipped);
}

TEST (LumexAttributesTest,
      GivenThisStandard_WhenStatementAttributesExpand_ThenTheyTakeTheirBranch)
{
#if __cplusplus >= 202002L
  EXPECT_STREQ ("[[likely]]", LUMEX_STRINGIZE (LUMEX_ATTRIBUTE_LIKELY));
  EXPECT_STREQ ("[[unlikely]]", LUMEX_STRINGIZE (LUMEX_ATTRIBUTE_UNLIKELY));
#else
  EXPECT_STREQ ("", LUMEX_STRINGIZE (LUMEX_ATTRIBUTE_LIKELY));
  EXPECT_STREQ ("", LUMEX_STRINGIZE (LUMEX_ATTRIBUTE_UNLIKELY));
#endif
}
