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

// CRC tests of the std::span overload of crc_parametric (C++20). The C++20
// suite compiles this file together with LumexCrcParametric.cxx14.tests.cpp.
// The overload exists only when the standard
// library has std::span (LUMEX_HAS_STD_SPAN): libstdc++ 8 has no <span> even
// with -std=c++2a, so there the tests skip.

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
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

using namespace lumex::core::crc::catalog;
using namespace lumex::core::crc::parametric;

namespace
{
using byte = std::uint8_t;
} // namespace

TEST (Crc8Span, MatchesVector)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<byte> data = { 0x01, 0x02, 0x03, 0x04 };
  std::span<byte const> sp (data.data (), data.size ());
  EXPECT_EQ (Crc8MaximDow::calculate (sp), Crc8MaximDow::calculate (data));
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST (Crc8Span, EmptySpan_ReturnsZero)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<byte> empty;
  std::span<byte const> sp (empty.data (), static_cast<std::size_t> (0));
  EXPECT_EQ (Crc8MaximDow::calculate (sp), 0);
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST (Crc8Span, StdSpanOfAnyExtentAndConstness_ConvertsToTheOverload)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<byte> data = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  std::span<byte> const mutable_span (data);
  std::span<byte const, 9> const static_span (data.data (), 9);
  EXPECT_EQ (Crc32IsoHdlc::calculate (mutable_span), 0xCBF43926u);
  EXPECT_EQ (Crc32IsoHdlc::calculate (static_span), 0xCBF43926u);
  EXPECT_EQ (Crc32IsoHdlc::calculate (static_span.subspan<2, 3> ()),
             Crc32IsoHdlc::calculate (data.data () + 2, 3));
  EXPECT_EQ (Crc32IsoHdlc::calculate (mutable_span.first (0)),
             Crc32IsoHdlc::calculate (nullptr, 0));
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}
