// text::to_case_insensitive (lumex/core/string/text) from C++11. Every suite
// of this directory compiles this file. text::join and quote* are covered by
// LumexJoin.cxx*.tests.cpp and LumexQuote.cxx*.tests.cpp in every standard.
// The tests use the LumexStringifyTest fixture of the stringify tests, so
// their names stay LumexStringifyTest.ToCaseInsensitive_*.

#include <string>

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexTextCase.hpp"

#include "lumex/tests/core/string/LumexStringTestFixtures.hpp"

TEST_F (LumexStringifyTest,
        ToCaseInsensitive_InPlace_LowercasesAndRemovesSpacesByDefault)
{
  std::string str = "Hello WORLD Test";
  lumex::core::string::text::to_case_insensitive (str);
  EXPECT_EQ (str, "helloworldtest");
}

TEST_F (LumexStringifyTest, ToCaseInsensitive_InPlace_KeepsSpacesWhenRequested)
{
  std::string str = "Hello WORLD";
  lumex::core::string::text::to_case_insensitive (str, false);
  EXPECT_EQ (str, "hello world");
}

TEST_F (LumexStringifyTest, ToCaseInsensitive_InPlace_EmptyString_StaysEmpty)
{
  std::string str;
  lumex::core::string::text::to_case_insensitive (str);
  EXPECT_TRUE (str.empty ());
}

TEST_F (LumexStringifyTest,
        ToCaseInsensitive_InPlace_AlreadyLowercase_Unchanged)
{
  std::string str = "already";
  lumex::core::string::text::to_case_insensitive (str, false);
  EXPECT_EQ (str, "already");
}

TEST_F (LumexStringifyTest, ToCaseInsensitive_CopyOut_DoesNotModifyOriginal)
{
  std::string const orig = "Hello WORLD";
  std::string out;
  lumex::core::string::text::to_case_insensitive (orig, out);
  EXPECT_EQ (orig, "Hello WORLD");
  EXPECT_EQ (out, "helloworld");
}

TEST_F (LumexStringifyTest,
        ToCaseInsensitive_CopyOut_RespectsNeedToRemoveSpacesFalse)
{
  std::string const orig = "Hello WORLD";
  std::string out;
  lumex::core::string::text::to_case_insensitive (orig, out, false);
  EXPECT_EQ (out, "hello world");
}

TEST_F (LumexStringifyTest, ToCaseInsensitive_CopyOut_DefaultRemovesSpaces)
{
  std::string const orig = "A B C";
  std::string out;
  lumex::core::string::text::to_case_insensitive (orig, out);
  EXPECT_EQ (out, "abc");
}

TEST_F (LumexStringifyTest, ToCaseInsensitive_HandlesTabsAndNewlinesAsSpaces)
{
  std::string str = "A\tB\nC";
  lumex::core::string::text::to_case_insensitive (str);
  EXPECT_EQ (str, "abc");
}
