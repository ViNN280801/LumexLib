// CRC tests of the span overload of crc_parametric (every standard the module
// builds at): calculate takes lumex::core::span::view::span<std::uint8_t
// const>, so a program of any standard passes contiguous bytes the way a C++20
// one passes a std::span (LumexCrcParametric.cxx20.tests.cpp).

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::span;
using namespace lumex::core::crc::parametric;

namespace
{
std::array<std::uint8_t, 9> const check_input
    = { { '1', '2', '3', '4', '5', '6', '7', '8', '9' } };
} // namespace

TEST (LumexCrcParametricSpanTest,
      GivenCheckInput_WhenSpan_ThenCatalogueCheckValues)
{
  span<std::uint8_t const> const view (check_input);
  EXPECT_EQ (Crc32IsoHdlc::calculate (view), 0xCBF43926u);
  EXPECT_EQ (Crc16Arc::calculate (view), 0xBB3Du);
  EXPECT_EQ (Crc8Smbus::calculate (view), 0xF4u);
  EXPECT_EQ (Crc64Xz::calculate (view), 0x995DC9BBDF1939FAULL);
  EXPECT_EQ (Crc16Xmodem::calculate (view), 0x31C3u);
}

TEST (LumexCrcParametricSpanTest, GivenContainers_WhenCalculate_ThenOneResult)
{
  std::vector<std::uint8_t> const as_vector (check_input.begin (),
                                             check_input.end ());
  std::uint8_t built_in[9];
  for (std::size_t index = 0; index < 9; ++index)
    {
      built_in[index] = check_input[index];
    }
  std::uint32_t const expected = Crc32IsoHdlc::calculate (as_vector);
  EXPECT_EQ (expected, 0xCBF43926u);
  EXPECT_EQ (Crc32IsoHdlc::calculate (check_input), expected);
  EXPECT_EQ (Crc32IsoHdlc::calculate (built_in), expected);
  EXPECT_EQ (Crc32IsoHdlc::calculate (
                 span<std::uint8_t const, 9> (check_input.data (), 9)),
             expected);
  EXPECT_EQ (Crc32IsoHdlc::calculate (check_input.data (), 9), expected);
}

TEST (LumexCrcParametricSpanTest,
      GivenSubviews_WhenCalculate_ThenOnlyTheSubviewCounts)
{
  span<std::uint8_t const> const view (check_input);
  EXPECT_EQ (Crc32IsoHdlc::calculate (view.first (4)),
             Crc32IsoHdlc::calculate (check_input.data (), 4));
  EXPECT_EQ (Crc32IsoHdlc::calculate (view.last (4)),
             Crc32IsoHdlc::calculate (check_input.data () + 5, 4));
  EXPECT_EQ (Crc32IsoHdlc::calculate (view.subspan (2, 5)),
             Crc32IsoHdlc::calculate (check_input.data () + 2, 5));
  EXPECT_EQ (Crc32IsoHdlc::calculate (view.first<3> ()),
             Crc32IsoHdlc::calculate (check_input.data (), 3));
}

TEST (LumexCrcParametricSpanTest,
      GivenEmptySpan_WhenCalculate_ThenSameAsEmptyBuffer)
{
  EXPECT_EQ (Crc32IsoHdlc::calculate (span<std::uint8_t const> ()),
             Crc32IsoHdlc::calculate (nullptr, 0));
  EXPECT_EQ (Crc32IsoHdlc::calculate (span<std::uint8_t const, 0> ()),
             Crc32IsoHdlc::calculate (nullptr, 0));
  EXPECT_EQ (Crc32IsoHdlc::calculate (std::array<std::uint8_t, 0> ()),
             Crc32IsoHdlc::calculate (std::vector<std::uint8_t> ()));
  EXPECT_EQ (Crc8Smbus::calculate (span<std::uint8_t const> ()), 0u);
}

TEST (LumexCrcParametricSpanTest,
      GivenMutableBytes_WhenCalculate_ThenReadOnlyView)
{
  std::vector<std::uint8_t> bytes (check_input.begin (), check_input.end ());
  span<std::uint8_t> const mutable_view (bytes);
  EXPECT_EQ (Crc32IsoHdlc::calculate (mutable_view), 0xCBF43926u);
  static_assert (
      std::is_convertible<span<std::uint8_t>, span<std::uint8_t const>>::value,
      "a span of mutable bytes converts to the argument type");
}
