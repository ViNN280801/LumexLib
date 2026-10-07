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

// LumexException.cxx17.tests.cpp
//
// lumex_base_exception tests of the std::string_view constructor (C++17). The
// C++17 and C++20 suites compile this file together with the .cxx11 files of
// this directory.

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/exceptions/LumexException"

#include "lumex/tests/core/exceptions/exception/LumexExceptionTestFixtures.hpp"

using lumex::core::exceptions::exception::lumex_base_exception;

// API Contract Verifier: the std::string_view constructor (an inline wrapper
// over the exported std::string one) copies exactly the view
TEST_F (LumexExceptionTest, LumexBaseException_StringViewCtor_CopiesTheView)
{
  std::string const text = "prefix:message:suffix";
  std::string_view const view = std::string_view (text).substr (7, 7);
  lumex_base_exception const ex (view);
  EXPECT_STREQ (ex.what (), "message");
}

TEST_F (LumexExceptionTest,
        LumexBaseException_StringViewCtor_EmptyAndEmbeddedNulKept)
{
  std::string_view const empty_view;
  lumex_base_exception const empty (empty_view);
  EXPECT_STREQ (empty.what (), "");

  std::string_view const with_nul ("a\0b", 3);
  lumex_base_exception const ex (with_nul);
  EXPECT_EQ (std::string (ex.what (), 3), std::string (with_nul));
}
