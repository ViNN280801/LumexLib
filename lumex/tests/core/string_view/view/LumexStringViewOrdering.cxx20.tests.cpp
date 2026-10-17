#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <ios>
#include <string>
#include <string_view>
#include <vector>
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#if __has_include(<concepts>)
#include <concepts>
#endif
#endif

#include <gtest/gtest.h>

#include "LumexStringViewTestSupport.hpp"
#include "lumex/core/string_view/LumexStringView"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

// Needs <compare> and <concepts> (not in GCC 8 -std=c++2a)
#if LUMEX_HAS_STD_THREE_WAY_COMPARISON && LUMEX_HAS_STD_CONCEPTS

// operator<=> (C++20): strong_ordering, as the standard view's, consistent
// with the six operators, with the embedded NUL and the unsigned order.

static_assert (
    std::is_same_v<decltype (std::declval<lumex_string_view> ()
                             <=> std::declval<lumex_string_view> ()),
                   std::strong_ordering>,
    "the narrow view orders strongly");
static_assert (
    std::is_same_v<decltype (std::declval<lumex_wstring_view> ()
                             <=> std::declval<lumex_wstring_view> ()),
                   std::strong_ordering>,
    "the wide view orders strongly");
static_assert (std::is_same_v<decltype (std::declval<std::string_view> ()
                                        <=> std::declval<std::string_view> ()),
                              std::strong_ordering>,
               "the standard view orders strongly");
static_assert (std::three_way_comparable<lumex_string_view>, "");
static_assert (std::three_way_comparable<lumex_wstring_view>, "");
static_assert (std::totally_ordered<lumex_string_view>, "");
static_assert (std::equality_comparable<lumex_wstring_view>, "");
static_assert (std::is_same_v<decltype (std::declval<lumex_string_view> ()
                                        <=> std::declval<std::string_view> ()),
                              std::strong_ordering>,
               "mixed <=>");
static_assert ((lumex_string_view ("a") <=> lumex_string_view ("b")) < 0, "");
static_assert ((lumex_string_view ("b") <=> lumex_string_view ("a")) > 0, "");
static_assert ((lumex_string_view ("a") <=> lumex_string_view ("a")) == 0, "");
static_assert ((lumex_string_view ("ab") <=> lumex_string_view ("abc")) < 0,
               "");
static_assert ((lumex_string_view ("\x80") <=> lumex_string_view ("a")) > 0,
               "unsigned order");
static_assert ((lumex_string_view ("a\0b", 3)
                <=> lumex_string_view ("a\0c", 3))
                   < 0,
               "embedded NUL");
static_assert ((lumex_string_view () <=> lumex_string_view ("")) == 0, "");
static_assert ((lumex_string_view ("a") <=> std::string_view ("b")) < 0
                   && (std::string_view ("b") <=> lumex_string_view ("a")) > 0,
               "mixed in both orders");

TEST (LumexStringViewOrderingCxx20Test,
      GivenEveryPair_WhenThreeWay_ThenSameAsStdStringView)
{
  std::vector<std::string> const texts = lumex_sv_test::all_texts (
      lumex_sv_test::char_set<char>::alphabet (), 4);
  int mismatches = 0;
  for (std::size_t i = 0; i < texts.size (); ++i)
    for (std::size_t j = 0; j < texts.size (); ++j)
      {
        lumex_string_view const left (texts[i]);
        lumex_string_view const right (texts[j]);
        std::string_view const std_left (texts[i]);
        std::string_view const std_right (texts[j]);
        std::strong_ordering const expected = std_left <=> std_right;
        bool const ok = (left <=> right) == expected
                        && (left <=> std_right) == expected
                        && (std_left <=> right) == expected
                        && ((left <=> right) < 0) == (left < right)
                        && ((left <=> right) > 0) == (left > right)
                        && ((left <=> right) <= 0) == (left <= right)
                        && ((left <=> right) >= 0) == (left >= right)
                        && ((left <=> right) == 0) == (left == right);
        if (!ok && ++mismatches < 10)
          ADD_FAILURE () << "pair " << i << ", " << j;
      }
  EXPECT_EQ (mismatches, 0);
}

TEST (LumexStringViewOrderingCxx20Test,
      GivenEveryWidePair_WhenThreeWay_ThenSameAsStdWstringView)
{
  std::vector<std::wstring> const texts = lumex_sv_test::all_texts (
      lumex_sv_test::char_set<wchar_t>::alphabet (), 3);
  int mismatches = 0;
  for (std::size_t i = 0; i < texts.size (); ++i)
    for (std::size_t j = 0; j < texts.size (); ++j)
      {
        std::strong_ordering const expected
            = std::wstring_view (texts[i]) <=> std::wstring_view (texts[j]);
        if (((lumex_wstring_view (texts[i]) <=> lumex_wstring_view (texts[j]))
             != expected)
            && ++mismatches < 10)
          ADD_FAILURE () << "pair " << i << ", " << j;
      }
  EXPECT_EQ (mismatches, 0);
}

TEST (LumexStringViewOrderingCxx20Test,
      GivenViews_WhenSorted_ThenOrderedByThreeWay)
{
  std::vector<lumex_string_view> views{ "pear", "apple", "fig", "apple pie",
                                        "" };
  std::vector<std::string_view> standard{ "pear", "apple", "fig", "apple pie",
                                          "" };
  std::sort (views.begin (), views.end ());
  std::sort (standard.begin (), standard.end ());
  ASSERT_EQ (views.size (), standard.size ());
  for (std::size_t i = 0; i < views.size (); ++i)
    EXPECT_TRUE (views[i] == standard[i]) << i;
}

namespace
{
using lumex_sv_test::caseless_traits;
using lumex_sv_test::caseless_view;

} // namespace

// [string.view.comparison]: without traits::comparison_category the result is
// std::weak_ordering; std::char_traits names std::strong_ordering.
static_assert (std::is_same_v<decltype (std::declval<caseless_view> ()
                                        <=> std::declval<caseless_view> ()),
                              std::weak_ordering>,
               "traits without a category give weak_ordering");

TEST (LumexStringViewOrderingCxx20Test,
      GivenCaselessTraits_WhenThreeWay_ThenWeakOrdering)
{
  caseless_view const upper ("ABC");
  caseless_view const lower ("abc");
  EXPECT_TRUE ((upper <=> lower) == std::weak_ordering::equivalent);
  EXPECT_TRUE (upper == lower);
  EXPECT_TRUE ((caseless_view ("abd") <=> upper)
               == std::weak_ordering::greater);
  EXPECT_TRUE ((caseless_view ("AB") <=> lower) == std::weak_ordering::less);
}

#endif // LUMEX_HAS_STD_THREE_WAY_COMPARISON && LUMEX_HAS_STD_CONCEPTS
