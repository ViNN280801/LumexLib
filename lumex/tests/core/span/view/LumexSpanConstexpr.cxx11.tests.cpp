// LumexSpanConstexpr.cxx11.tests.cpp
//
// Everything a C++11 constant expression allows: construction from arrays
// and pointers, the observers, the subviews and a recursive function over
// a span. The checks are static_assert, so a member that stops being
// constexpr stops the file from compiling.
#include <cstddef>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;

namespace
{
constexpr int numbers[5] = { 3, 1, 4, 1, 5 };

constexpr span<int const> all_numbers (numbers);
constexpr span<int const, 5> fixed_numbers (numbers);
constexpr span<int const> no_numbers;

constexpr int
sum_of (span<int const> view)
{
  return view.empty () ? 0 : view.front () + sum_of (view.subspan (1));
}

constexpr int
last_of (span<int const> view)
{
  return view.back ();
}

constexpr std::size_t
count_of (span<int const> view, int wanted)
{
  return view.empty () ? 0
                       : (view.front () == wanted ? 1 : 0)
                             + count_of (view.subspan (1), wanted);
}
} // namespace

static_assert (all_numbers.size () == 5, "size");
static_assert (all_numbers.size_bytes () == 5 * sizeof (int), "size_bytes");
static_assert (!all_numbers.empty (), "empty");
static_assert (no_numbers.empty (), "default is empty");
static_assert (no_numbers.data () == nullptr, "default has no data");
static_assert (all_numbers.data () == numbers, "data");
static_assert (all_numbers[0] == 3, "operator[]");
static_assert (all_numbers[4] == 5, "operator[]");
static_assert (all_numbers.front () == 3, "front");
static_assert (all_numbers.back () == 5, "back");
static_assert (all_numbers.at (2) == 4, "at in range");
static_assert (all_numbers.begin () == numbers, "begin");
static_assert (all_numbers.end () == numbers + 5, "end");
static_assert (all_numbers.cbegin () == numbers, "cbegin");
static_assert (all_numbers.cend () == numbers + 5, "cend");

static_assert (fixed_numbers.size () == 5, "static size");
static_assert (decltype (fixed_numbers)::extent == 5, "static extent");
static_assert (fixed_numbers[3] == 1, "static operator[]");

static_assert (all_numbers.first (2).size () == 2, "first");
static_assert (all_numbers.first (2).back () == 1, "first");
static_assert (all_numbers.last (2).front () == 1, "last");
static_assert (all_numbers.last (2).back () == 5, "last");
static_assert (all_numbers.subspan (1).size () == 4, "subspan");
static_assert (all_numbers.subspan (1, 2).back () == 4, "subspan");
static_assert (all_numbers.subspan (5).empty (), "subspan at the end");
static_assert (all_numbers.first<3> ().size () == 3, "first<3>");
static_assert (decltype (all_numbers.first<3> ())::extent == 3,
               "first<3> has a static extent");
static_assert (all_numbers.last<2> ().front () == 1, "last<2>");
static_assert (all_numbers.subspan<1, 3> ().back () == 1, "subspan<1, 3>");
static_assert (decltype (fixed_numbers.subspan<2> ())::extent == 3,
               "subspan<2> of a static extent");
static_assert (fixed_numbers.subspan<2> ().front () == 4, "subspan<2>");

static_assert (sum_of (all_numbers) == 14, "recursive sum over subspan");
static_assert (sum_of (fixed_numbers) == 14, "implicit static to dynamic");
static_assert (sum_of (no_numbers) == 0, "empty");
static_assert (sum_of (all_numbers.last (3)) == 10, "sum of the last three");
static_assert (last_of (all_numbers) == 5, "back in a constexpr function");
static_assert (count_of (all_numbers, 1) == 2, "count of a value");
static_assert (count_of (all_numbers, 9) == 0, "count of a missing value");

constexpr span<int const> from_pointer_and_count (numbers + 1, 3);
static_assert (from_pointer_and_count.size () == 3, "pointer and count");
static_assert (from_pointer_and_count.front () == 1, "pointer and count");

constexpr span<int const> from_pointer_pair (numbers + 1, numbers + 4);
static_assert (from_pointer_pair.size () == 3, "pointer pair");
static_assert (from_pointer_pair.back () == 1, "pointer pair");

constexpr span<int const, 2> fixed_from_pointers (numbers, numbers + 2);
static_assert (fixed_from_pointers.size () == 2, "static pointer pair");

constexpr span<int const> converted = fixed_numbers;
static_assert (converted.size () == 5, "span conversion");
static_assert (converted.data () == numbers, "span conversion");

constexpr span<int const, 5> explicitly_fixed (all_numbers);
static_assert (explicitly_fixed.size () == 5, "dynamic to static");

TEST (LumexSpanConstexprTest, GivenConstantExpressions_WhenRun_ThenSameValues)
{
  // The same expressions at run time, through a volatile-free path the
  // optimizer cannot fold away by constant evaluation of the file above.
  int values[5] = { 3, 1, 4, 1, 5 };
  span<int const> const view (values);
  EXPECT_EQ (sum_of (view), 14);
  EXPECT_EQ (count_of (view, 1), 2u);
  EXPECT_EQ (last_of (view), 5);
  EXPECT_EQ (sum_of (view.last (3)), 10);
}

TEST (LumexSpanConstexprTest, GivenConstexprSpan_WhenOdrUsed_ThenLinks)
{
  span<int const> const *address = &all_numbers;
  EXPECT_EQ (address->size (), 5u);
  EXPECT_EQ (fixed_numbers.size (), 5u);
  EXPECT_TRUE (no_numbers.empty ());
}
