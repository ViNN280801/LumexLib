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
// lumex_base_exception tests of std::string_view arguments (C++17): they
// convert to the lumex_string_view the constructor takes. The
// C++17 and C++20 suites compile this file together with the .cxx11 files of
// this directory.

#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/exceptions/LumexException"
#include "lumex/core/string_view/LumexStringView"

#include "lumex/tests/core/exceptions/exception/LumexExceptionTestFixtures.hpp"

using lumex::core::exceptions::exception::lumex_base_exception;

static_assert (
    std::is_constructible<lumex_base_exception, std::string_view>::value,
    "a std::string_view is accepted (it converts to lumex_string_view)");
static_assert (std::is_constructible<lumex_base_exception,
                                     std::string_view const &>::value,
               "a const lvalue std::string_view is accepted");
static_assert (std::is_convertible<std::string_view, lumex_string_view>::value,
               "the conversion is implicit");

// API Contract Verifier: the std::string_view argument (converted to the
// lumex_string_view the inline constructor takes, which wraps the exported
// std::string one) copies exactly the view
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

// The view of the library is the type the constructor takes; the char const *,
// std::string and literal constructors keep their calls.
TEST_F (LumexExceptionTest, LumexBaseException_LumexStringView_CopiesTheView)
{
  std::string const text = "prefix:message:suffix";
  lumex_string_view const view = lumex_string_view (text).substr (7, 7);
  lumex_base_exception const ex (view);
  EXPECT_STREQ (ex.what (), "message");
  lumex_base_exception const empty (lumex_string_view{});
  EXPECT_STREQ (empty.what (), "");
  std::string const zeros ("a\0b", 3);
  lumex_base_exception const with_nul{ lumex_string_view (zeros) };
  EXPECT_EQ (std::string (with_nul.what (), 3), zeros);
  EXPECT_STREQ (lumex_base_exception ("literal").what (), "literal");
  EXPECT_STREQ (lumex_base_exception (text).what (), text.c_str ());
}
