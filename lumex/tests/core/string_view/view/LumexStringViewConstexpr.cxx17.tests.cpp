#include <cstddef>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

// C++17: the reverse iterators are constexpr (std::reverse_iterator is), and
// so are the conversions to and from std::string_view and the comparisons
// with it.

namespace
{
constexpr lumex_string_view hello = "hello";
constexpr std::string_view standard = "hello";
constexpr lumex_string_view from_standard = standard;
constexpr std::string_view to_standard = hello;
constexpr lumex_wstring_view wide = L"wide";
constexpr std::wstring_view wide_standard = wide;
} // namespace

static_assert (*hello.rbegin () == 'o' && *hello.crbegin () == 'o', "rbegin");
static_assert (*(hello.rend () - 1) == 'h' && *(hello.crend () - 1) == 'h',
               "rend");
static_assert (hello.rend () - hello.rbegin () == 5, "the reverse range");
static_assert (from_standard == hello && from_standard.size () == 5,
               "from std::string_view");
static_assert (to_standard == standard && to_standard.size () == 5,
               "to std::string_view");
static_assert (hello == standard && standard == hello, "== with the standard");
static_assert (hello != std::string_view ("hell"), "!= with the standard");
static_assert (hello < std::string_view ("help")
                   && std::string_view ("hel") < hello,
               "< with the standard");
static_assert (hello > std::string_view ("hel")
                   && std::string_view ("help") > hello,
               "> with the standard");
static_assert (hello <= standard && standard >= hello, "<= and >=");
static_assert (wide_standard == L"wide" && wide == wide_standard,
               "the wide views");

TEST (LumexStringViewConstexprCxx17Test,
      GivenReverseIterators_WhenWalkAtRunTime_ThenBackwards)
{
  lumex_string_view const view = hello;
  std::string backwards;
  for (lumex_string_view::const_reverse_iterator it = view.rbegin ();
       it != view.rend (); ++it)
    backwards.push_back (*it);
  EXPECT_EQ (backwards, "olleh");
}
