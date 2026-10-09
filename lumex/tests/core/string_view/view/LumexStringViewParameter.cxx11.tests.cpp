// LumexStringViewParameter.cxx11.tests.cpp
//
// lumex_string_view / lumex_wstring_view are the string view parameter types
// of the whole library in every standard: a function that takes one by value
// gets the same overload resolution at C++11 and at C++23. The overload sets
// below are the shape of base64, crc, exceptions and xml (the view next to
// `char const *`, `std::string` and pointer and size). The C++17 and C++20
// suites compile this file too and add the std::string_view arguments, which
// convert through the member-template constructor.
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

using lumex::core::string_view::view::lumex_string_view;
using lumex::core::string_view::view::lumex_wstring_view;

namespace
{
// Which overload was chosen.
enum chosen_t
{
  by_view = 1,
  by_c_string = 2,
  by_string = 3,
  by_pointer_and_size = 4
};

// The set that has a view next to a C string, a std::string and a sized
// pointer: nothing is ambiguous for any argument form.
int
choose (lumex_string_view)
{
  return by_view;
}
int
choose (char const *)
{
  return by_c_string;
}
int
choose (std::string const &)
{
  return by_string;
}
int
choose (char const *, std::size_t)
{
  return by_pointer_and_size;
}

// The set where the view is the only text parameter (the shape of the base64
// and xml wrappers): every argument form converts to it.
std::size_t
only_view_size (lumex_string_view view)
{
  return view.size ();
}
std::size_t
only_wide_view_size (lumex_wstring_view view)
{
  return view.size ();
}

template <class T, class = void> struct picks_view : std::false_type
{
};
template <class T>
struct picks_view<T, decltype (void (only_view_size (std::declval<T> ())))>
    : std::true_type
{
};
template <class T, class = void> struct picks_wide_view : std::false_type
{
};
template <class T>
struct picks_wide_view<T, decltype (void (only_wide_view_size (
                              std::declval<T> ())))> : std::true_type
{
};
} // namespace

static_assert (picks_view<char const (&)[4]>::value, "a literal");
static_assert (picks_view<char (&)[4]>::value, "a char array");
static_assert (picks_view<char const *>::value, "a char const *");
static_assert (picks_view<char *>::value, "a char *");
static_assert (picks_view<std::string>::value, "a std::string");
static_assert (picks_view<std::string const &>::value, "a std::string &");
static_assert (picks_view<lumex_string_view>::value, "the view");
static_assert (picks_view<lumex_string_view const &>::value, "the view &");
static_assert (picks_view<std::nullptr_t>::value, "nullptr: an empty view");
static_assert (!picks_view<int>::value, "a number is not text");
static_assert (!picks_view<wchar_t const *>::value, "a wide text");
static_assert (!picks_view<std::wstring>::value, "a wide string");
static_assert (!picks_view<lumex_wstring_view>::value, "a wide view");
static_assert (picks_wide_view<wchar_t const (&)[4]>::value, "a wide literal");
static_assert (picks_wide_view<wchar_t const *>::value, "a wchar_t const *");
static_assert (picks_wide_view<std::wstring>::value, "a std::wstring");
static_assert (picks_wide_view<lumex_wstring_view>::value, "the wide view");
static_assert (picks_wide_view<std::nullptr_t>::value, "nullptr: empty");
static_assert (!picks_wide_view<char const *>::value, "a narrow text");
static_assert (!picks_wide_view<std::string>::value, "a narrow string");
static_assert (!picks_wide_view<lumex_string_view>::value, "a narrow view");
#if __cplusplus >= 201703L
static_assert (picks_view<std::string_view>::value,
               "a std::string_view converts to the view");
static_assert (picks_view<std::string_view const &>::value,
               "a const std::string_view & converts to the view");
static_assert (picks_wide_view<std::wstring_view>::value,
               "a std::wstring_view converts to the wide view");
static_assert (!picks_view<std::wstring_view>::value,
               "a wide std view is not a narrow text");
static_assert (!picks_wide_view<std::string_view>::value,
               "a narrow std view is not a wide text");
static_assert (
    std::is_nothrow_constructible<lumex_string_view, std::string_view>::value,
    "the conversion does not throw");
#endif

TEST (LumexStringViewParameterTest,
      GivenArgumentForms_WhenChoose_ThenNoAmbiguity)
{
  std::string const text = "text";
  char buffer[] = "text";
  EXPECT_EQ (choose ("literal"), static_cast<int> (by_c_string));
  EXPECT_EQ (choose (static_cast<char const *> (buffer)),
             static_cast<int> (by_c_string));
  EXPECT_EQ (choose (buffer), static_cast<int> (by_c_string));
  EXPECT_EQ (choose (nullptr), static_cast<int> (by_c_string));
  EXPECT_EQ (choose (text), static_cast<int> (by_string));
  EXPECT_EQ (choose (std::string ("temporary")), static_cast<int> (by_string));
  EXPECT_EQ (choose (lumex_string_view (text)), static_cast<int> (by_view));
  EXPECT_EQ (choose ("pointer", 3U), static_cast<int> (by_pointer_and_size));
  EXPECT_EQ (choose (nullptr, 0U), static_cast<int> (by_pointer_and_size));
#if __cplusplus >= 201703L
  // The only way a std::string_view reaches the set is the conversion.
  EXPECT_EQ (choose (std::string_view (text)), static_cast<int> (by_view));
  EXPECT_EQ (choose (std::string_view ()), static_cast<int> (by_view));
#endif
}

TEST (LumexStringViewParameterTest, GivenOnlyView_WhenCall_ThenSizeAndDataKept)
{
  std::string const zeros ("a\0b\0", 4);
  EXPECT_EQ (only_view_size ("abc"), 3U);
  EXPECT_EQ (only_view_size (zeros), 4U);
  EXPECT_EQ (only_view_size (static_cast<char const *> (nullptr)), 0U);
  EXPECT_EQ (only_view_size (nullptr), 0U);
  EXPECT_EQ (only_view_size (lumex_string_view ()), 0U);
  EXPECT_EQ (only_view_size (lumex_string_view ("abc", 2)), 2U);
  EXPECT_EQ (only_wide_view_size (L"abc"), 3U);
  EXPECT_EQ (only_wide_view_size (nullptr), 0U);
  EXPECT_EQ (only_wide_view_size (std::wstring (L"ab")), 2U);
#if __cplusplus >= 201703L
  EXPECT_EQ (only_view_size (std::string_view ("abc", 2)), 2U);
  EXPECT_EQ (only_view_size (std::string_view (zeros)), 4U);
  EXPECT_EQ (only_view_size (std::string_view ()), 0U);
  EXPECT_EQ (only_wide_view_size (std::wstring_view (L"abc", 2)), 2U);
  EXPECT_EQ (only_wide_view_size (std::wstring_view ()), 0U);
  // The pointer is the one of the standard view: nothing is copied.
  std::string_view const standard (zeros);
  lumex_string_view const view = standard;
  EXPECT_EQ (view.data (), zeros.data ());
  EXPECT_EQ (view.size (), zeros.size ());
#endif
}
