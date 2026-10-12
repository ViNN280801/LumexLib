// LumexStringViewStd.cxx17.tests.cpp
//
// From C++17 lumex_string_view / lumex_wstring_view convert implicitly to and
// from std::string_view / std::wstring_view, like span converts to and from
// std::span. The conversions are member templates (never exported), the
// comparisons with the standard view are constrained templates that win over
// the operators of the standard library, and nothing that worked before
// becomes ambiguous: a C string and a std::string still go to the view.
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

using lumex::core::string_view::view::lumex_string_view;
using lumex::core::string_view::view::lumex_wstring_view;

namespace
{
// Overloads that take the standard view, a C string and a std::string next to
// each other: a view of the library must pick the standard view (the only one
// it converts to implicitly), a literal the C string.
int
pick (std::string_view)
{
  return 1;
}

int
pick (char const *)
{
  return 2;
}

int
pick (std::string const &)
{
  return 3;
}

int
pick_wide (std::wstring_view)
{
  return 1;
}

int
pick_wide (wchar_t const *)
{
  return 2;
}

int
pick_wide (std::wstring const &)
{
  return 3;
}

std::size_t
std_size (std::string_view view)
{
  return view.size ();
}

std::size_t
lumex_size (lumex_string_view view)
{
  return view.size ();
}
} // namespace

static_assert (std::is_convertible<lumex_string_view, std::string_view>::value,
               "the view converts to std::string_view");
static_assert (std::is_convertible<std::string_view, lumex_string_view>::value,
               "std::string_view converts to the view");
static_assert (
    std::is_convertible<lumex_wstring_view, std::wstring_view>::value,
    "the wide view converts to std::wstring_view");
static_assert (
    std::is_convertible<std::wstring_view, lumex_wstring_view>::value,
    "std::wstring_view converts to the wide view");
static_assert (
    std::is_nothrow_constructible<std::string_view, lumex_string_view>::value,
    "the conversion does not throw");
static_assert (
    std::is_nothrow_constructible<lumex_string_view, std::string_view>::value,
    "the conversion does not throw");
static_assert (
    !std::is_convertible<lumex_string_view, std::wstring_view>::value,
    "the narrow view does not become a wide one");
static_assert (
    !std::is_convertible<std::wstring_view, lumex_string_view>::value,
    "the wide view does not become a narrow one");
static_assert (
    !std::is_convertible<lumex_wstring_view, std::string_view>::value,
    "the wide view does not become a narrow one");
static_assert (
    !std::is_convertible<std::string_view, lumex_wstring_view>::value,
    "the narrow view does not become a wide one");
// The conversion to std::string stays explicit.
static_assert (!std::is_convertible<lumex_string_view, std::string>::value,
               "a view does not become an owning string implicitly");
static_assert (std::is_constructible<std::string, lumex_string_view>::value,
               "a std::string is still built from a view explicitly");
// A C string and a std::string still convert to the view.
static_assert (std::is_convertible<char const *, lumex_string_view>::value,
               "a C string is still accepted");
static_assert (
    std::is_convertible<std::string const &, lumex_string_view>::value,
    "a std::string is still accepted");

TEST (LumexStringViewStdTest, GivenView_WhenConvertToStd_ThenSameCharacters)
{
  std::string const text = "prefix:body:suffix";
  lumex_string_view const view (text.data () + 7, 4);
  std::string_view const standard = view;
  EXPECT_EQ (standard.data (), text.data () + 7);
  EXPECT_EQ (standard.size (), 4U);
  EXPECT_EQ (standard, "body");
  std::string_view const direct (view);
  EXPECT_EQ (direct.data (), view.data ());
  EXPECT_EQ (static_cast<std::string_view> (view), standard);
}

TEST (LumexStringViewStdTest,
      GivenStdView_WhenConvertToView_ThenSameCharacters)
{
  std::string const text = "prefix:body:suffix";
  std::string_view const standard (text.data () + 7, 4);
  lumex_string_view const view = standard;
  EXPECT_EQ (view.data (), text.data () + 7);
  EXPECT_EQ (view.size (), 4U);
  EXPECT_EQ (view, lumex_string_view ("body"));
  lumex_string_view const direct (standard);
  EXPECT_EQ (direct.data (), standard.data ());
  EXPECT_EQ (lumex_size (standard), 4U);
  EXPECT_EQ (std_size (view), 4U);
}

