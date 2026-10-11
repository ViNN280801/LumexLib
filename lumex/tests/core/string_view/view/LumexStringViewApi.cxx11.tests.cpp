#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

// The members and constructors that the view has beyond the first version of
// the library: contains, the C string overloads of starts_with and ends_with,
// traits_type, the iterator-pair and range constructors, the _sv literals, the
// other character types, and the null-pointer views.

namespace
{
std::size_t const npos = static_cast<std::size_t> (-1);
template <class T>
void
sink (T const &)
{
}
using namespace lumex::core::string_view::view::literals;
using lumex::core::string_view::view::lumex_u16string_view;
using lumex::core::string_view::view::lumex_u32string_view;
} // namespace

// Keep these first: the trait behind the range constructor is instantiated
// once per range type, and the first question asked about a range type decides
// what a program that makes the constructor implicit would see.
static_assert (
    !std::is_convertible<std::vector<char>, lumex_string_view>::value,
    "a vector does not convert implicitly (rvalue)");
static_assert (
    !std::is_convertible<std::vector<char> &, lumex_string_view>::value,
    "a vector does not convert implicitly (lvalue)");
static_assert (
    std::is_constructible<lumex_string_view, std::vector<char> &>::value,
    "a non-const vector constructs a view explicitly");
static_assert (
    std::is_constructible<lumex_string_view, std::vector<char>>::value,
    "a vector rvalue constructs a view explicitly");

static_assert (std::is_same<lumex_string_view::traits_type,
                            std::char_traits<char>>::value,
               "traits_type");
static_assert (std::is_same<lumex_wstring_view::traits_type,
                            std::char_traits<wchar_t>>::value,
               "traits_type of the wide view");
static_assert (std::is_same<lumex_string_view::value_type, char>::value, "");
static_assert (std::is_trivially_copyable<lumex_string_view>::value,
               "a view is trivially copyable");
static_assert (std::is_trivially_copyable<lumex_wstring_view>::value, "");
static_assert (std::is_nothrow_copy_constructible<lumex_string_view>::value,
               "");
static_assert (std::is_nothrow_default_constructible<lumex_string_view>::value,
               "");
static_assert (sizeof (lumex_string_view) == 2 * sizeof (void *),
               "pointer and size");
static_assert (lumex_string_view::npos == static_cast<std::size_t> (-1), "");

TEST (LumexStringViewApiTest, GivenView_WhenContainsView_ThenSameAsFind)
{
  lumex_string_view const view ("hello world");
  EXPECT_TRUE (view.contains (lumex_string_view ("lo w")));
  EXPECT_TRUE (view.contains (lumex_string_view ("hello world")));
  EXPECT_TRUE (view.contains (lumex_string_view ("")));
  EXPECT_FALSE (view.contains (lumex_string_view ("worlds")));
  EXPECT_FALSE (view.contains (lumex_string_view ("xyz")));
  EXPECT_FALSE (lumex_string_view ().contains (lumex_string_view ("a")));
}

TEST (LumexStringViewApiTest, GivenView_WhenContainsChar_ThenSameAsFind)
{
  lumex_string_view const view ("hello world");
  EXPECT_TRUE (view.contains ('h'));
  EXPECT_TRUE (view.contains ('d'));
  EXPECT_TRUE (view.contains (' '));
  EXPECT_FALSE (view.contains ('z'));
  EXPECT_FALSE (view.contains ('\0'));
  EXPECT_FALSE (lumex_string_view ().contains ('a'));
  EXPECT_TRUE (lumex_string_view ("a\0b", 3).contains ('\0'));
}

TEST (LumexStringViewApiTest, GivenView_WhenContainsCString_ThenSameAsFind)
{
  lumex_string_view const view ("hello world");
  EXPECT_TRUE (view.contains ("o w"));
  EXPECT_TRUE (view.contains (""));
  EXPECT_FALSE (view.contains ("xyz"));
  EXPECT_FALSE (view.contains ("hello world!"));
  char const *const none = nullptr;
  EXPECT_TRUE (view.contains (none));
}

TEST (LumexStringViewApiTest, GivenWideView_WhenContains_ThenWorks)
{
  lumex_wstring_view const view (L"hello world");
  EXPECT_TRUE (view.contains (L'w'));
  EXPECT_TRUE (view.contains (L"o w"));
  EXPECT_TRUE (view.contains (lumex_wstring_view (L"world")));
  EXPECT_FALSE (view.contains (L'\x20AC'));
  EXPECT_FALSE (view.contains (L"xyz"));
}

