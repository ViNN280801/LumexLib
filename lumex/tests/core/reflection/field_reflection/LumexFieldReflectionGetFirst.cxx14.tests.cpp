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

// LumexFieldReflectionGetFirst.cxx14.tests.cpp
//
// get<I> as the first thing a translation unit does with an aggregate. Below
// C++17 get<I> takes the field types from the friend-injection loophole;
// they were once injected only by tuple_size<T>, so a get<I> before any
// tuple_size<T> of the type did not compile. Nothing in this file names
// tuple_size of an aggregate before its get<I> calls, and its aggregates are
// used nowhere else. The C++14, C++17 and C++20 suites compile this file.
// Built when LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as
// in the other field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

using namespace lumex::core::reflection::field_reflection;

namespace
{
struct get_first_t
{
  int id;
  double ratio;
  char tag;
};

struct get_first_const_t
{
  long long big;
  short small;
};

struct get_first_wide_t
{
  int f0;
  char f1;
  double f2;
  bool f3;
  unsigned f4;
  float f5;
  short f6;
  long long f7;
  int f8;
};

struct get_first_string_t
{
  std::string name;
  int count;
};
} // namespace

TEST (LumexAggregateFieldsTest,
      GivenNoTupleSizeBefore_WhenGet_ThenFieldValuesAndWritable)
{
  get_first_t obj{ 7, 2.5, 'x' };
  EXPECT_EQ (get<0> (obj), 7);
  EXPECT_DOUBLE_EQ (get<1> (obj), 2.5);
  EXPECT_EQ (get<2> (obj), 'x');
  get<0> (obj) = 8;
  EXPECT_EQ (obj.id, 8);
}

TEST (LumexAggregateFieldsTest,
      GivenNoTupleSizeBefore_WhenGetOnConst_ThenConstFieldValues)
{
  get_first_const_t const obj{ 1LL << 40, 3 };
  EXPECT_TRUE ((std::is_same<decltype (get<1> (obj)), short const &>::value));
  EXPECT_EQ (get<0> (obj), 1LL << 40);
  EXPECT_EQ (get<1> (obj), 3);
}

TEST (LumexAggregateFieldsTest,
      GivenNoTupleSizeBefore_WhenGetLastOfNine_ThenValue)
{
  get_first_wide_t obj{ 1, 'b', 3.5, true, 5u, 6.5f, 7, 8LL, 9 };
  EXPECT_EQ (get<8> (obj), 9);
  EXPECT_EQ (get<7> (obj), 8LL);
  EXPECT_FLOAT_EQ (get<5> (obj), 6.5f);
  EXPECT_EQ (get<1> (obj), 'b');
}

TEST (LumexAggregateFieldsTest,
      GivenNoTupleSizeBefore_WhenGetString_ThenSameObject)
{
  get_first_string_t obj{ "name", 2 };
  get<0> (obj) += "d";
  EXPECT_EQ (obj.name, "named");
  EXPECT_EQ (&get<0> (obj), &obj.name);
  EXPECT_EQ (get<1> (obj), 2);
}

// tuple_size after get<I> still counts the same fields.
TEST (LumexAggregateFieldsTest, GivenGetBefore_WhenTupleSize_ThenFieldCount)
{
  EXPECT_EQ (tuple_size<get_first_t>::value, 3u);
  EXPECT_EQ (tuple_size<get_first_const_t>::value, 2u);
  EXPECT_EQ (tuple_size<get_first_wide_t>::value, 9u);
  EXPECT_EQ (tuple_size<get_first_string_t>::value, 2u);
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
