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

// LumexString.cxx20.tests.cpp
// format::stringify at C++20: stringify_v2 exists, and the stream traits
// are the Streamable / AllStreamable concepts instead of the SFINAE
// structs (LumexString.cxx11.tests.cpp). The C++20 suite compiles this file
// together with LumexString.cxx11.tests.cpp. The library declares both only
// when the compiler has concepts (LUMEX_HAS_CONCEPTS); GCC 8 has none even
// with -std=c++2a. Without them TypeTraits_Dirty of the C++11 file runs
// instead: the test name is the same, so there is no skipped stand-in here.
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/string/LumexString"

#include "lumex/tests/core/string/LumexStringTestFixtures.hpp"

#if LUMEX_HAS_CONCEPTS

using lumex::core::string::utility::stringify_v2;

TEST_F (LumexStringifyTest, TypeTraits_Dirty)
{
  // C++20 path: the SFINAE structs of the C++11 file do not exist here -
  // `Streamable`/`AllStreamable` concepts cover the same purpose instead.
  EXPECT_TRUE (lumex::core::utility::traits::stream::Streamable<int>);
  EXPECT_TRUE (lumex::core::utility::traits::stream::Streamable<std::string>);
  EXPECT_TRUE (
      lumex::core::utility::traits::stream::Streamable<CustomStreamable>);
  EXPECT_FALSE (
      lumex::core::utility::traits::stream::Streamable<NonStreamable>);

  EXPECT_TRUE (
      (lumex::core::utility::traits::stream::AllStreamable<int, std::string>));
  EXPECT_TRUE (
      (lumex::core::utility::traits::stream::AllStreamable<CustomStreamable,
                                                           int>));
  EXPECT_FALSE (
      (lumex::core::utility::traits::stream::AllStreamable<NonStreamable,
                                                           int>));
  EXPECT_FALSE (
      (lumex::core::utility::traits::stream::AllStreamable<int,
                                                           NonStreamable>));

  EXPECT_TRUE (lumex::core::utility::traits::stream::AllStreamable<>);
}

#endif