TEST (LumexStringViewApiTest,
      GivenCStringArguments_WhenStartsOrEndsWith_ThenPrefixAndSuffix)
{
  lumex_string_view const view ("hello world");
  EXPECT_TRUE (view.starts_with ("hello"));
  EXPECT_TRUE (view.starts_with (""));
  EXPECT_FALSE (view.starts_with ("world"));
  EXPECT_FALSE (view.starts_with ("hello world!"));
  EXPECT_TRUE (view.ends_with ("world"));
  EXPECT_TRUE (view.ends_with (""));
  EXPECT_FALSE (view.ends_with ("hello"));
  EXPECT_FALSE (view.ends_with ("!hello world"));
  EXPECT_TRUE (view.starts_with ('h'));
  EXPECT_FALSE (view.starts_with ('w'));
  EXPECT_TRUE (view.ends_with ('d'));
  EXPECT_FALSE (view.ends_with ('h'));
  EXPECT_FALSE (lumex_string_view ().starts_with ('a'));
  EXPECT_FALSE (lumex_string_view ().ends_with ('a'));
  EXPECT_TRUE (lumex_string_view ().starts_with (""));
  EXPECT_TRUE (view.starts_with (lumex_string_view ("hello w")));
  EXPECT_TRUE (view.ends_with (lumex_string_view ("o world")));
  EXPECT_TRUE (view.starts_with (view)) << "a prefix as long as the view";
  EXPECT_TRUE (view.ends_with (view)) << "a suffix as long as the view";
  EXPECT_TRUE (view.starts_with ("hello world"));
  EXPECT_TRUE (view.ends_with ("hello world"));
  EXPECT_FALSE (view.ends_with ("hello w")) << "the suffix is at the end";
  EXPECT_FALSE (view.starts_with ("o world")) << "the prefix is at the start";
  EXPECT_FALSE (view.ends_with (lumex_string_view ("hello world!")));
}

TEST (LumexStringViewApiTest,
      GivenNullPointerView_WhenEveryQuery_ThenAnEmptyView)
{
  lumex_string_view const none;
  lumex_string_view const from_null_pointer (
      static_cast<char const *> (nullptr));
  lumex_string_view const from_null_and_size (nullptr, 0);
  lumex_string_view const views[]
      = { none, from_null_pointer, from_null_and_size };
  for (std::size_t i = 0; i < 3; ++i)
    {
      lumex_string_view const view = views[i];
      EXPECT_EQ (view.data (), nullptr);
      EXPECT_EQ (view.size (), 0U);
      EXPECT_TRUE (view.empty ());
      EXPECT_TRUE (view.begin () == view.end ());
      EXPECT_TRUE (view.rbegin () == view.rend ());
      EXPECT_EQ (view.compare (lumex_string_view ()), 0);
      EXPECT_EQ (view.compare ("a"), -1);
      EXPECT_EQ (view.compare (""), 0);
      EXPECT_TRUE (view == lumex_string_view ());
      EXPECT_TRUE (view.starts_with (""));
      EXPECT_TRUE (view.ends_with (""));
      EXPECT_FALSE (view.starts_with ("a"));
      EXPECT_EQ (view.substr ().size (), 0U);
      EXPECT_EQ (view.substr (0, 5).size (), 0U);
      EXPECT_THROW (sink (view.substr (1)), std::out_of_range);
      EXPECT_THROW (sink (view.at (0)), std::out_of_range);
      char buffer[2] = { 'x', 'y' };
      EXPECT_EQ (view.copy (buffer, 2), 0U);
      EXPECT_EQ (buffer[0], 'x');
      EXPECT_EQ (std::string (view).size (), 0U);
      EXPECT_EQ (view.to_string ().size (), 0U);
      lumex_string_view moved = view;
      moved.remove_prefix (3);
      moved.remove_suffix (3);
      EXPECT_TRUE (moved.empty ());
    }
}

TEST (LumexStringViewApiTest,
      GivenNullPointerView_WhenCopyOutZero_ThenNullDestinationAllowed)
{
  lumex_string_view const none;
  EXPECT_EQ (none.copy (nullptr, 0), 0U);
  lumex_string_view const text ("abc");
  EXPECT_EQ (text.copy (nullptr, 0, 3), 0U);
}