TEST (LumexStringViewStdTest,
      GivenEmptyAndEmbeddedZeros_WhenRoundTrip_ThenKept)
{
  lumex_string_view const empty;
  std::string_view const standard_empty = empty;
  EXPECT_EQ (standard_empty.data (), nullptr);
  EXPECT_TRUE (standard_empty.empty ());
  lumex_string_view const back = standard_empty;
  EXPECT_EQ (back.data (), nullptr);
  EXPECT_TRUE (back.empty ());

  std::string const zeros ("a\0b\0", 4);
  lumex_string_view const view (zeros);
  std::string_view const standard = view;
  EXPECT_EQ (standard.size (), 4U);
  EXPECT_EQ (std::string (standard), zeros);
  lumex_string_view const again = standard;
  EXPECT_EQ (again.size (), 4U);
  EXPECT_EQ (again.data (), zeros.data ());
}

TEST (LumexStringViewStdTest,
      GivenViews_WhenConvertInConstantExpression_ThenWorks)
{
  constexpr lumex_string_view view ("abc", 3);
  constexpr std::string_view standard = view;
  static_assert (standard.size () == 3, "constexpr conversion");
  constexpr lumex_string_view back = standard;
  static_assert (back.size () == 3, "constexpr conversion back");
  EXPECT_EQ (standard.data (), view.data ());
  EXPECT_EQ (back.data (), view.data ());
}

TEST (LumexStringViewStdTest, GivenWideViews_WhenConvert_ThenSameCharacters)
{
  std::wstring const text = L"prefix:body:suffix";
  lumex_wstring_view const view (text.data () + 7, 4);
  std::wstring_view const standard = view;
  EXPECT_EQ (standard.data (), text.data () + 7);
  EXPECT_EQ (standard.size (), 4U);
  EXPECT_EQ (standard, L"body");
  lumex_wstring_view const back = standard;
  EXPECT_EQ (back.data (), view.data ());
  EXPECT_EQ (back.size (), 4U);
  lumex_wstring_view const empty;
  std::wstring_view const standard_empty = empty;
  EXPECT_EQ (standard_empty.data (), nullptr);
  EXPECT_EQ (standard_empty.size (), 0U);
}

TEST (LumexStringViewStdTest,
      GivenMixedViews_WhenCompare_ThenNoAmbiguityAndSameAsViews)
{
  lumex_string_view const abc ("abc");
  lumex_string_view const abd ("abd");
  std::string_view const std_abc ("abc");
  std::string_view const std_abd ("abd");
  // Equal.
  EXPECT_TRUE (abc == std_abc);
  EXPECT_TRUE (std_abc == abc);
  EXPECT_FALSE (abc != std_abc);
  EXPECT_FALSE (std_abc != abc);
  // Different.
  EXPECT_FALSE (abc == std_abd);
  EXPECT_FALSE (std_abd == abc);
  EXPECT_TRUE (abc != std_abd);
  EXPECT_TRUE (std_abd != abc);
  // Order, every operator and both orders of the operands.
  EXPECT_TRUE (abc < std_abd);
  EXPECT_TRUE (std_abc < abd);
  EXPECT_FALSE (abd < std_abc);
  EXPECT_TRUE (abd > std_abc);
  EXPECT_TRUE (std_abd > abc);
  EXPECT_TRUE (abc <= std_abc);
  EXPECT_TRUE (std_abc <= abc);
  EXPECT_TRUE (abc <= std_abd);
  EXPECT_FALSE (abd <= std_abc);
  EXPECT_TRUE (abc >= std_abc);
  EXPECT_TRUE (std_abc >= abc);
  EXPECT_TRUE (abd >= std_abc);
  EXPECT_FALSE (abc >= std_abd);
  // A prefix is less than the longer text, an empty view is less than all.
  EXPECT_TRUE (lumex_string_view ("ab") < std_abc);
  EXPECT_TRUE (std::string_view ("ab") < abc);
  EXPECT_TRUE (lumex_string_view () < std_abc);
  EXPECT_TRUE (lumex_string_view () == std::string_view ());
  EXPECT_TRUE (std::string_view () == lumex_string_view ());
  // The result is the one of the standard view.
  EXPECT_EQ ((abc < std_abd), (std_abc < std_abd));
  EXPECT_EQ ((std_abd < abc), (std_abd < std_abc));
}

