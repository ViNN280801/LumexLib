// LumexStringViewPortable.cxx11.tests.cpp
//
// portable_string_view_t / portable_wstring_view_t name the standard view from
// C++17 and the view of the module below it. The C++17 and C++20 suites
// compile this file too.
#include <string>
#include <type_traits>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

using lumex::core::string_view::view::lumex_string_view;
using lumex::core::string_view::view::lumex_wstring_view;
using lumex::core::string_view::view::portable_string_view_t;
using lumex::core::string_view::view::portable_wstring_view_t;

#if __cplusplus >= 201703L
static_assert (std::is_same<portable_string_view_t, std::string_view>::value,
               "from C++17 the portable view is the standard one");
static_assert (std::is_same<portable_wstring_view_t, std::wstring_view>::value,
               "from C++17 the portable wide view is the standard one");
#else
static_assert (std::is_same<portable_string_view_t, lumex_string_view>::value,
               "below C++17 the portable view is the view of the module");
static_assert (
    std::is_same<portable_wstring_view_t, lumex_wstring_view>::value,
    "below C++17 the portable wide view is the view of the module");
#endif

TEST (LumexStringViewPortableTest, GivenAnySource_WhenConvert_ThenAccepted)
{
  // What every standard accepts: a C string, a std::string and a view of the
  // module (the last one converts to std::string_view from C++17).
  std::string const text = "portable";
  EXPECT_TRUE (
      (std::is_convertible<char const *, portable_string_view_t>::value));
  EXPECT_TRUE ((std::is_convertible<std::string const &,
                                    portable_string_view_t>::value));
  EXPECT_TRUE (
      (std::is_convertible<lumex_string_view, portable_string_view_t>::value));
  EXPECT_TRUE (
      (std::is_convertible<wchar_t const *, portable_wstring_view_t>::value));
  EXPECT_TRUE ((std::is_convertible<std::wstring const &,
                                    portable_wstring_view_t>::value));
  EXPECT_TRUE ((std::is_convertible<lumex_wstring_view,
                                    portable_wstring_view_t>::value));
  EXPECT_FALSE (
      (std::is_convertible<portable_string_view_t, std::string>::value));
  portable_string_view_t const from_text = text;
  portable_string_view_t const from_view = lumex_string_view (text);
  portable_string_view_t const from_literal = "portable";
  EXPECT_EQ (from_text.size (), text.size ());
  EXPECT_EQ (from_view.data (), text.data ());
  EXPECT_EQ (from_literal.size (), 8U);
  portable_wstring_view_t const wide = L"wide";
  EXPECT_EQ (wide.size (), 4U);
}
