/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

/**
 * @file LumexTestNarrowSkip.hpp
 * @brief Skips a test that needs a stream to open a non-ASCII name where the
 * library cannot open one.
 * @details The Windows build whose standard library has no wide stream open
 * (MinGW before GCC 9) reads a narrow name in the ANSI code page, so
 * `utf8_file_name::open` refuses a name with a byte of 0x80 or more
 * (`narrow_name_is_faithful ()`). A test whose subject is such a name through
 * a stream cannot pass there by design; this skips it with the reason and
 * changes nothing on the other builds.
 */
#ifndef LUMEX_TESTS_SUPPORT_NARROW_SKIP_HPP
#define LUMEX_TESTS_SUPPORT_NARROW_SKIP_HPP

#include <gtest/gtest.h>

#include "lumex/core/utility/util/LumexUtf8FileName.hpp"

/// First statement of a test body that opens a Cyrillic name through a stream.
#define LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES()                           \
  if (!::lumex::core::utility::utf8_file_name::narrow_name_is_faithful (      \
          "\xD0\x9F"))                                                        \
  GTEST_SKIP () << "this build opens streams by the narrow name: the ANSI "   \
                   "code page cannot read a UTF-8 name"

#endif // !LUMEX_TESTS_SUPPORT_NARROW_SKIP_HPP
