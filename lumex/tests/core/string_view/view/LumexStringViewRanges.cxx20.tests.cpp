#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <deque>
#include <iterator>
#include <list>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

// C++20: the views are views and borrowed ranges, and the iterator-pair
// constructor takes contiguous iterators and nothing else.

using lumex::core::string_view::view::lumex_u16string_view;

static_assert (std::ranges::view<lumex_string_view>, "a view is a view");
static_assert (std::ranges::view<lumex_wstring_view>, "");
static_assert (std::ranges::borrowed_range<lumex_string_view>,
               "and a borrowed range");
static_assert (std::ranges::borrowed_range<lumex_string_view &&>, "");
static_assert (std::ranges::borrowed_range<lumex_wstring_view>, "");
static_assert (std::ranges::contiguous_range<lumex_string_view>, "");
static_assert (std::ranges::sized_range<lumex_string_view>, "");
static_assert (std::ranges::random_access_range<lumex_wstring_view>, "");
static_assert (std::ranges::enable_view<lumex_string_view>, "");
static_assert (std::ranges::enable_borrowed_range<lumex_string_view>, "");
static_assert (std::ranges::enable_view<lumex_u16string_view>, "");
static_assert (
    std::is_same_v<std::ranges::range_value_t<lumex_string_view>, char>, "");
static_assert (std::ranges::range_value_t<lumex_wstring_view>{} == L'\0', "");

// the iterator-pair constructor
static_assert (
    std::is_constructible_v<lumex_string_view, char const *, char const *>,
    "");
static_assert (
    std::is_constructible_v<lumex_string_view, std::string::const_iterator,
                            std::string::const_iterator>,
    "string iterators are contiguous");
static_assert (
    std::is_constructible_v<lumex_string_view, std::vector<char>::iterator,
                            std::vector<char>::iterator>,
    "vector iterators are contiguous");
static_assert (std::is_constructible_v<lumex_string_view,
                                       std::array<char, 3>::const_iterator,
                                       std::array<char, 3>::const_iterator>,
               "");
static_assert (std::is_constructible_v<lumex_string_view,
                                       std::string_view::const_iterator,
                                       std::string_view::const_iterator>,
               "");
static_assert (
    !std::is_constructible_v<lumex_string_view, std::deque<char>::iterator,
                             std::deque<char>::iterator>,
    "deque iterators are not contiguous");
static_assert (
    !std::is_constructible_v<lumex_string_view, std::list<char>::iterator,
                             std::list<char>::iterator>,
    "list iterators are not contiguous");
static_assert (
    !std::is_constructible_v<lumex_string_view, std::vector<wchar_t>::iterator,
                             std::vector<wchar_t>::iterator>,
    "the value type must be the character type");

// the range constructor from a standard span
static_assert (
    std::is_constructible_v<lumex_string_view, std::span<char const>>, "");
static_assert (std::is_constructible_v<lumex_string_view, std::span<char>>,
               "");

TEST (LumexStringViewRangesTest,
      GivenContiguousIterators_WhenConstruct_ThenViewOfThem)
{
  std::string const text = "hello world";
  lumex_string_view const from_string (text.begin () + 6, text.end ());
  EXPECT_EQ (from_string, "world");
  std::vector<char> vec{ 'a', 'b', 'c', 'd' };
  lumex_string_view const from_vector (vec.begin (), vec.begin () + 3);
  EXPECT_EQ (from_vector, "abc");
  EXPECT_EQ (from_vector.data (), vec.data ());
  std::string_view const standard = "standard";
  lumex_string_view const from_view (standard.begin () + 3, standard.end ());
  EXPECT_EQ (from_view, "ndard");
  lumex_string_view const empty_range (text.begin (), text.begin ());
  EXPECT_TRUE (empty_range.empty ());
  std::array<char, 3> const arr{ { 'x', 'y', 'z' } };
  EXPECT_EQ (lumex_string_view (arr.begin (), arr.end ()), "xyz");
  std::wstring const wide = L"wide text";
  EXPECT_EQ (lumex_wstring_view (wide.begin () + 5, wide.end ()), L"text");
}

TEST (LumexStringViewRangesTest, GivenSpan_WhenConstruct_ThenViewOfTheSpan)
{
  char const buffer[] = { 'a', 'b', 'c' };
  std::span<char const> const span (buffer);
  lumex_string_view const view (span);
  EXPECT_EQ (view.size (), 3U);
  EXPECT_EQ (view.data (), buffer);
}

TEST (LumexStringViewRangesTest,
      GivenBorrowedRange_WhenAlgorithmReturnsIterator_ThenNotDangling)
{
  auto const it = std::ranges::find (lumex_string_view ("hello"), 'l');
  static_assert (std::is_same_v<std::remove_cv_t<decltype (it)>, char const *>,
                 "a pointer, not std::ranges::dangling");
  EXPECT_EQ (*it, 'l');
  auto const sub
      = std::ranges::subrange (lumex_string_view ("hello").begin () + 1,
                               lumex_string_view ("hello").begin () + 3);
  EXPECT_EQ (sub.size (), 2U);
  std::string collected;
  for (char const chr : lumex_string_view ("abc") | std::views::reverse)
    collected.push_back (chr);
  EXPECT_EQ (collected, "cba");
  std::string taken;
  for (char const chr : lumex_string_view ("abcdef") | std::views::take (3))
    taken.push_back (chr);
  EXPECT_EQ (taken, "abc");
}
