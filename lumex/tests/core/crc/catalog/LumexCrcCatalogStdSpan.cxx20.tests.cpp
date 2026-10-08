// CRC catalogue tests of std::span arguments (C++20): a std::span converts to
// the span the functions take, so the calls that compiled when the overload
// took a std::span still compile. The tests skip where the standard library
// has no std::span (libstdc++ 8 has no <span> even with -std=c++2a).

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

using namespace lumex::core::crc::catalog;

TEST (LumexCrcCatalogStdSpanTest,
      GivenStdSpan_WhenCatalogCrc_ThenSameAsThePointerForm)
{
#if LUMEX_HAS_STD_SPAN
  std::array<std::uint8_t, 9> const data
      = { { '1', '2', '3', '4', '5', '6', '7', '8', '9' } };
  for (std::uint32_t entry = 0; entry < get_crc_catalog_entry_count ();
       ++entry)
    {
      std::uint64_t const expected
          = compute_crc_catalog (entry, data.data (), data.size ());
      EXPECT_EQ (
          compute_crc_catalog (entry, std::span<std::uint8_t const> (data)),
          expected)
          << "catalog index " << entry;
      EXPECT_EQ (
          compute_crc_catalog (entry, std::span<std::uint8_t const, 9> (data)),
          expected)
          << "static std::span, catalog index " << entry;
    }
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST (LumexCrcCatalogStdSpanTest,
      GivenMutableStdSpan_WhenRevEngCrc_ThenCheckValue)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<std::uint8_t> data
      = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  crc_params_t params;
  params.widthBits = 32;
  params.poly = 0x04C11DB7ULL;
  params.init = 0xFFFFFFFFULL;
  params.refIn = true;
  params.refOut = true;
  params.xorOut = 0xFFFFFFFFULL;
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (params, std::span<std::uint8_t> (data)),
      0xCBF43926ULL);
  EXPECT_EQ (compute_crc_with_rev_eng_params (
                 params, std::span<std::uint8_t> (data).subspan (0, 3)),
             compute_crc_with_rev_eng_params (params, data.data (), 3));
  EXPECT_EQ (compute_crc_with_rev_eng_params (
                 params, std::span<std::uint8_t const> ()),
             compute_crc_with_rev_eng_params (params, nullptr, 0));
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}
