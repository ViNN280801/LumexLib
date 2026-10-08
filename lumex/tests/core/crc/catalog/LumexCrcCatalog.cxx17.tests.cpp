/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// CRC tests of the std::string_view overloads of the catalogue (C++17):
// compute_crc_catalog, compute_crc_with_rev_eng_params and
// append_crc_least_significant_byte_first. The C++17 and C++20 suites compile
// this file together with LumexCrcCatalog.cxx11.tests.cpp.

#include <cstdint>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"

using namespace lumex::core::crc::catalog;
using namespace lumex::core::crc::parametric;

namespace
{
// The RevEng check message: every catalogue entry documents its CRC of it.
std::string_view const kCheckMessage ("123456789");

template <typename Spec>
crc_params_t
params_of ()
{
  crc_params_t params{};
  params.widthBits = Spec::kWidth;
  params.poly = Spec::kPoly;
  params.init = Spec::kInit;
  params.refIn = Spec::kRefIn;
  params.refOut = Spec::kRefOut;
  params.xorOut = Spec::kXorOut;
  return params;
}

std::uint8_t const *
bytes_of (std::string_view text)
{
  return reinterpret_cast<std::uint8_t const *> (text.data ());
}
} // namespace

TEST (CrcCatalog, ComputeCrcCatalogStringView_WhenFound_ThenMatchesPointerForm)
{
  EXPECT_EQ (compute_crc_catalog (0, kCheckMessage),
             static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
  EXPECT_EQ (compute_crc_catalog (0, kCheckMessage),
             compute_crc_catalog (0, bytes_of (kCheckMessage),
                                  kCheckMessage.size ()));
}

TEST (CrcCatalog, ComputeCrcCatalogStringView_WhenUnfound_ThenZero)
{
  EXPECT_EQ (compute_crc_catalog (0, std::string_view ()), 0U);
  EXPECT_EQ (
      compute_crc_catalog (get_crc_catalog_entry_count (), kCheckMessage), 0U);
}

TEST (CrcCatalog, RevEngParamsStringView_WhenFound_ThenMatchesPointerForm)
{
  crc_params_t const params = params_of<crc8_maxim_dow_spec_t> ();
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (params, kCheckMessage),
      static_cast<std::uint64_t> (crc8_maxim_dow_spec_t::kCatalogCheck));
  EXPECT_EQ (compute_crc_with_rev_eng_params (params, kCheckMessage),
             compute_crc_with_rev_eng_params (params, bytes_of (kCheckMessage),
                                              kCheckMessage.size ()));
}

TEST (CrcCatalog, RevEngParamsStringView_WhenEmpty_ThenZero)
{
  crc_params_t const params = params_of<crc8_maxim_dow_spec_t> ();
  EXPECT_EQ (compute_crc_with_rev_eng_params (params, std::string_view ()),
             0U);
}

TEST (CrcCatalog, AppendCrcLeastSignificantByteFirst_AppendsTheCheckBytes)
{
  std::vector<std::uint8_t> frame8 (bytes_of (kCheckMessage),
                                    bytes_of (kCheckMessage)
                                        + kCheckMessage.size ());
  append_crc_least_significant_byte_first (params_of<crc8_maxim_dow_spec_t> (),
                                           frame8);
  ASSERT_EQ (frame8.size (), kCheckMessage.size () + 1U);
  EXPECT_EQ (frame8.back (), crc8_maxim_dow_spec_t::kCatalogCheck);

  std::vector<std::uint8_t> frame16 (bytes_of (kCheckMessage),
                                     bytes_of (kCheckMessage)
                                         + kCheckMessage.size ());
  append_crc_least_significant_byte_first (params_of<crc16_modbus_spec_t> (),
                                           frame16);
  ASSERT_EQ (frame16.size (), kCheckMessage.size () + 2U);
  EXPECT_EQ (
      frame16[kCheckMessage.size ()],
      static_cast<std::uint8_t> (crc16_modbus_spec_t::kCatalogCheck & 0xFFU));
  EXPECT_EQ (
      frame16[kCheckMessage.size () + 1U],
      static_cast<std::uint8_t> (crc16_modbus_spec_t::kCatalogCheck >> 8));
}
