// LumexSpanChecksBytes.cxx17.tests.cpp
//
// The precondition check of the explicit conversion from a dynamic span of
// the other byte type (std::byte) to a span of byte with a static extent.
#include <cstddef>

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

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

TEST (LumexSpanChecksBytesDeathTest,
      GivenStdByteSpan_WhenSizeDiffers_ThenAborts)
{
  GTEST_FLAG_SET (death_test_style, "fast");
  std::byte raw[4] = {};
  span<std::byte> const four (raw, 4);
  span<std::byte> const three (raw, 3);
  EXPECT_EQ ((span<byte, 4> (four).size ()), 4u);
  LUMEX_SPAN_EXPECT_ABORT (lumex_discard (span<byte, 4> (three)),
                           "other.size \\(\\) == Extent.*LumexSpan\\.hpp");
}

#else

TEST (LumexSpanChecksBytesTest, GivenChecksOff_WhenSizeDiffers_ThenNoAbort)
{
  std::byte raw[4] = {};
  span<std::byte> const three (raw, 3);
  EXPECT_EQ ((span<byte, 4> (three).size ()), 4u);
}

#endif
