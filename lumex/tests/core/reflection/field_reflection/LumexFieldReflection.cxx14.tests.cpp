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

// LumexFieldReflection.cxx14.tests.cpp
//
// Indexed get<I> on aggregates (C++14: the friend-auto loophole, or
// structured bindings from C++17). The C++14, C++17 and C++20 suites compile
// this file together with LumexFieldReflection.cxx11.tests.cpp. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the
// C++11 file.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/LumexFieldReflectionTestFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

TEST (LumexAggregateFieldsTest, GivenPlainAggregate_WhenGet_ThenFieldValues)
{
  EXPECT_EQ (tuple_size<Plain>::value, 2u);
  EXPECT_EQ (tuple_size<OneField>::value, 1u);
  Plain obj{ 7, "x" };
  EXPECT_EQ (get<0> (obj), 7);
  EXPECT_EQ (get<1> (obj), "x");
  get<0> (obj) = 8;
  EXPECT_EQ (obj.id, 8);
}

TEST (LumexAggregateFieldsTest,
      GivenConstPlainAggregate_WhenGet_ThenFieldValues)
{
  Plain const obj{ 3, "c" };
  EXPECT_EQ (get<0> (obj), 3);
  EXPECT_EQ (get<1> (obj), "c");
}

TEST (LumexAggregateFieldsTest, GivenPaddedAggregate_WhenGet_ThenAllFields)
{
  Padded obj{ 'z', 11, 2.5 };
  EXPECT_EQ (get<0> (obj), 'z');
  EXPECT_EQ (get<1> (obj), 11);
  EXPECT_DOUBLE_EQ (get<2> (obj), 2.5);
  get<1> (obj) = 12;
  EXPECT_EQ (obj.b, 12);
  EXPECT_EQ (obj.a, 'z');
  EXPECT_DOUBLE_EQ (obj.c, 2.5);
}

TEST (LumexAggregateFieldsTest, GivenEightFields_WhenGet_ThenEachIndex)
{
  EightFields obj{ 0, 1, 2, 3, 4, 5, 6, 7 };
  EXPECT_EQ (get<0> (obj), 0);
  EXPECT_EQ (get<3> (obj), 3);
  EXPECT_EQ (get<7> (obj), 7);
}

TYPED_TEST (LumexFieldArityTest, GivenArity_WhenGetEachIndex_ThenSampleValues)
{
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  agg_t obj{};
  fill_fields (obj, std::make_index_sequence<tag_t::value>{});
  check_fields (obj, std::make_index_sequence<tag_t::value>{});

  agg_t const frozen = obj;
  check_fields (frozen, std::make_index_sequence<tag_t::value>{});
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
