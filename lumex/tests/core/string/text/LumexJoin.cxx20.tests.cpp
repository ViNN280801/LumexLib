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

// LumexJoin.cxx20.tests.cpp
// text::join at C++20, where it takes a std::ranges::input_range and a
// separator that converts to std::string_view. The C++20 suite compiles this
// file together with the .cxx11 and .cxx17 files. The library takes this
// path only when the standard library has std::ranges (LUMEX_HAS_STD_RANGES):
// libstdc++ 8 has no <ranges> even with -std=c++2a, so there the tests skip
// and the .cxx11 / .cxx17 cases of the other path run instead.
#include <string_view>
#include <vector>
#if defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexJoin.hpp"

using lumex::core::string::text::join;

TEST (LumexJoinTest, GivenStringViewSeparator_WhenJoin_ThenUsed)
{
#if LUMEX_HAS_STD_RANGES
  std::vector<int> const numbers = { 1, 2 };
  std::string_view const separator (", ");
  EXPECT_EQ (join (numbers, separator), "1, 2");
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}

TEST (LumexJoinTest, GivenConstIterableView_WhenJoin_ThenWalksIt)
{
#if LUMEX_HAS_STD_RANGES
  EXPECT_EQ (join (std::views::iota (1, 6), " "), "1 2 3 4 5");
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}

// The range is taken as `input_range auto const &`, so a view that is not
// iterable through const (std::views::filter) is rejected at the call.
TEST (LumexJoinTest, GivenFilterView_WhenJoin_ThenNotViableThroughConst)
{
#if LUMEX_HAS_STD_RANGES
  auto evens = std::views::iota (1, 11)
               | std::views::filter ([] (int n) { return n % 2 == 0; });
  EXPECT_FALSE (std::ranges::input_range<decltype (evens) const>);
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}

TEST (LumexJoinTest, GivenTransformedView_WhenJoin_ThenTransformedText)
{
#if LUMEX_HAS_STD_RANGES
  std::vector<int> const numbers = { 1, 2, 3 };
  EXPECT_EQ (
      join (numbers | std::views::transform ([] (int n) { return n * n; }),
            "+"),
      "1+4+9");
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}
