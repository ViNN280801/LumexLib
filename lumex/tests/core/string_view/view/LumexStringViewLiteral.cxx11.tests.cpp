// LumexStringViewLiteral.cxx11.tests.cpp
//
// lumex_basic_string_view is a literal type at every standard of the build
// (todo 108): a constexpr object built by each constructor and a trivial
// destructor. The constructors are constexpr from C++11 (an empty body); a
// static_assert here stops the build when one of them loses its constexpr.
// The constructor from std::basic_string is not constexpr before C++20 (the
// string is not a literal type), so it is not pinned. The conversion from
// std::string_view is pinned in LumexStringViewConstexpr.cxx17.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

namespace
{
// A contiguous range of characters that is a literal type: data () and size ()
// are constexpr, so the explicit range constructor can be constant-evaluated.
struct literal_char_range
{
  char const *first;
  unsigned long count;

  constexpr char const *
  data () const
  {
    return first;
  }

  constexpr unsigned long
  size () const
  {
    return count;
  }
};

constexpr char const text[] = "hello world";
constexpr literal_char_range range = { text, 5 };

constexpr lumex_string_view k_default;
constexpr lumex_string_view k_null (nullptr);
constexpr lumex_string_view k_c_string = text;
constexpr lumex_string_view k_counted (text, 5);
constexpr lumex_string_view k_pointers (text, text + 5);
constexpr lumex_string_view k_range (range);
constexpr lumex_string_view k_copy (k_counted);
constexpr lumex_wstring_view k_wide = L"wide";

static_assert (std::is_trivially_destructible<lumex_string_view>::value,
               "lumex_string_view");
static_assert (std::is_trivially_destructible<lumex_wstring_view>::value,
               "lumex_wstring_view");
static_assert (std::is_trivially_copy_constructible<lumex_string_view>::value,
               "the copy constructor stays trivial");

static_assert (k_default.empty () && k_default.data () == nullptr, "default");
static_assert (k_null.empty (), "from nullptr");
static_assert (k_c_string.size () == 11, "from a C string");
static_assert (k_counted.size () == 5 && k_counted.data () == text,
               "from a pointer and a length");
static_assert (k_pointers.size () == 5 && k_pointers.data () == text,
               "from a pair of pointers");
static_assert (k_range.size () == 5 && k_range.data () == text,
               "from a range");
static_assert (k_copy == k_counted, "copy");
static_assert (k_wide.size () == 4, "wide");
} // namespace

TEST (LumexStringViewLiteralTest, ConstantObjects_AgreeWithTheRunTimeOnes)
{
  EXPECT_TRUE (k_default.empty ());
  EXPECT_TRUE (k_null.empty ());
  EXPECT_EQ (k_c_string, lumex_string_view ("hello world"));
  EXPECT_EQ (k_counted, lumex_string_view ("hello"));
  EXPECT_EQ (k_pointers, k_counted);
  EXPECT_EQ (k_range, k_counted);
  EXPECT_EQ (k_copy, k_counted);
  EXPECT_EQ (k_wide, lumex_wstring_view (L"wide"));
}
