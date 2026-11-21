// LumexSpanLiteral.cxx11.tests.cpp
//
// span is a literal type at every standard of the build (todo 108): a
// constexpr object built by the constructor from a range, the copy
// constructor and the empty static span, and a trivial destructor for both
// kinds of extent. The pointer and array forms are in
// LumexSpanConstexpr.cxx11. A static_assert here stops the build when a
// constructor loses its constexpr.
#include <cstddef>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

namespace
{
using lumex::core::span::view::span;

// A contiguous range that is a literal type: data () and size () are
// constexpr, so the range constructors can be constant-evaluated.
struct literal_range
{
  int const *first;
  std::size_t count;

  constexpr int const *
  data () const
  {
    return first;
  }

  constexpr std::size_t
  size () const
  {
    return count;
  }
};

constexpr int numbers[3] = { 7, 8, 9 };
constexpr literal_range range = { numbers, 3 };

constexpr span<int const> k_from_range (range);
constexpr span<int const, 3> k_fixed_from_range (range);
constexpr span<int const> k_copy (k_from_range);
constexpr span<int const, 0> k_empty_fixed;

static_assert (std::is_trivially_destructible<span<int>>::value,
               "span of dynamic extent");
static_assert (std::is_trivially_destructible<span<int, 4>>::value,
               "span of static extent");
static_assert (k_from_range.size () == 3 && k_from_range[2] == 9,
               "from a range");
static_assert (k_fixed_from_range.size () == 3 && k_fixed_from_range[0] == 7,
               "from a range, static extent");
static_assert (k_copy.data () == numbers && k_copy.size () == 3, "copy");
static_assert (k_empty_fixed.empty () && k_empty_fixed.data () == nullptr,
               "empty static span");
} // namespace

TEST (LumexSpanLiteralTest, ConstantObjects_AgreeWithTheRunTimeOnes)
{
  EXPECT_EQ (k_from_range.size (), 3U);
  EXPECT_EQ (k_fixed_from_range.back (), 9);
  EXPECT_EQ (k_copy.front (), 7);
  EXPECT_TRUE (k_empty_fixed.empty ());
}
