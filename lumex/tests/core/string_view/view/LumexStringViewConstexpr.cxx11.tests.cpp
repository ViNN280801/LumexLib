#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

// Constant evaluation under the rules of C++11: a constexpr function is one
// return statement, so every query is checked here as a static_assert. The
// modifiers (remove_prefix, swap, ...) become constexpr in C++14
// (LumexStringViewConstexpr.cxx14), rbegin and rend in C++17. The long
// texts prove that the C++11 recursion over halves does not hit the limit of
// the compiler's constexpr depth (512 on GCC and Clang by default).

namespace
{
#define LUMEX_SV_TEST_TEN(x) x x x x x x x x x x

// 1000 characters, then "XYZ".
constexpr char const long_text[]
    = LUMEX_SV_TEST_TEN (LUMEX_SV_TEST_TEN ("abcdefghij")) "XYZ";
// 400 wide characters, then L"XYZ".
constexpr wchar_t const long_wide[]
    = LUMEX_SV_TEST_TEN (LUMEX_SV_TEST_TEN (L"abcd")) L"XYZ";

constexpr lumex_string_view hello = "hello world";
constexpr lumex_string_view empty;
constexpr lumex_string_view none (nullptr, 0);
constexpr lumex_string_view long_view = long_text;
constexpr lumex_wstring_view wide_hello = L"hello world";
constexpr lumex_wstring_view long_wide_view = long_wide;
constexpr std::size_t npos = lumex_string_view::npos;
using lumex::core::string_view::view::lumex_u16string_view;
using lumex::core::string_view::view::lumex_u32string_view;
} // namespace

// -- construction and observers --
static_assert (hello.size () == 11 && hello.length () == 11, "size");
static_assert (!hello.empty () && empty.empty () && none.empty (), "empty");
static_assert (hello.data ()[0] == 'h', "data");
static_assert (empty.data () == nullptr && none.data () == nullptr, "null");
static_assert (hello[4] == 'o' && hello.front () == 'h'
                   && hello.back () == 'd',
               "element access");
static_assert (hello.at (1) == 'e', "at");
static_assert (hello.begin () + 11 == hello.end (), "begin and end");
static_assert (hello.cbegin () == hello.begin ()
                   && hello.cend () == hello.end (),
               "cbegin and cend");
static_assert (lumex_string_view::max_size () > 0, "max_size");
static_assert (lumex_string_view ("abc").size () == 3,
               "the C string constructor");
static_assert (
    lumex_string_view (static_cast<char const *> (nullptr)).empty (),
    "a null C string");
static_assert (lumex_string_view ("abcdef", 3).size () == 3,
               "pointer and size");
static_assert (long_view.size () == 1003, "a long C string");
static_assert (wide_hello.size () == 11, "a wide C string");
static_assert (long_wide_view.size () == 403, "a long wide C string");
static_assert (lumex_string_view::npos == static_cast<std::size_t> (-1),
               "npos");

// -- substr --
static_assert (hello.substr (6) == "world", "substr");
static_assert (hello.substr (6, 2) == "wo", "substr count");
static_assert (hello.substr (11).empty (), "substr at the end");
static_assert (hello.substr (3, 100) == "lo world", "substr is clamped");
static_assert (long_view.substr (1000) == "XYZ", "substr of a long text");

// -- comparison --
static_assert (hello.compare ("hello world") == 0, "compare equal");
static_assert (hello.compare ("hello") > 0, "compare longer");
static_assert (hello.compare ("help") < 0, "compare less");
static_assert (hello.compare (lumex_string_view ("hello world!")) < 0,
               "compare shorter");
static_assert (hello.compare (6, 5, "world") == 0, "compare substring");
static_assert (hello.compare (6, 5, lumex_string_view ("xworldx"), 1, 5) == 0,
               "compare two substrings");
static_assert (hello.compare (6, 5, "world!", 5) == 0, "compare counted");
static_assert (lumex_string_view ("\x80").compare ("a") > 0, "unsigned order");
static_assert (
    lumex_string_view ("a\0b", 3).compare (lumex_string_view ("a\0c", 3)) < 0,
    "embedded NUL");
static_assert (long_view.compare (long_view) == 0, "compare long");
static_assert (long_view.compare (long_view.substr (0, 1002)) > 0,
               "compare long, shorter");
static_assert (hello == "hello world" && hello != "hello" && "a" < hello
                   && hello > "a" && hello <= hello && hello >= hello,
               "six operators");
static_assert (long_view == long_view, "== on long texts");

// -- prefix, suffix, containment --
static_assert (hello.starts_with ("hello") && hello.starts_with ('h')
                   && hello.starts_with (lumex_string_view ("hell")),
               "starts_with");
static_assert (!hello.starts_with ("world") && !hello.starts_with ('w'),
               "not starts_with");
static_assert (hello.ends_with ("world") && hello.ends_with ('d')
                   && hello.ends_with (lumex_string_view ("rld")),
               "ends_with");