TEST (LumexStringViewApiTest, GivenView_WhenClear_ThenNullPointerAndZero)
{
  lumex_string_view view ("abc");
  view.clear ();
  EXPECT_EQ (view.data (), nullptr);
  EXPECT_EQ (view.size (), 0U);
}

TEST (LumexStringViewApiTest,
      GivenRemoveBeyondSize_WhenRemove_ThenClampedToEmpty)
{
  lumex_string_view prefix ("abc");
  prefix.remove_prefix (10);
  EXPECT_TRUE (prefix.empty ());
  lumex_string_view suffix ("abc");
  suffix.remove_suffix (10);
  EXPECT_TRUE (suffix.empty ());
  lumex_string_view both ("abcdef");
  both.remove_prefix (2);
  both.remove_suffix (1);
  EXPECT_EQ (both, lumex_string_view ("cde"));
  lumex_string_view exact ("abc");
  exact.remove_prefix (3);
  EXPECT_TRUE (exact.empty ());
  lumex_string_view whole ("abc");
  whole.remove_suffix (3);
  EXPECT_TRUE (whole.empty ());
}

TEST (LumexStringViewApiTest, GivenSwap_WhenSwap_ThenBothExchanged)
{
  lumex_string_view first ("abc");
  lumex_string_view second ("xy");
  char const *const first_data = first.data ();
  char const *const second_data = second.data ();
  first.swap (second);
  EXPECT_EQ (first.data (), second_data);
  EXPECT_EQ (first.size (), 2U);
  EXPECT_EQ (second.data (), first_data);
  EXPECT_EQ (second.size (), 3U);
}

TEST (LumexStringViewApiTest, GivenCopy_WhenCountAndPosition_ThenClamped)
{
  lumex_string_view const view ("abcdef");
  char buffer[8] = {};
  EXPECT_EQ (view.copy (buffer, 3), 3U);
  EXPECT_EQ (std::string (buffer, 3), "abc");
  EXPECT_EQ (view.copy (buffer, 10, 4), 2U);
  EXPECT_EQ (std::string (buffer, 2), "ef");
  EXPECT_EQ (view.copy (buffer, 10, 6), 0U);
  EXPECT_THROW (view.copy (buffer, 1, 7), std::out_of_range);
}

TEST (LumexStringViewApiTest, GivenSubstr_WhenCountPastTheEnd_ThenClamped)
{
  lumex_string_view const view ("abcdef");
  EXPECT_EQ (view.substr (2), lumex_string_view ("cdef"));
  EXPECT_EQ (view.substr (2, 2), lumex_string_view ("cd"));
  EXPECT_EQ (view.substr (2, 100), lumex_string_view ("cdef"));
  EXPECT_EQ (view.substr (6), lumex_string_view (""));
  EXPECT_EQ (view.substr (6).size (), 0U);
  EXPECT_EQ (view.substr (0, npos).size (), 6U);
  EXPECT_THROW (sink (view.substr (7)), std::out_of_range);
  EXPECT_EQ (view.substr (3, 3).data (), view.data () + 3);
}

TEST (LumexStringViewApiTest, GivenReverseIterators_WhenWalk_ThenBackwards)
{
  lumex_string_view const view ("abc");
  std::string back (view.rbegin (), view.rend ());
  EXPECT_EQ (back, "cba");
  std::string cback (view.crbegin (), view.crend ());
  EXPECT_EQ (cback, "cba");
  EXPECT_EQ (*view.rbegin (), 'c');
  EXPECT_EQ (*(view.rend () - 1), 'a');
  EXPECT_TRUE (view.begin () == view.cbegin ());
  EXPECT_TRUE (view.end () == view.cend ());
  EXPECT_EQ (view.end () - view.begin (), 3);
}

TEST (LumexStringViewApiTest, GivenMaxSize_WhenAsked_ThenLargePositive)
{
  EXPECT_GT (lumex_string_view::max_size (), 1000000U);
  EXPECT_LT (lumex_string_view::max_size (), npos);
}

