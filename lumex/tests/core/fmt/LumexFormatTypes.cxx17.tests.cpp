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

// LumexFormatTypes.cxx17.tests.cpp
// std::string_view arguments (C++17). The C++17 and C++20 suites compile
// this file together with LumexFormatTypes.cxx11.tests.cpp.
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"

namespace fmt = lumex::core::fmt;

TEST (LumexFormatTypesTest, GivenStdStringView_WhenFormat_ThenText)
{
  EXPECT_EQ (fmt::format ("{}", std::string_view ("test")), "test");
  EXPECT_EQ (fmt::format ("{:?}", std::string_view ("t\nst")), "\"t\\nst\"");
  EXPECT_EQ (fmt::format ("{}", std::string_view ()), "");
  EXPECT_EQ (fmt::format ("{:*^8}", std::string_view ("mid")), "**mid***");
}
