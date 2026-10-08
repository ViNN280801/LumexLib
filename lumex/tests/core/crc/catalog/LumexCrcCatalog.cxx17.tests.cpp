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

// CRC tests of the std::string_view arguments of the catalogue (C++17):
// compute_crc_catalog and compute_crc_with_rev_eng_params. The C++17 and
// C++20 suites compile this file together with LumexCrcCatalog.cxx11.tests.cpp
// and LumexCrcCatalogStringView.cxx11.tests.cpp (the string overloads in every
// standard, and append_crc_least_significant_byte_first).

#include <cstdint>
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/string_view/LumexStringView"

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

TEST (CrcCatalog, LumexStringView_WhenCatalogCrc_ThenSameAsStdStringView)
{
  // The view of the library converts to std::string_view, so it is accepted
  // next to the std::string, char const * and span overloads.
  lumex_string_view const view ("123456789");
  EXPECT_EQ (compute_crc_catalog (0, view),
             static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
  EXPECT_EQ (compute_crc_catalog (0, view),
             compute_crc_catalog (0, kCheckMessage));
  EXPECT_EQ (compute_crc_catalog (0, view.substr (0, 4)),
             compute_crc_catalog (0, kCheckMessage.substr (0, 4)));
  EXPECT_EQ (compute_crc_catalog (0, lumex_string_view ()), 0U);
  crc_params_t const params = params_of<crc8_maxim_dow_spec_t> ();
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (params, view),
      static_cast<std::uint64_t> (crc8_maxim_dow_spec_t::kCatalogCheck));
  EXPECT_EQ (compute_crc_with_rev_eng_params (params, lumex_string_view ()),
             0U);
}
