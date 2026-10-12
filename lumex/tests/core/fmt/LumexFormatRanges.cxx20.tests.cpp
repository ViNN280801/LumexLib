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

// LumexFormatRanges.cxx20.tests.cpp
// LumexFormatRanges.hpp against std::format, where the standard library
// formats ranges (__cpp_lib_format_ranges, C++23 P2286; MSVC
// /std:c++latest). The C++20 suite compiles this file together with
// LumexFormatRanges.cxx11.tests.cpp; without range formatting in the
// standard library the test skips.
#include <map>
#include <string>
#include <tuple>
#include <vector>
#if defined(__has_include)
#if __has_include(<format>)
#include <format>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormatRanges.hpp"

namespace fmt = lumex::core::fmt;

TEST (LumexFormatRangesTest, GivenRanges_WhenFormat_ThenSameAsStd)
{
#if defined(__cpp_lib_format_ranges) && __cpp_lib_format_ranges >= 202207L
  std::vector<int> const values = { 1, 2, 3 };
  std::map<std::string, int> const map = { { "a", 1 } };
  std::vector<char> const chars = { 'a', '\t' };
  std::tuple<int, std::string, char> const tuple (1, "s", 'c');
  EXPECT_EQ (fmt::format ("{:*^15}|{::#x}|{:n}", values, values, values),
             std::format ("{:*^15}|{::#x}|{:n}", values, values, values));
  EXPECT_EQ (fmt::format ("{}|{:n}", map, map),
             std::format ("{}|{:n}", map, map));
  EXPECT_EQ (fmt::format ("{}|{:s}|{:?s}", chars, chars, chars),
             std::format ("{}|{:s}|{:?s}", chars, chars, chars));
  EXPECT_EQ (fmt::format ("{}|{:n}|{:>12}", tuple, tuple, tuple),
             std::format ("{}|{:n}|{:>12}", tuple, tuple, tuple));
#else
  GTEST_SKIP () << "the standard library does not format ranges";
#endif
}