TEST (LumexStringViewApiTest,
      GivenString_WhenConvert_ThenImplicitToViewExplicitBack)
{
  std::string const text ("a\0b", 3);
  lumex_string_view const view = text;
  EXPECT_EQ (view.size (), 3U);
  EXPECT_EQ (view.data (), text.data ());
  std::string const copy = static_cast<std::string> (view);
  EXPECT_EQ (copy, text);
  EXPECT_EQ (view.to_string (), text);
  EXPECT_FALSE ((std::is_convertible<lumex_string_view, std::string>::value));
  EXPECT_TRUE ((std::is_constructible<std::string, lumex_string_view>::value));
  std::wstring const wide (L"ab\0c", 4);
  lumex_wstring_view const wide_view = wide;
  EXPECT_EQ (wide_view.size (), 4U);
  EXPECT_EQ (static_cast<std::wstring> (wide_view), wide);
}

TEST (LumexStringViewApiTest,
      GivenPointerPair_WhenConstruct_ThenViewOfTheRange)
{
  char const text[] = "hello world";
  lumex_string_view const view (text + 6, text + 11);
  EXPECT_EQ (view, lumex_string_view ("world"));
  EXPECT_EQ (view.data (), text + 6);
  char buffer[] = "abcdef";
  lumex_string_view const mutable_view (buffer, buffer + 3);
  EXPECT_EQ (mutable_view, lumex_string_view ("abc"));
  lumex_string_view const empty_pair (text, text);
  EXPECT_TRUE (empty_pair.empty ());
  EXPECT_EQ (empty_pair.data (), text);
  lumex_string_view const sized (text, 5);
  EXPECT_EQ (sized.size (), 5U) << "(pointer, size) is still the size form";
  std::size_t const length = 4;
  EXPECT_EQ (lumex_string_view (text, length).size (), 4U);
  wchar_t const wide[] = L"wide text";
  EXPECT_EQ (lumex_wstring_view (wide + 5, wide + 9),
             lumex_wstring_view (L"text"));
}

TEST (LumexStringViewApiTest,
      GivenContiguousRanges_WhenConstruct_ThenViewOfTheRange)
{
  std::vector<char> const vec{ 'a', 'b', 'c' };
  lumex_string_view const from_vector (vec);
  EXPECT_EQ (from_vector.size (), 3U);
  EXPECT_EQ (from_vector.data (), vec.data ());
  std::array<char, 4> const arr{ { 'w', 'x', 'y', 'z' } };
  lumex_string_view const from_array (arr);
  EXPECT_EQ (from_array, lumex_string_view ("wxyz"));
  std::vector<wchar_t> const wide_vec{ L'a', L'b' };
  EXPECT_EQ (lumex_wstring_view (wide_vec).size (), 2U);
  std::vector<char> const none;
  EXPECT_TRUE (lumex_string_view (none).empty ());
}

namespace
{
struct data_and_size_of_chars
{
  char const *p;
  std::size_t n;
  char const *
  data () const
  {
    return p;
  }
  std::size_t
  size () const
  {
    return n;
  }
};
struct data_and_size_of_ints
{
  int const *
  data () const
  {
    return nullptr;
  }
  std::size_t
  size () const
  {
    return 0;
  }
};
struct foreign_traits_chars
{
  typedef std::char_traits<wchar_t> traits_type;
  char const *
  data () const
  {
    return nullptr;
  }
  std::size_t
  size () const
  {
    return 0;
  }
};
} // namespace

static_assert (
    std::is_constructible<lumex_string_view, std::vector<char> const &>::value,
    "a vector of char");
static_assert (
    !std::is_convertible<std::vector<char> const &, lumex_string_view>::value,
    "the range constructor is explicit");
static_assert (std::is_constructible<lumex_string_view,
                                     data_and_size_of_chars const &>::value,
               "data () and size ()");
static_assert (!std::is_constructible<lumex_string_view,
                                      data_and_size_of_ints const &>::value,
               "elements that are not char");
static_assert (!std::is_constructible<lumex_string_view,
                                      std::vector<wchar_t> const &>::value,
               "wide elements in a narrow view");
static_assert (!std::is_constructible<lumex_string_view,
                                      foreign_traits_chars const &>::value,
               "a range with other traits");
static_assert (!std::is_constructible<lumex_string_view, int>::value,
               "a number");
static_assert (
    std::is_convertible<std::string const &, lumex_string_view>::value,
    "a string converts implicitly (not through the range form)");
