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

// LumexFieldNamesGet.cxx14.tests.cpp
//
// From C++14 an aggregate without a registration has an automatic get<I>
// (friend-auto loophole at C++14, structured bindings from C++17), so a
// registered type and an unregistered twin of the same shape can be read side
// by side. The C++14, C++17 and C++20 suites compile this file together with
// LumexFieldNamesGet.cxx11.tests.cpp. Built when LUMEX_WITH_FIELD_REFLECTION
// is ON; the macro gates the body as in the other field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionRegisteredFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
// The shape of RegPlain and RegPadded, without a registration: get<I> is the
// automatic one.
struct TwinPlain
{
  int id;
  std::string name;
};

struct TwinPadded
{
  char a;
  int b;
  double c;
};
} // namespace lumex_field_reflection_tests

TEST (LumexFieldNamesGetTest,
      GivenRegisteredAndUnregisteredTwins_WhenGet_ThenSameValuesSameTypes)
{
  TwinPlain twin = { 7, "x" };
  RegPlain registered = { 7, "x" };
  EXPECT_EQ (get<0> (twin), get<0> (registered));
  EXPECT_EQ (get<1> (twin), get<1> (registered));
  EXPECT_TRUE ((std::is_same<decltype (get<0> (twin)),
                             decltype (get<0> (registered))>::value));
  EXPECT_TRUE ((std::is_same<decltype (get<1> (twin)),
                             decltype (get<1> (registered))>::value));

  TwinPlain const frozen_twin = { 8, "y" };
  RegPlain const frozen_registered = { 8, "y" };
  EXPECT_TRUE ((std::is_same<decltype (get<0> (frozen_twin)),
                             decltype (get<0> (frozen_registered))>::value));
  EXPECT_TRUE ((std::is_same<decltype (get<1> (frozen_twin)),
                             decltype (get<1> (frozen_registered))>::value));
}

TEST (LumexFieldNamesGetTest, GivenPaddedTwins_WhenWritten_ThenBothReadAlike)
{
  TwinPadded twin = { 'a', 1, 1.5 };
  RegPadded registered = { 'a', 1, 1.5 };
  get<1> (twin) = 5;
  get<1> (registered) = 5;
  get<2> (twin) = 2.5;
  get<2> (registered) = 2.5;
  EXPECT_EQ (get<0> (twin), get<0> (registered));
  EXPECT_EQ (get<1> (twin), get<1> (registered));
  EXPECT_DOUBLE_EQ (get<2> (twin), get<2> (registered));
  EXPECT_EQ (twin.b, registered.b);
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
