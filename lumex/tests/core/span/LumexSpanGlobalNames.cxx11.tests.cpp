// The module declares nothing at global scope. A program that already has
// global declarations named span, dynamic_extent, byte, as_bytes,
// as_writable_bytes and to_integer (its own helpers, another library's span,
// or std names brought in with a using-directive) must still compile when it
// includes lumex/core/span/LumexSpan and uses both. A global declaration of
// any of those names in LumexLib makes this file fail to compile. The
// declarations below stand in for that program, so they carry the names they
// must not clash with rather than this library's naming rules, and this file
// does not include the shared test support header, whose using-declarations
// would make the short names ambiguous.
#include <cstddef>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

// The program's declarations.
template <typename T> struct span;

template <typename T> struct span
{
  T *first;
  std::size_t count;
};

static const std::size_t dynamic_extent = 7;

enum class byte
{
  low,
  high
};

template <typename T>
int
as_bytes (span<T> const &)
{
  return 1;
}

template <typename T>
int
as_writable_bytes (span<T> const &)
{
  return 2;
}

int
to_integer (byte value)
{
  return value == byte::high ? 1 : 0;
}

namespace lumex_view = lumex::core::span::view;

TEST (LumexSpanGlobalNamesTest,
      GivenGlobalDeclarationsOfTheProgram_WhenNamed_ThenTheyAreNotLumexNames)
{
  static_assert (!std::is_same<::span<int>, lumex_view::span<int>>::value,
                 "the global span is the program's own");
  static_assert (!std::is_same<::byte, lumex_view::byte>::value,
                 "the global byte is the program's own");
  EXPECT_EQ (::dynamic_extent, 7u);
  EXPECT_NE (::dynamic_extent, lumex_view::dynamic_extent);
  EXPECT_EQ (to_integer (byte::high), 1);

  int values[2] = { 1, 2 };
  span<int> plain = { values, 2 };
  EXPECT_EQ (plain.count, 2u);
  EXPECT_EQ (as_bytes (plain), 1);
  EXPECT_EQ (as_writable_bytes (plain), 2);
}

TEST (LumexSpanGlobalNamesTest,
      GivenBothSetsOfNames_WhenUsedTogether_ThenEachKeepsItsBehaviour)
{
  int values[3] = { 1, 2, 3 };
  span<int> plain = { values, 3 };
  lumex_view::span<int> const view (values);
  EXPECT_EQ (plain.count, view.size ());
  EXPECT_EQ (plain.first, view.data ());
  // Qualified calls reach the module's functions, unqualified ones with a
  // module argument find them by argument-dependent lookup.
  EXPECT_EQ (as_bytes (view).size (), 3 * sizeof (int));
  EXPECT_EQ (lumex_view::as_bytes (view).size (), 3 * sizeof (int));
  EXPECT_EQ (as_bytes (plain), 1);
}
