// LumexJoin.cxx11.tests.cpp
// text::join from C++11. Every suite of the module compiles this file; the
// C++17 and C++20 cases are in LumexJoin.cxx17.tests.cpp and
// LumexJoin.cxx20.tests.cpp.
#include <array>
#include <cstddef>
#include <deque>
#include <list>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexJoin.hpp"
#include "lumex/core/string_view/view/LumexStringView.hpp"

using lumex::core::string::text::join;

namespace
{
struct point_t
{
  int x;
  int y;
};

std::ostream &
operator<< (std::ostream &os, point_t const &point)
{
  return os << '(' << point.x << ';' << point.y << ')';
}
} // namespace

TEST (LumexJoinTest, GivenEmptyVector_WhenJoin_ThenEmptyString)
{
  std::vector<std::string> const empty;
  EXPECT_EQ (join (empty, ", "), "");
}

TEST (LumexJoinTest, GivenSingleElement_WhenJoin_ThenNoSeparator)
{
  std::vector<std::string> const single (1, "COM1");
  EXPECT_EQ (join (single, ", "), "COM1");
}

TEST (LumexJoinTest, GivenSeveralElements_WhenJoin_ThenNoTrailingSeparator)
{
  std::vector<std::string> const ports = { "COM1", "COM2", "COM3", "COM4" };
  EXPECT_EQ (join (ports, ", "), "COM1, COM2, COM3, COM4");
}

TEST (LumexJoinTest, GivenIntegers_WhenJoin_ThenStreamedText)
{
  std::vector<int> const numbers = { 1, -2, 3 };
  EXPECT_EQ (join (numbers, "-"), "1--2-3");
}

TEST (LumexJoinTest, GivenDoubles_WhenJoin_ThenDefaultStreamFormatting)
{
  std::vector<double> const values = { 0.5, 1.25, 3.0 };
  EXPECT_EQ (join (values, "|"), "0.5|1.25|3");
}

TEST (LumexJoinTest, GivenCustomStreamable_WhenJoin_ThenUsesItsOperator)
{
  std::vector<point_t> points;
  point_t const a = { 1, 2 };
  point_t const b = { 3, 4 };
  points.push_back (a);
  points.push_back (b);
  EXPECT_EQ (join (points, " "), "(1;2) (3;4)");
}

TEST (LumexJoinTest, GivenCharacters_WhenJoin_ThenCharactersNotCodes)
{
  std::vector<char> const letters = { 'a', 'b', 'c' };
  EXPECT_EQ (join (letters, ","), "a,b,c");
}

TEST (LumexJoinTest, GivenEmptySeparator_WhenJoin_ThenConcatenates)
{
  std::vector<std::string> const parts = { "ab", "cd", "ef" };
  EXPECT_EQ (join (parts, ""), "abcdef");
}

TEST (LumexJoinTest, GivenStdStringSeparator_WhenJoin_ThenUsed)
{
  std::vector<int> const numbers = { 7, 8 };
  std::string const separator (" -> ");
  EXPECT_EQ (join (numbers, separator), "7 -> 8");
}

TEST (LumexJoinTest, GivenElementsContainingSeparator_WhenJoin_ThenVerbatim)
{
  std::vector<std::string> const parts = { "a,b", "c" };
  EXPECT_EQ (join (parts, ","), "a,b,c");
}

TEST (LumexJoinTest, GivenEmptyStringElements_WhenJoin_ThenSeparatorsKept)
{
  std::vector<std::string> const parts = { "", "", "" };
  EXPECT_EQ (join (parts, ";"), ";;");
}

TEST (LumexJoinTest, GivenSet_WhenJoin_ThenIterationOrder)
{
  std::set<std::string> const ports = { "COM2", "COM1" };
  EXPECT_EQ (join (ports, ","), "COM1,COM2");
}

TEST (LumexJoinTest, GivenListAndDeque_WhenJoin_ThenSameAsVector)
{
  std::list<int> const as_list = { 1, 2, 3 };
  std::deque<int> const as_deque = { 1, 2, 3 };
  EXPECT_EQ (join (as_list, ","), "1,2,3");
  EXPECT_EQ (join (as_deque, ","), "1,2,3");
}

TEST (LumexJoinTest, GivenStdArray_WhenJoin_ThenJoined)
{
  std::array<int, 3> const values = { { 4, 5, 6 } };
  EXPECT_EQ (join (values, " "), "4 5 6");
}

TEST (LumexJoinTest, GivenCArray_WhenJoin_ThenJoined)
{
  int const values[] = { 9, 8, 7 };
  EXPECT_EQ (join (values, "/"), "9/8/7");
}

TEST (LumexJoinTest, GivenCharPointers_WhenJoin_ThenTextNotAddresses)
{
  std::vector<char const *> const words = { "alpha", "beta" };
  EXPECT_EQ (join (words, " "), "alpha beta");
}

TEST (LumexJoinTest, GivenUtf8Text_WhenJoin_ThenBytesPreserved)
{
  std::vector<std::string> const words = { "\xD0\x9F\xD1\x80\xD0\xB8", "ok" };
  EXPECT_EQ (join (words, "-"), "\xD0\x9F\xD1\x80\xD0\xB8-ok");
}

TEST (LumexJoinTest, Stress_GivenManyElements_WhenJoin_ThenLengthAndEdges)
{
  std::vector<int> values;
  for (int i = 0; i < 10000; ++i)
    values.push_back (i % 10);
  std::string const text = join (values, ",");
  EXPECT_EQ (text.size (), static_cast<std::size_t> (10000 * 2 - 1));
  EXPECT_EQ (text.substr (0, 5), "0,1,2");
  EXPECT_EQ (text.substr (text.size () - 3), "8,9");
}

// Without std::ranges (below C++20, and with a standard library that has no
// <ranges> at C++20, such as libstdc++ 8) join takes any streamable
// separator. LumexJoin.cxx20.tests.cpp covers the std::ranges path; both
// follow the library's own switch, LUMEX_HAS_STD_RANGES.
#if !LUMEX_HAS_STD_RANGES

TEST (LumexJoinTest, GivenCharSeparator_WhenJoinPreCxx20_ThenUsed)
{
  std::vector<int> const numbers = { 1, 2, 3 };
  EXPECT_EQ (join (numbers, ';'), "1;2;3");
}

TEST (LumexJoinTest, GivenLumexStringViewSeparator_WhenJoinPreCxx20_ThenUsed)
{
  std::vector<int> const numbers = { 1, 2 };
  lumex::core::string_view::view::lumex_string_view const separator (" | ", 3);
  EXPECT_EQ (join (numbers, separator), "1 | 2");
}

#endif
