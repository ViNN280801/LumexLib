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

// LumexQuote.cxx20.tests.cpp
// text::quote and quote_single at C++20, where they take a
// std::ranges::input_range and a separator that converts to
// std::string_view. The C++20 suite compiles this file together with
// LumexQuote.cxx11.tests.cpp. The library takes this path only when the
// standard library has std::ranges (LUMEX_HAS_STD_RANGES): libstdc++ 8 has
// no <ranges> even with -std=c++2a, so there the tests skip.
#include <string>
#include <string_view>
#include <vector>
#if defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexQuote.hpp"

using lumex::core::string::text::quote;
using lumex::core::string::text::quote_single;

TEST (LumexQuoteTest, GivenStringViewSeparator_WhenQuote_ThenUsed)
{
#if LUMEX_HAS_STD_RANGES
  std::vector<std::string> const parts = { "a", "b" };
  std::string_view const separator ("; ");
  EXPECT_EQ (quote (parts, separator), "\"a\"; \"b\"");
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}

TEST (LumexQuoteTest, GivenConstIterableView_WhenQuoteSingle_ThenWalksIt)
{
#if LUMEX_HAS_STD_RANGES
  std::vector<std::string> const parts = { "a", "b" };
  auto shouted = parts
                 | std::views::transform ([] (std::string const &text)
                                            { return text + "!"; });
  EXPECT_EQ (quote_single (shouted, ","), "'a!','b!'");
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}