static_assert (!hello.ends_with ("hello") && !hello.ends_with ('h'),
               "not ends_with");
static_assert (hello.contains ("o w") && hello.contains ('w')
                   && hello.contains (lumex_string_view ("hello")),
               "contains");
static_assert (!hello.contains ("xyz") && !hello.contains ('z'),
               "not contains");

// -- find --
static_assert (hello.find ("wor") == 6, "find");
static_assert (hello.find ("o", 5) == 7, "find from a position");
static_assert (hello.find ("") == 0 && hello.find ("", 11) == 11
                   && hello.find ("", 12) == npos,
               "find empty");
static_assert (hello.find ("hello world!") == npos, "find longer");
static_assert (hello.find ('o') == 4 && hello.find ('o', 5) == 7
                   && hello.find ('z') == npos,
               "find char");
static_assert (hello.find ("xyz", 0, 0) == 0 && hello.find ("wxyz", 0, 1) == 6,
               "find counted");
static_assert (hello.find (lumex_string_view ("ld")) == 9, "find view");
static_assert (long_view.find ("XYZ") == 1000, "find in a long text");
static_assert (long_view.find ("jab") == 9, "find across a repeat");
static_assert (long_view.find ('Z') == 1002, "find char in a long text");
static_assert (long_view.find ("Z", 1003) == npos, "find past the end");
static_assert (long_wide_view.find (L"XYZ") == 400,
               "find in a long wide text");
static_assert (empty.find ('a') == npos && none.find ("a") == npos
                   && none.find ("") == 0,
               "find in an empty view");

// -- rfind --
static_assert (hello.rfind ("o") == 7 && hello.rfind ("o", 6) == 4, "rfind");
static_assert (hello.rfind ("") == 11 && hello.rfind ("", 3) == 3,
               "rfind empty");
static_assert (hello.rfind ('l') == 9 && hello.rfind ('l', 8) == 3
                   && hello.rfind ('z') == npos,
               "rfind char");
static_assert (hello.rfind ("hello world!") == npos, "rfind longer");
static_assert (hello.rfind ("lo", npos, 1) == 9, "rfind counted");
static_assert (long_view.rfind ("abc") == 990, "rfind in a long text");
static_assert (long_view.rfind ('a', 989) == 980,
               "rfind char from a position");
static_assert (empty.rfind ('a') == npos, "rfind in an empty view");

// -- the sets --
static_assert (hello.find_first_of ("xyw") == 6
                   && hello.find_first_of ("ol", 5) == 7
                   && hello.find_first_of ('l') == 2
                   && hello.find_first_of ("xyz") == npos,
               "find_first_of");
static_assert (hello.find_last_of ("lo") == 9
                   && hello.find_last_of ("lo", 8) == 7
                   && hello.find_last_of ('l') == 9
                   && hello.find_last_of ("") == npos,
               "find_last_of");
static_assert (hello.find_first_not_of ("hel") == 4
                   && hello.find_first_not_of ('h') == 1
                   && hello.find_first_not_of ("") == 0
                   && hello.find_first_not_of ("helo wrd") == npos,
               "find_first_not_of");
static_assert (hello.find_last_not_of ("dl") == 8
                   && hello.find_last_not_of ('d') == 9
                   && hello.find_last_not_of ("") == 10
                   && hello.find_last_not_of ("helo wrd") == npos,
               "find_last_not_of");
static_assert (long_view.find_first_of ("XYZ") == 1000
                   && long_view.find_last_of ("XYZ") == 1002
                   && long_view.find_first_not_of ("abcdefghij") == 1000
                   && long_view.find_last_not_of ("XYZ") == 999,
               "the sets in a long text");
static_assert (hello.find_first_of ("xxw", 0, 2) == npos
                   && hello.find_first_of ("xxw", 0, 3) == 6,
               "set counted");

// -- the wide twin --
static_assert (wide_hello.find (L"wor") == 6 && wide_hello.rfind (L'o') == 7
                   && wide_hello.find_first_of (L"xyw") == 6
                   && wide_hello.find_last_not_of (L"dl") == 8
                   && wide_hello.compare (L"hello") > 0
                   && wide_hello.substr (6) == L"world",
               "the wide view");

// The length of a wide C string in a constant expression (C++11: four
// characters per step): every remainder of a division by 4.
static_assert (lumex_wstring_view (L"").size () == 0,
               "lumex_wstring_view length 0");
static_assert (lumex_wstring_view (L"a").size () == 1,
               "lumex_wstring_view length 1");
static_assert (lumex_wstring_view (L"ab").size () == 2,
               "lumex_wstring_view length 2");
static_assert (lumex_wstring_view (L"abc").size () == 3,
               "lumex_wstring_view length 3");
static_assert (lumex_wstring_view (L"abcd").size () == 4,
               "lumex_wstring_view length 4");
static_assert (lumex_wstring_view (L"abcde").size () == 5,
               "lumex_wstring_view length 5");