static_assert (
    std::is_convertible<char const (&)[3], lumex_string_view>::value,
    "an array converts as a C string");

TEST (LumexStringViewApiTest, GivenCharArray_WhenConstruct_ThenCStringLength)
{
  char buffer[16] = "abc";
  lumex_string_view const view (buffer);
  EXPECT_EQ (view.size (), 3U) << "an array is a C string, not a range";
  data_and_size_of_chars const custom = { "abcdef", 4 };
  EXPECT_EQ (lumex_string_view (custom).size (), 4U);
}

TEST (LumexStringViewApiTest, GivenLiterals_WhenSuffixSv_ThenLengthIncludesNul)
{
  lumex_string_view const narrow = "a\0b"_sv;
  EXPECT_EQ (narrow.size (), 3U);
  EXPECT_EQ (narrow[1], '\0');
  EXPECT_EQ (""_sv.size (), 0U);
  EXPECT_NE (""_sv.data (), nullptr);
  lumex_wstring_view const wide = L"a\0b"_sv;
  EXPECT_EQ (wide.size (), 3U);
  EXPECT_EQ (wide[2], L'b');
  lumex_u16string_view const u16 = u"ab\0"_sv;
  EXPECT_EQ (u16.size (), 3U);
  lumex_u32string_view const u32 = U"abcd"_sv;
  EXPECT_EQ (u32.size (), 4U);
  EXPECT_EQ (u32[3], U'd');
  EXPECT_TRUE ((std::is_same<decltype ("x"_sv), lumex_string_view>::value));
  EXPECT_TRUE ((std::is_same<decltype (L"x"_sv), lumex_wstring_view>::value));
  EXPECT_TRUE (
      (std::is_same<decltype (u"x"_sv), lumex_u16string_view>::value));
  EXPECT_TRUE (
      (std::is_same<decltype (U"x"_sv), lumex_u32string_view>::value));
}

TEST (LumexStringViewApiTest,
      GivenOtherCharacterTypes_WhenUse_ThenSameBehavior)
{
  lumex_u16string_view const view = u"hello world"_sv;
  EXPECT_EQ (view.find (u'w'), 6U);
  EXPECT_EQ (view.find (u"o w"), 4U);
  EXPECT_EQ (view.rfind (u'o'), 7U);
  EXPECT_TRUE (view.contains (u"world"));
  EXPECT_EQ (view.compare (u"hello"), 1);
  EXPECT_EQ (view.substr (6), u"world"_sv);
  lumex_u32string_view const wide = U"a\x1F600z"_sv;
  EXPECT_EQ (wide.size (), 3U);
  EXPECT_EQ (wide.find (U'z'), 2U);
  EXPECT_TRUE (wide > U"a"_sv);
}

TEST (LumexStringViewApiTest, GivenAtOutOfRange_WhenAt_ThenOutOfRange)
{
  lumex_string_view const view ("abc");
  EXPECT_EQ (view.at (0), 'a');
  EXPECT_EQ (view.at (2), 'c');
  EXPECT_THROW (sink (view.at (3)), std::out_of_range);
  EXPECT_THROW (sink (view.at (npos)), std::out_of_range);
  EXPECT_EQ (view.front (), 'a');
  EXPECT_EQ (view.back (), 'c');
  EXPECT_EQ (view[1], 'b');
}

namespace
{
std::size_t
size_of_view (lumex_string_view view)
{
  return view.size ();
}
} // namespace

TEST (LumexStringViewApiTest,
      GivenNonConstRanges_WhenConstruct_ThenViewOfTheRange)
{
  std::vector<char> vec;
  vec.push_back ('a');
  vec.push_back ('b');
  lumex_string_view const from_vector (vec);
  EXPECT_EQ (from_vector.size (), 2U);
  EXPECT_EQ (from_vector.data (), vec.data ());
  std::array<char, 3> arr = { { 'x', 'y', 'z' } };
  lumex_string_view const from_array (arr);
  EXPECT_EQ (from_array.size (), 3U);
  EXPECT_EQ (size_of_view (lumex_string_view (vec)), 2U);
  EXPECT_EQ (size_of_view ("abc"), 3U);
  std::wstring wide = L"wide";
  lumex_wstring_view const from_wide_string (wide);
  EXPECT_EQ (from_wide_string.size (), 4U);
}
