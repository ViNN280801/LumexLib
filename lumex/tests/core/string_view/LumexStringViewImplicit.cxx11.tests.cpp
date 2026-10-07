// LumexStringViewImplicit.cxx11.tests.cpp
//
// lumex_string_view / lumex_wstring_view convert implicitly from C strings and
// std::basic_string, like std::string_view; the conversion back to a string
// stays explicit.
#include <cstddef>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

using lumex::core::string_view::view::lumex_string_view;
using lumex::core::string_view::view::lumex_wstring_view;

namespace
{
std::size_t
narrow_length (lumex_string_view view)
{
  return view.size ();
}

std::size_t
wide_length (lumex_wstring_view view)
{
  return view.size ();
}

// Overload set that must stay unambiguous: an exact char const * overload
// next to the view overload.
int
pick (char const *)
{
  return 1;
}

int
pick (lumex_string_view)
{
  return 2;
}
} // namespace

TEST (LumexStringViewImplicitTest,
      GivenTypes_WhenConvertibility_ThenMatchesDesign)
{
  EXPECT_TRUE ((std::is_convertible<char const *, lumex_string_view>::value));
  EXPECT_TRUE (
      (std::is_convertible<char const (&)[4], lumex_string_view>::value));
  EXPECT_TRUE ((std::is_convertible<std::string, lumex_string_view>::value));
  EXPECT_TRUE (
      (std::is_convertible<std::string const &, lumex_string_view>::value));
  EXPECT_FALSE ((std::is_convertible<lumex_string_view, std::string>::value));
  EXPECT_FALSE ((std::is_convertible<int, lumex_string_view>::value));
  EXPECT_FALSE ((std::is_convertible<std::wstring, lumex_string_view>::value));

  EXPECT_TRUE (
      (std::is_convertible<wchar_t const *, lumex_wstring_view>::value));
  EXPECT_TRUE ((std::is_convertible<std::wstring, lumex_wstring_view>::value));
  EXPECT_FALSE (
      (std::is_convertible<lumex_wstring_view, std::wstring>::value));
  EXPECT_FALSE ((std::is_convertible<std::string, lumex_wstring_view>::value));
}

TEST (LumexStringViewImplicitTest, GivenLiteral_WhenPassedAsView_ThenViewsIt)
{
  EXPECT_EQ (narrow_length ("hello"), 5u);
  EXPECT_EQ (wide_length (L"hello"), 5u);
}

TEST (LumexStringViewImplicitTest, GivenString_WhenPassedAsView_ThenSameData)
{
  std::string const text ("abc");
  lumex_string_view const view = text;
  EXPECT_EQ (view.data (), text.data ());
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (narrow_length (text), 3u);

  std::wstring const wide (L"abcd");
  lumex_wstring_view const wide_view = wide;
  EXPECT_EQ (wide_view.data (), wide.data ());
  EXPECT_EQ (wide_length (wide), 4u);
}

TEST (LumexStringViewImplicitTest, GivenEmptyInputs_WhenConverted_ThenEmpty)
{
  EXPECT_EQ (narrow_length (""), 0u);
  EXPECT_EQ (narrow_length (std::string ()), 0u);
  char const *const null_text = nullptr;
  EXPECT_EQ (narrow_length (null_text), 0u);
  EXPECT_EQ (wide_length (L""), 0u);
}

TEST (LumexStringViewImplicitTest,
      GivenView_WhenComparedWithLiteral_ThenBothWays)
{
  lumex_string_view const view ("abc");
  EXPECT_TRUE (view == "abc");
  EXPECT_TRUE ("abc" == view);
  EXPECT_TRUE (view != "abd");
  EXPECT_TRUE (view < "abd");
  EXPECT_EQ (view.compare ("abc"), 0);
}

TEST (LumexStringViewImplicitTest,
      GivenView_WhenComparedWithString_ThenBothWays)
{
  std::string const text ("abc");
  lumex_string_view const view ("abc");
  EXPECT_TRUE (view == text);
  EXPECT_TRUE (text == view);
  EXPECT_FALSE (view != text);
}

TEST (LumexStringViewImplicitTest,
      GivenCharPointerOverload_WhenLiteral_ThenExact)
{
  EXPECT_EQ (pick ("x"), 1);
  EXPECT_EQ (pick (lumex_string_view ("x")), 2);
  EXPECT_EQ (pick (std::string ("x")), 2);
}

TEST (LumexStringViewImplicitTest, GivenView_WhenToString_ThenStillExplicit)
{
  lumex_string_view const view ("abc");
  std::string const copy = static_cast<std::string> (view);
  EXPECT_EQ (copy, "abc");
}
