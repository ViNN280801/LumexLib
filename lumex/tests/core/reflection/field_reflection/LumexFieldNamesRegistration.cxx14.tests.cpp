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

// LumexFieldNamesRegistration.cxx14.tests.cpp
//
// What differs for registered aggregates from C++14: tuple_size_v exists. The
// C++14, C++17 and C++20 suites compile this file together with
// LumexFieldNamesRegistration.cxx11.tests.cpp. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the other
// field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <cstddef>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionRegisteredFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

TEST (LumexFieldNamesRegistrationTest,
      GivenRegisteredTypes_WhenTupleSizeVariable_ThenCount)
{
  EXPECT_EQ (tuple_size_v<RegPlain>, 2u);
  EXPECT_EQ (tuple_size_v<RegShape>, 4u);
  EXPECT_EQ (tuple_size_v<RegHost::inner_t>, 2u);
  EXPECT_EQ (tuple_size_v<RegPlain>, names_as_array<RegPlain> ().size ());
}

TEST (
    LumexFieldNamesRegistrationTest,
    GivenRegisteredAndUnregistered_WhenMemberUsable_ThenOnlyRegisteredInRange)
{
  EXPECT_TRUE ((detail::registered_usable<RegPlain, 0>::value));
  EXPECT_TRUE ((detail::registered_usable<RegPlain, 1>::value));
  EXPECT_FALSE ((detail::registered_usable<RegPlain, 2>::value));
  EXPECT_FALSE ((detail::registered_usable<Plain, 0>::value));
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
