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

// LumexFieldNamesJsonNested.cxx20.tests.cpp
//
// The recursion of to_json into nested aggregates whose names the compiler
// supplies (C++20): no registration anywhere, or a registration on one level
// and the compiler's names on the next. The library has the compiler's names
// only when __cplusplus is at least 202002L (GCC 8 reports 201709L at
// -std=c++2a), so the tests skip below it. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the other
// field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionNestedFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
// Nothing of these is registered.
struct AutoInner
{
  int a;
  std::string b;
};

struct AutoOuter
{
  AutoInner inner;
  std::vector<AutoInner> list;
  int c;
};

// A registered aggregate that holds an unregistered one, and the reverse.
struct AutoInRegistered
{
  NestPoint point;
  AutoInner inner;
};
LUMEX_DEFINE_FIELD_NAMES (AutoInRegistered, point, inner);

struct AutoHoldsRegistered
{
  AutoInner inner;
  NestPoint point;
};
} // namespace lumex_field_reflection_tests

TEST (LumexFieldNamesJsonNestedTest,
      GivenNothingRegistered_WhenToJson_ThenCompilerNamesAtEveryLevel)
{
#if __cplusplus >= 202002L
  AutoOuter value;
  value.inner.a = 1;
  value.inner.b = "x";
  AutoInner item = { 2, "y" };
  value.list.push_back (item);
  value.c = 3;
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (value);
  EXPECT_EQ (j.dump (), "{\"c\":3,\"inner\":{\"a\":1,\"b\":\"x\"},"
                        "\"list\":[{\"a\":2,\"b\":\"y\"}]}");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenRegisteredHoldingUnregistered_WhenToJson_ThenEachUsesItsSource)
{
#if __cplusplus >= 202002L
  AutoInRegistered value = { { 1, 2 }, { 3, "z" } };
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (value);
  EXPECT_EQ (j.dump (),
             "{\"inner\":{\"a\":3,\"b\":\"z\"},\"point\":{\"x\":1,\"y\":2}}");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenUnregisteredHoldingRegistered_WhenToJson_ThenEachUsesItsSource)
{
#if __cplusplus >= 202002L
  AutoHoldsRegistered value = { { 3, "z" }, { 1, 2 } };
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (value);
  EXPECT_EQ (j.dump (),
             "{\"inner\":{\"a\":3,\"b\":\"z\"},\"point\":{\"x\":1,\"y\":2}}");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
