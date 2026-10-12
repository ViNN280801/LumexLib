#include <cstddef>

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

// C++14 relaxed constexpr: the modifiers of the view, copy and a function
// body with loops and variables work in constant expressions
// (LumexStringViewConstexpr.cxx11 has the queries).

namespace
{
constexpr std::size_t
trimmed_size ()
{
  lumex_string_view view = "  padded text  ";
  while (!view.empty () && view.front () == ' ')
    view.remove_prefix (1);
  while (!view.empty () && view.back () == ' ')
    view.remove_suffix (1);
  return view.size ();
}

constexpr bool
split_and_swap ()
{
  lumex_string_view first = "key=value";
  std::size_t const eq = first.find ('=');
  lumex_string_view second = first.substr (eq + 1);
  first.remove_suffix (first.size () - eq);
  first.swap (second);
  return first == "value" && second == "key";
}

constexpr std::size_t
cleared_size ()
{
  lumex_string_view view = "abc";
  view.clear ();
  return view.size () + (view.data () == nullptr ? 100 : 0);
}

constexpr char
copied_char (std::size_t idx)
{
  char buffer[8] = {};
  lumex_string_view const view = "abcdef";
  std::size_t const copied = view.copy (buffer, 4, 1);
  return copied == 4 ? buffer[idx] : '?';
}

constexpr std::size_t
copy_count ()
{
  char buffer[8] = {};
  return lumex_string_view ("abc").copy (buffer, 10, 1);
}

constexpr std::size_t
count_words (lumex_string_view text)
{
  std::size_t words = 0;
  while (true)
    {
      std::size_t const begin = text.find_first_not_of (' ');
      if (begin == lumex_string_view::npos)
        return words;
      text.remove_prefix (begin);
      std::size_t const end = text.find (' ');
      ++words;
      if (end == lumex_string_view::npos)
        return words;
      text.remove_prefix (end);
    }
}

constexpr lumex_wstring_view
wide_tail ()
{
  lumex_wstring_view view = L"wide text";
  view.remove_prefix (5);
  return view;
}
} // namespace

static_assert (trimmed_size () == 11, "remove_prefix and remove_suffix");
static_assert (split_and_swap (), "substr, remove_suffix and swap");
static_assert (cleared_size () == 100, "clear");
static_assert (copied_char (0) == 'b' && copied_char (3) == 'e',
               "copy at a position");
static_assert (copy_count () == 2, "copy is clamped");
static_assert (count_words ("  the quick  brown fox ") == 4,
               "a loop over finds");
static_assert (wide_tail () == L"text", "the wide modifiers");
static_assert (wide_tail ().size () == 4, "the wide view after remove_prefix");

TEST (LumexStringViewConstexprCxx14Test,
      GivenRelaxedConstexpr_WhenRunAtRunTime_ThenSameResults)
{
  EXPECT_EQ (trimmed_size (), 11U);
  EXPECT_TRUE (split_and_swap ());
  EXPECT_EQ (cleared_size (), 100U);
  EXPECT_EQ (copied_char (3), 'e');
  EXPECT_EQ (copy_count (), 2U);
  EXPECT_EQ (count_words ("a b   c"), 3U);
  EXPECT_EQ (count_words (""), 0U);
  EXPECT_EQ (count_words ("   "), 0U);
}
