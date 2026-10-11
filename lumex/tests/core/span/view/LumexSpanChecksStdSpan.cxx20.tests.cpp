// LumexSpanChecksStdSpan.cxx20.tests.cpp
//
// The precondition check of the explicit conversion from a dynamic
// std::span<std::byte> to a span of byte with a static extent, and of the
// range constructor given a std::span.
#include <cstddef>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

#if LUMEX_SPAN_HAS_STD_SPAN

using lumex::core::span::view::byte;
using lumex::core::span::view::span;

#if LUMEX_SPAN_CHECKS && GTEST_HAS_DEATH_TEST

namespace
{
// A cast to void does not silence warn_unused_result of GCC.
template <typename T>
void
lumex_discard (T const &)
{
}
} // namespace

TEST (LumexSpanChecksStdSpanDeathTest, GivenStdSpan_WhenSizeDiffers_ThenAborts)
{
  GTEST_FLAG_SET (death_test_style, "fast");
  std::byte raw[4] = {};
  std::span<std::byte> const four (raw, 4);
  std::span<std::byte> const three (raw, 3);
  EXPECT_EQ ((span<byte, 4> (four).size ()), 4u);
  EXPECT_DEATH (lumex_discard (span<byte, 4> (three)),
                "other.size \\(\\) == Extent.*LumexSpan\\.hpp");
  int values[4] = {};
  std::span<int> const std_three (values, 3);
  EXPECT_DEATH (lumex_discard (span<int, 4> (std_three)),
                "range.size \\(\\)\\) == Extent.*LumexSpan\\.hpp");
}

#else

TEST (LumexSpanChecksStdSpanTest, GivenChecksOff_WhenSizeDiffers_ThenNoAbort)
{
  std::byte raw[4] = {};
  std::span<std::byte> const three (raw, 3);
  EXPECT_EQ ((span<byte, 4> (three).size ()), 4u);
}

#endif

#endif
