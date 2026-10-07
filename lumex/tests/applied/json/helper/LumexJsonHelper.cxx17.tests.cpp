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

// LumexJsonHelper tests that need C++17: is_empty_value on std::string_view.
// The json suites of C++17 and C++20 compile this file next to
// LumexJsonHelper.cxx11.tests.cpp.

#include <string_view>

#include <gtest/gtest.h>

#include "lumex/applied/json/LumexJson"
#include "lumex/tests/applied/json/LumexJsonHelperTestFixture.hpp"

TEST_F (LumexJsonHelperTest, GivenStdStringView_WhenEmpty_ThenIsEmptyValue)
{
  EXPECT_TRUE (lumex::applied::json::helper::Detail::is_empty_value (
      std::string_view ()));
  EXPECT_FALSE (lumex::applied::json::helper::Detail::is_empty_value (
      std::string_view ("x")));
}