TEST (LumexStringViewStdTest, GivenWideMixedViews_WhenCompare_ThenNoAmbiguity)
{
  lumex_wstring_view const abc (L"abc");
  std::wstring_view const std_abd (L"abd");
  EXPECT_TRUE (abc == std::wstring_view (L"abc"));
  EXPECT_TRUE (std::wstring_view (L"abc") == abc);
  EXPECT_TRUE (abc != std_abd);
  EXPECT_TRUE (abc < std_abd);
  EXPECT_TRUE (std_abd > abc);
  EXPECT_TRUE (abc <= std_abd);
  EXPECT_TRUE (std_abd >= abc);
}

TEST (LumexStringViewStdTest,
      GivenCStringAndStdString_WhenCompareWithView_ThenAsBefore)
{
  lumex_string_view const abc ("abc");
  std::string const text = "abc";
  EXPECT_TRUE (abc == "abc");
  EXPECT_TRUE ("abc" == abc);
  EXPECT_TRUE (abc == text);
  EXPECT_TRUE (text == abc);
  EXPECT_TRUE (abc != "abd");
  EXPECT_TRUE (abc < "abd");
  EXPECT_TRUE ("abd" > abc);
  EXPECT_TRUE (abc <= text);
  EXPECT_TRUE (text >= abc);
  EXPECT_TRUE (abc == lumex_string_view ("abc"));
}

TEST (LumexStringViewStdTest, GivenOverloads_WhenCallWithViews_ThenNoAmbiguity)
{
  lumex_string_view const view ("abc");
  std::string_view const standard ("abc");
  std::string const text = "abc";
  EXPECT_EQ (pick (view), 1);
  EXPECT_EQ (pick (standard), 1);
  EXPECT_EQ (pick ("abc"), 2);
  EXPECT_EQ (pick (text), 3);
  EXPECT_EQ (pick (text.c_str ()), 2);
  lumex_wstring_view const wide (L"abc");
  std::wstring const wtext = L"abc";
  EXPECT_EQ (pick_wide (wide), 1);
  EXPECT_EQ (pick_wide (std::wstring_view (L"abc")), 1);
  EXPECT_EQ (pick_wide (L"abc"), 2);
  EXPECT_EQ (pick_wide (wtext), 3);
}

TEST (LumexStringViewStdTest,
      GivenView_WhenUsedWhereStdViewIsExpected_ThenWorks)
{
  lumex_string_view const view ("hello world");
  // std::string accepts anything that converts to std::string_view.
  std::string text = "say ";
  text += view;
  EXPECT_EQ (text, "say hello world");
  text.append (view.substr (0, 5));
  EXPECT_EQ (text, "say hello worldhello");
  EXPECT_EQ (text.find (view.substr (6)), text.find ("world"));
  // The explicit conversion to std::string keeps working.
  EXPECT_EQ (std::string (view), "hello world");
  EXPECT_EQ (static_cast<std::string> (view), "hello world");
  // The std::string_view API on a view of the library.
  std::string_view const standard = view;
  EXPECT_EQ (standard.find ("world"), view.find ("world"));
  EXPECT_EQ (standard.compare (0, 5, "hello"), 0);
}

TEST (LumexStringViewStdTest, GivenViews_WhenStream_ThenSameText)
{
  lumex_string_view const view ("text");
  std::string_view const standard ("text");
  std::ostringstream out;
  out << view << ' ' << standard;
  EXPECT_EQ (out.str (), "text text");
  std::wostringstream wide_out;
  wide_out << lumex_wstring_view (L"wide") << L' '
           << std::wstring_view (L"wide");
  EXPECT_EQ (wide_out.str (), L"wide wide");
}
