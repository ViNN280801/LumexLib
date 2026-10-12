// LumexSpanStdIterators.cxx17.tests.cpp
//
// The iterators of std::string_view (a pointer in libstdc++, a wrapper in
// libc++) are contiguous and make a span without a specialization.
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::is_contiguous_iterator;
using lumex::core::span::view::span;

TEST (LumexSpanStdIteratorsCxx17Test,
      GivenStringView_WhenIterator_ThenContiguous)
{
  EXPECT_TRUE (is_contiguous_iterator<std::string_view::iterator>::value);
  EXPECT_TRUE (
      is_contiguous_iterator<std::string_view::const_iterator>::value);
  EXPECT_TRUE (is_contiguous_iterator<std::wstring_view::iterator>::value);
  std::string_view const text = "hello";
  span<char const> const view (text.begin (), text.end ());
  EXPECT_EQ (view.size (), 5u);
  EXPECT_EQ (view.data (), text.data ());
  span<char const> const counted (text.begin () + 1, 3);
  EXPECT_EQ (counted.size (), 3u);
  EXPECT_EQ (counted[0], 'e');
}
