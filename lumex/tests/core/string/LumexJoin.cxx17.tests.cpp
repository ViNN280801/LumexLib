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

// LumexJoin.cxx17.tests.cpp
// text::join with a std::string_view separator on the path without
// std::ranges, where join takes any streamable separator. The C++17 and
// C++20 suites compile this file together with LumexJoin.cxx11.tests.cpp;
// with std::ranges (LUMEX_HAS_STD_RANGES) the std::string_view separator is
// tested in LumexJoin.cxx20.tests.cpp.
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexJoin.hpp"

using lumex::core::string::text::join;

#if !LUMEX_HAS_STD_RANGES

TEST (LumexJoinTest, GivenStdStringViewSeparator_WhenJoinCxx17_ThenUsed)
{
  std::vector<int> const numbers = { 1, 2 };
  EXPECT_EQ (join (numbers, std::string_view (", ")), "1, 2");
}

#endif
