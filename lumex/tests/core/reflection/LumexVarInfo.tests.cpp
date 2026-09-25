// LumexVarInfo.tests.cpp
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::reflection::var_info;

namespace
{
struct NoStreamable
{
  int value = 42;
};

struct Streamable
{
  int value;
};

std::ostream &
operator<< (std::ostream &os, Streamable const &s)
{
  return os << "Streamable(" << s.value << ")";
}

// LUMEX_VARINFO/VarInfo captures its argument by forwarding reference
// (T&&), which cannot bind to a void expression - these two return int
// rather than void so they can be used as a VarInfo argument at all, the
// same constraint the ported original design has.
int
NoexceptFunction () noexcept
{
  return 1;
}

int
ThrowingFunction ()
{
  return 2;
}
} // namespace

// --- stream detection (traits::stream) / FormatValue -----------------------

TEST (LumexVarInfoTest, GivenStreamableType_WhenIsOstreamable_ThenValueIsTrue)
{
  EXPECT_TRUE (lumex::core::utility::traits::stream::is_ostreamable<
               int const &>::value);
  EXPECT_TRUE (lumex::core::utility::traits::stream::is_ostreamable<
               std::string const &>::value);
  EXPECT_TRUE (lumex::core::utility::traits::stream::is_ostreamable<
               Streamable const &>::value);
}

TEST (LumexVarInfoTest,
      GivenNonStreamableType_WhenIsOstreamable_ThenValueIsFalse)
{
  EXPECT_FALSE (lumex::core::utility::traits::stream::is_ostreamable<
                NoStreamable const &>::value);
}

TEST (LumexVarInfoTest,
      GivenStreamableValue_WhenFormatValue_ThenReturnsStreamedRepresentation)
{
  EXPECT_EQ (VarInfoDetail::FormatValue (42), "42");
  EXPECT_EQ (VarInfoDetail::FormatValue (Streamable{ 7 }), "Streamable(7)");
}

TEST (
    LumexVarInfoTest,
    GivenNonStreamableValue_WhenFormatValue_ThenReturnsPlaceholderWithoutFailingToBuild)
{
  // This is the key philosophy difference from LumexStringify.hpp's
  // stringify(): a missing operator<< degrades gracefully here instead of
  // triggering a static_assert.
  EXPECT_EQ (VarInfoDetail::FormatValue (NoStreamable{}), "<no operator<<>");
}

// --- LUMEX_VARINFO / VarInfo ---------------------------------------------

TEST (LumexVarInfoTest,
      GivenNamedVariable_WhenVarInfo_ThenMessageContainsExprValueAndLocation)
{
  int x = 5;
  std::string info = LUMEX_VARINFO (x);

  EXPECT_NE (info.find ("x = 5"), std::string::npos) << info;
  EXPECT_NE (info.find ("size="), std::string::npos) << info;
  EXPECT_NE (info.find ("addr="), std::string::npos) << info;
  EXPECT_NE (info.find (__FILE__), std::string::npos) << info;
}

TEST (LumexVarInfoTest,
      GivenNoexceptExpression_WhenVarInfo_ThenReportsNoexceptTrue)
{
  std::string info = LUMEX_VARINFO (NoexceptFunction ());
  EXPECT_NE (info.find ("noexcept=true"), std::string::npos) << info;
}

TEST (LumexVarInfoTest,
      GivenPotentiallyThrowingExpression_WhenVarInfo_ThenReportsNoexceptFalse)
{
  std::string info = LUMEX_VARINFO (ThrowingFunction ());
  EXPECT_NE (info.find ("noexcept=false"), std::string::npos) << info;
}

TEST (
    LumexVarInfoTest,
    GivenTemporaryWithTopLevelCommaInTemplateArgs_WhenVarInfo_ThenCompilesWithoutExtraParens)
{
  // Regression check for the exact preprocessor trap documented in this
  // header: a single-parameter macro would see the comma inside
  // std::pair<int,int> as an argument separator. LUMEX_VARINFO is variadic
  // specifically to avoid requiring callers to add extra parentheses here.
  std::string info = LUMEX_VARINFO (std::pair<int, int> (1, 2));
  // The preprocessor's stringize operator preserves the call site's own
  // spacing, so the captured expression text has the same "int, int"/"1, 2"
  // spacing written below, not a compacted form.
  EXPECT_NE (info.find ("std::pair<int, int> (1, 2) ="), std::string::npos)
      << "unexpected expr text, got: " << info;
}

TEST (LumexVarInfoTest,
      GivenConstVariable_WhenVarInfo_ThenReportsConstQualifier)
{
  int const y = 10;
  std::string info = LUMEX_VARINFO (y);
  EXPECT_NE (info.find ("const"), std::string::npos) << info;
}

TEST (
    LumexVarInfoTest,
    GivenNonStreamableExpression_WhenVarInfo_ThenReportsPlaceholderInsteadOfFailingToBuild)
{
  NoStreamable value;
  std::string info = LUMEX_VARINFO (value);
  EXPECT_NE (info.find ("<no operator<<>"), std::string::npos) << info;
}

TEST (LumexVarInfoTest,
      GivenArrayExpression_WhenVarInfo_ThenSizeIsTheArraySizeNotAPointerSize)
{
  int arr[5] = { 1, 2, 3, 4, 5 };
  std::string info = LUMEX_VARINFO (arr);
  EXPECT_NE (info.find ("size=" + std::to_string (sizeof (arr))),
             std::string::npos)
      << info;
}

TEST (LumexVarInfoTest,
      GivenVolatileVariable_WhenVarInfo_ThenReportsVolatileQualifier)
{
  int volatile z = 3;
  std::string info = LUMEX_VARINFO (z);
  EXPECT_NE (info.find ("volatile"), std::string::npos) << info;
}

TEST (LumexVarInfoTest,
      GivenRvalueTemporary_WhenVarInfo_ThenReportsValueWithoutExtraParens)
{
  std::string info = LUMEX_VARINFO (1 + 1);
  EXPECT_NE (info.find ("1 + 1 = 2"), std::string::npos) << info;
}

TEST (LumexVarInfoTest,
      GivenStringLiteral_WhenVarInfo_ThenSizeIsTheArraySizeIncludingNull)
{
  std::string info = LUMEX_VARINFO ("hi");
  EXPECT_NE (info.find ("size=" + std::to_string (sizeof ("hi"))),
             std::string::npos)
      << info;
}
