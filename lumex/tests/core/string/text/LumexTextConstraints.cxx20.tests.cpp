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

// LumexTextConstraints.cxx20.tests.cpp
//
// At C++20 the separator of join / quote must convert to std::string_view
// (a requires-clause); below C++20 any streamable separator is viable
// (LumexTextConstraints.cxx11.tests.cpp). The C++20 suite compiles this file
// together with LumexTextConstraints.cxx11.tests.cpp. The library adds the
// requires-clauses only with std::ranges (LUMEX_HAS_STD_RANGES): libstdc++ 8
// has no <ranges> even with -std=c++2a, so there the test skips.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/string/text/LumexTextConstraintsTestHelpers.hpp"

using text_constraints_test_helpers::can_join;
using text_constraints_test_helpers::can_quote;

// --- C++20: the separator must convert to std::string_view ---

TEST (LumexTextConstraintsTest, GivenNonStringViewSeparator_ThenRejected)
{
#if LUMEX_HAS_STD_RANGES
  EXPECT_FALSE ((can_join<std::vector<int>, char>::value));
  EXPECT_FALSE ((can_join<std::vector<int>, int>::value));
  EXPECT_FALSE ((can_quote<std::vector<std::string>, char>::value));
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}
