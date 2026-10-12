// CRC catalogue tests of the span overloads (every standard the module
// builds at): compute_crc_catalog and compute_crc_with_rev_eng_params take
// lumex::core::span::view::span<std::uint8_t const>, so a program of any
// standard passes contiguous bytes the way a C++20 one passes a std::span
// (LumexCrcCatalogStdSpan.cxx20.tests.cpp).

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::span;
using namespace lumex::core::crc::catalog;

namespace
{
// "123456789": the check input of the CRC catalogue.
std::array<std::uint8_t, 9> const check_input
    = { { '1', '2', '3', '4', '5', '6', '7', '8', '9' } };

// CRC-32/ISO-HDLC in RevEng notation, check value 0xCBF43926.
crc_params_t
crc32_iso_hdlc ()
{
  crc_params_t params;
  params.widthBits = 32;
  params.poly = 0x04C11DB7ULL;
  params.init = 0xFFFFFFFFULL;
  params.refIn = true;
  params.refOut = true;
  params.xorOut = 0xFFFFFFFFULL;
  return params;
}
} // namespace

TEST (LumexCrcCatalogSpanTest,
      GivenEveryCatalogIndex_WhenSpan_ThenSameAsThePointerForm)
{
  std::vector<std::uint8_t> payload (300);
  for (std::size_t index = 0; index < payload.size (); ++index)
    {
      payload[index] = static_cast<std::uint8_t> (index * 7u + 3u);
    }
  std::uint32_t const count = get_crc_catalog_entry_count ();
  ASSERT_GT (count, 100u);
  for (std::uint32_t entry = 0; entry < count; ++entry)
    {
      span<std::uint8_t const> const whole (payload);
      EXPECT_EQ (compute_crc_catalog (entry, whole),
                 compute_crc_catalog (entry, payload.data (), payload.size ()))
          << "catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (entry, whole.subspan (10, 100)),
                 compute_crc_catalog (entry, payload.data () + 10, 100))
          << "catalog index " << entry;
    }
}

TEST (LumexCrcCatalogSpanTest, GivenContainers_WhenCatalogCrc_ThenOneResult)
{
  std::vector<std::uint8_t> const as_vector (check_input.begin (),
                                             check_input.end ());
  std::uint8_t built_in[9];
  for (std::size_t index = 0; index < 9; ++index)
    {
      built_in[index] = check_input[index];
    }
  for (std::uint32_t entry = 0; entry < get_crc_catalog_entry_count ();
       ++entry)
    {
      std::uint64_t const expected = compute_crc_catalog (entry, as_vector);
      EXPECT_EQ (compute_crc_catalog (entry, check_input), expected)
          << "std::array, catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (entry, built_in), expected)
          << "built-in array, catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (
                     entry, span<std::uint8_t const, 9> (check_input.data (),
                                                         check_input.size ())),
                 expected)
          << "static span, catalog index " << entry;
    }
}

TEST (LumexCrcCatalogSpanTest,
      GivenEmptySpan_WhenCatalogCrc_ThenSameAsEmptyBuffer)
{
  for (std::uint32_t entry = 0; entry < get_crc_catalog_entry_count ();
       ++entry)
    {
      EXPECT_EQ (compute_crc_catalog (entry, span<std::uint8_t const> ()),
                 compute_crc_catalog (entry, nullptr, 0))
          << "catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (entry, std::vector<std::uint8_t> ()),
                 compute_crc_catalog (entry, span<std::uint8_t const> ()))
          << "catalog index " << entry;
    }
}

TEST (LumexCrcCatalogSpanTest, GivenRevEngParams_WhenSpan_ThenCheckValue)
{
  crc_params_t const params = crc32_iso_hdlc ();
  EXPECT_EQ (compute_crc_with_rev_eng_params (params, check_input),
             0xCBF43926ULL);
  EXPECT_EQ (compute_crc_with_rev_eng_params (
                 params, span<std::uint8_t const> (check_input.data (), 9)),
             0xCBF43926ULL);
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (
          params, span<std::uint8_t const> (check_input).subspan (0, 3)),
      compute_crc_with_rev_eng_params (params, check_input.data (), 3));
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (params, span<std::uint8_t const> ()),
      compute_crc_with_rev_eng_params (params, nullptr, 0));
}

TEST (LumexCrcCatalogSpanTest,
      GivenMutableBytes_WhenSpanOverload_ThenReadOnlyView)
{
  std::vector<std::uint8_t> bytes (check_input.begin (), check_input.end ());
  span<std::uint8_t> const mutable_view (bytes);
  EXPECT_EQ (compute_crc_with_rev_eng_params (crc32_iso_hdlc (), mutable_view),
             0xCBF43926ULL);
  EXPECT_EQ (compute_crc_catalog (0, mutable_view),
             compute_crc_catalog (0, bytes));
}