static_assert (lumex_wstring_view (L"abcdef").size () == 6,
               "lumex_wstring_view length 6");
static_assert (lumex_wstring_view (L"abcdefg").size () == 7,
               "lumex_wstring_view length 7");
static_assert (lumex_wstring_view (L"abcdefgh").size () == 8,
               "lumex_wstring_view length 8");
static_assert (lumex_wstring_view (L"abcdefghi").size () == 9,
               "lumex_wstring_view length 9");
static_assert (lumex_wstring_view (L"abcdefghij").size () == 10,
               "lumex_wstring_view length 10");
static_assert (lumex_wstring_view (L"abcdefghijk").size () == 11,
               "lumex_wstring_view length 11");
static_assert (lumex_u16string_view (u"").size () == 0,
               "lumex_u16string_view length 0");
static_assert (lumex_u16string_view (u"a").size () == 1,
               "lumex_u16string_view length 1");
static_assert (lumex_u16string_view (u"ab").size () == 2,
               "lumex_u16string_view length 2");
static_assert (lumex_u16string_view (u"abc").size () == 3,
               "lumex_u16string_view length 3");
static_assert (lumex_u16string_view (u"abcd").size () == 4,
               "lumex_u16string_view length 4");
static_assert (lumex_u16string_view (u"abcde").size () == 5,
               "lumex_u16string_view length 5");
static_assert (lumex_u16string_view (u"abcdef").size () == 6,
               "lumex_u16string_view length 6");
static_assert (lumex_u16string_view (u"abcdefg").size () == 7,
               "lumex_u16string_view length 7");
static_assert (lumex_u16string_view (u"abcdefgh").size () == 8,
               "lumex_u16string_view length 8");
static_assert (lumex_u16string_view (u"abcdefghi").size () == 9,
               "lumex_u16string_view length 9");
static_assert (lumex_u16string_view (u"abcdefghij").size () == 10,
               "lumex_u16string_view length 10");
static_assert (lumex_u16string_view (u"abcdefghijk").size () == 11,
               "lumex_u16string_view length 11");
static_assert (lumex_u32string_view (U"").size () == 0,
               "lumex_u32string_view length 0");
static_assert (lumex_u32string_view (U"a").size () == 1,
               "lumex_u32string_view length 1");
static_assert (lumex_u32string_view (U"ab").size () == 2,
               "lumex_u32string_view length 2");
static_assert (lumex_u32string_view (U"abc").size () == 3,
               "lumex_u32string_view length 3");
static_assert (lumex_u32string_view (U"abcd").size () == 4,
               "lumex_u32string_view length 4");
static_assert (lumex_u32string_view (U"abcde").size () == 5,
               "lumex_u32string_view length 5");
static_assert (lumex_u32string_view (U"abcdef").size () == 6,
               "lumex_u32string_view length 6");
static_assert (lumex_u32string_view (U"abcdefg").size () == 7,
               "lumex_u32string_view length 7");
static_assert (lumex_u32string_view (U"abcdefgh").size () == 8,
               "lumex_u32string_view length 8");
static_assert (lumex_u32string_view (U"abcdefghi").size () == 9,
               "lumex_u32string_view length 9");
static_assert (lumex_u32string_view (U"abcdefghij").size () == 10,
               "lumex_u32string_view length 10");
static_assert (lumex_u32string_view (U"abcdefghijk").size () == 11,
               "lumex_u32string_view length 11");

TEST (LumexStringViewConstexprTest,
      GivenConstantExpressions_WhenRunAtRunTime_ThenSameResults)
{
  lumex_string_view const view = hello;
  EXPECT_EQ (view.find ("wor"), 6U);
  EXPECT_EQ (view.rfind ('o'), 7U);
  EXPECT_EQ (view.find_first_of ("xyw"), 6U);
  EXPECT_EQ (view.find_last_not_of ("dl"), 8U);
  EXPECT_EQ (view.substr (6), lumex_string_view ("world"));
  EXPECT_EQ (view.compare (6, 5, "world"), 0);
  EXPECT_EQ (long_view.find ("XYZ"), 1000U);
  EXPECT_EQ (long_view.rfind ("abc"), 990U);
  EXPECT_EQ (long_wide_view.find (L"XYZ"), 400U);
}

TEST (LumexStringViewConstexprTest,
      GivenRuntimeLongText_WhenSearch_ThenSameAsConstant)
{
  std::string text (1000000, 'a');
  text += "XYZ";
  lumex_string_view const view (text);
  EXPECT_EQ (view.size (), 1000003U);
  EXPECT_EQ (view.find ("XYZ"), 1000000U);
  EXPECT_EQ (view.rfind ("aXY"), 999999U);
  EXPECT_EQ (view.find_last_not_of ("XYZ"), 999999U);
  EXPECT_EQ (view.find_first_of ("Z"), 1000002U);
  EXPECT_EQ (view.compare (view), 0);
}
