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

// LumexFieldNamesGet.cxx11.tests.cpp
//
// Indexed get<I> of registered aggregates, from C++11. Every suite of the
// module compiles this file: at C++11 get<I> is the registered one; from
// C++14 a registered aggregate still reads through its registration, which
// wins over the automatic get. Built when LUMEX_WITH_FIELD_REFLECTION is ON;
// the macro gates the body as in the other field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionRegisteredFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
// The members of a standard-layout class lie at increasing addresses in
// declaration order, so get<0>, get<1>, ... of a correct registration have
// strictly increasing addresses: a source of the order that does not use the
// registration. (A permutation of distinct members with increasing addresses
// is the identity.)
template <std::size_t I, typename Agg>
std::uintptr_t
address (Agg &obj)
{
  return reinterpret_cast<std::uintptr_t> (&get<I> (obj));
}

// The first and the last member of the registered aggregate of N members: the
// type of the sample cycle, the value written and read back, the order.
template <std::size_t N>
void
expect_arity_ends ()
{
  typedef typename reg_fields_of<N>::type agg_t;
  typedef typename field_type_at<0>::type first_t;
  typedef typename field_type_at<N - 1>::type last_t;
  agg_t obj = agg_t ();
  get<0> (obj) = make_sample<first_t> (0);
  get<N - 1> (obj) = make_sample<last_t> (N - 1);
  agg_t const &frozen = obj;
  EXPECT_TRUE ((std::is_same<decltype (get<0> (obj)), first_t &>::value));
  EXPECT_TRUE (
      (std::is_same<decltype (get<N - 1> (frozen)), last_t const &>::value));
  expect_sample_eq (get<0> (frozen), make_sample<first_t> (0));
  expect_sample_eq (get<N - 1> (frozen), make_sample<last_t> (N - 1));
  EXPECT_LE (address<0> (obj), address<N - 1> (obj)) << N;
}
} // namespace lumex_field_reflection_tests

TEST (LumexFieldNamesGetTest, GivenRegisteredPlain_WhenGet_ThenFieldValues)
{
  RegPlain obj = { 7, "x" };
  EXPECT_EQ (get<0> (obj), 7);
  EXPECT_EQ (get<1> (obj), "x");
  get<0> (obj) = 8;
  get<1> (obj) += "y";
  EXPECT_EQ (obj.id, 8);
  EXPECT_EQ (obj.name, "xy");
}

TEST (LumexFieldNamesGetTest,
      GivenConstRegisteredPlain_WhenGet_ThenFieldValues)
{
  RegPlain const obj = { 3, "c" };
  EXPECT_EQ (get<0> (obj), 3);
  EXPECT_EQ (get<1> (obj), "c");
}

TEST (LumexFieldNamesGetTest,
      GivenRegisteredPlain_WhenGet_ThenSameObjectAsMember)
{
  RegPlain obj = { 1, "a" };
  EXPECT_EQ (&get<0> (obj), &obj.id);
  EXPECT_EQ (&get<1> (obj), &obj.name);
  RegPlain const &frozen = obj;
  EXPECT_EQ (&get<0> (frozen), &obj.id);
  EXPECT_EQ (&get<1> (frozen), &obj.name);
}

TEST (LumexFieldNamesGetTest, GivenRegisteredPlain_WhenGet_ThenReferenceTypes)
{
  RegPlain obj = { 1, "a" };
  RegPlain const frozen = { 2, "b" };
  EXPECT_TRUE ((std::is_same<decltype (get<0> (obj)), int &>::value));
  EXPECT_TRUE ((std::is_same<decltype (get<1> (obj)), std::string &>::value));
  EXPECT_TRUE ((std::is_same<decltype (get<0> (frozen)), int const &>::value));
  EXPECT_TRUE (
      (std::is_same<decltype (get<1> (frozen)), std::string const &>::value));
}

TEST (LumexFieldNamesGetTest, GivenRegisteredPadded_WhenGet_ThenAllFields)
{
  RegPadded obj = { 'z', 11, 2.5 };
  EXPECT_EQ (get<0> (obj), 'z');
  EXPECT_EQ (get<1> (obj), 11);
  EXPECT_DOUBLE_EQ (get<2> (obj), 2.5);
  get<1> (obj) = 12;
  EXPECT_EQ (obj.b, 12);
  EXPECT_EQ (obj.a, 'z');
  EXPECT_DOUBLE_EQ (obj.c, 2.5);
}

TEST (LumexFieldNamesGetTest,
      GivenTwelveRegisteredFields_WhenGet_ThenEachIndex)
{
  RegTwelveFields obj = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  EXPECT_EQ (get<0> (obj), 0);
  EXPECT_EQ (get<3> (obj), 3);
  EXPECT_EQ (get<7> (obj), 7);
  EXPECT_EQ (get<11> (obj), 11);
}

TEST (LumexFieldNamesGetTest, GivenNestedAggregate_WhenGet_ThenWritesThrough)
{
  RegShape shape = { "box", { 5, 6 }, { { 1, 2 }, { 3, 4 } }, 1.5 };
  EXPECT_EQ (get<0> (shape), "box");
  EXPECT_EQ (get<1> (shape).x, 5);
  get<1> (shape).y = 9;
  EXPECT_EQ (shape.origin.y, 9);
  EXPECT_EQ (get<2> (shape).size (), 2u);
  EXPECT_DOUBLE_EQ (get<3> (shape), 1.5);
}

TEST (LumexFieldNamesGetTest, GivenAggregateNestedInClass_WhenGet_ThenFields)
{
  RegHost::inner_t obj = { 4, "dev" };
  EXPECT_EQ (get<0> (obj), 4);
  EXPECT_EQ (get<1> (obj), "dev");
  EXPECT_EQ (&get<1> (obj), &obj.label);
}

TEST (LumexFieldNamesGetTest, GivenGlobalAndHiddenTypes_WhenGet_ThenFields)
{
  RegGlobalType global = { 5, 0.5 };
  EXPECT_EQ (get<0> (global), 5);
  EXPECT_DOUBLE_EQ (get<1> (global), 0.5);
  RegHiddenType hidden = { 1LL << 40, true };
  EXPECT_EQ (get<0> (hidden), 1LL << 40);
  EXPECT_TRUE (get<1> (hidden));
}

TEST (LumexFieldNamesGetTest,
      GivenClassTemplateInstanceThroughTypedef_WhenGet_ThenFields)
{
  reg_pair_t pair = { 3, 2.5 };
  EXPECT_EQ (get<0> (pair), 3);
  EXPECT_DOUBLE_EQ (get<1> (pair), 2.5);
}

TEST (LumexFieldNamesGetTest, GivenMixedOptionals_WhenGet_ThenOptionalsSurvive)
{
  RegMixedOptionals obj;
  obj.missing = nullopt;
  obj.always = 9;
  obj.present = 4;
  EXPECT_FALSE (get<0> (obj).has_value ());
  EXPECT_EQ (get<1> (obj), 9);
  ASSERT_TRUE (get<2> (obj).has_value ());
  EXPECT_EQ (*get<2> (obj), 4);
}

TEST (LumexFieldNamesGetTest,
      GivenRegistrationInOtherOrder_WhenGet_ThenRegisteredMemberIsRead)
{
  // The registration lists b, a: its first member is b, in every standard
  // (from C++14 the registration wins over the automatic get).
  RegSwapped obj = { 1, 2 };
  EXPECT_EQ (&get<0> (obj), &obj.b);
  EXPECT_EQ (&get<1> (obj), &obj.a);
  EXPECT_EQ (get<0> (obj), 2);
  EXPECT_EQ (get<1> (obj), 1);
  RegSwapped const frozen = { 3, 4 };
  EXPECT_EQ (&get<0> (frozen), &frozen.b);
  EXPECT_EQ (&get<1> (frozen), &frozen.a);
}

TEST (LumexFieldNamesOrderTest,
      GivenRegisteredFixtures_WhenGet_ThenRegisteredOrderIsDeclaredOrder)
{
  RegPlain plain = RegPlain ();
  EXPECT_LT (address<0> (plain), address<1> (plain));
  RegPadded padded = RegPadded ();
  EXPECT_LT (address<0> (padded), address<1> (padded));
  EXPECT_LT (address<1> (padded), address<2> (padded));
  RegScalars scalars = RegScalars ();
  EXPECT_LT (address<0> (scalars), address<1> (scalars));
  EXPECT_LT (address<1> (scalars), address<2> (scalars));
  RegUnderscoreNames underscore = RegUnderscoreNames ();
  EXPECT_LT (address<0> (underscore), address<1> (underscore));
  RegShape shape = RegShape ();
  EXPECT_LT (address<0> (shape), address<1> (shape));
  EXPECT_LT (address<1> (shape), address<2> (shape));
  EXPECT_LT (address<2> (shape), address<3> (shape));
  RegHost::inner_t inner = RegHost::inner_t ();
  EXPECT_LT (address<0> (inner), address<1> (inner));
  reg_inner_ns::RegInNamespace in_namespace = reg_inner_ns::RegInNamespace ();
  EXPECT_LT (address<0> (in_namespace), address<1> (in_namespace));
  reg_pair_t pair = reg_pair_t ();
  EXPECT_LT (address<0> (pair), address<1> (pair));
  RegGlobalType global = RegGlobalType ();
  EXPECT_LT (address<0> (global), address<1> (global));
  RegHiddenType hidden = RegHiddenType ();
  EXPECT_LT (address<0> (hidden), address<1> (hidden));
  RegTwelveFields twelve = RegTwelveFields ();
  EXPECT_LT (address<0> (twelve), address<1> (twelve));
  EXPECT_LT (address<1> (twelve), address<2> (twelve));
  EXPECT_LT (address<2> (twelve), address<3> (twelve));
  EXPECT_LT (address<3> (twelve), address<4> (twelve));
  EXPECT_LT (address<4> (twelve), address<5> (twelve));
  EXPECT_LT (address<5> (twelve), address<6> (twelve));
  EXPECT_LT (address<6> (twelve), address<7> (twelve));
  EXPECT_LT (address<7> (twelve), address<8> (twelve));
  EXPECT_LT (address<8> (twelve), address<9> (twelve));
  EXPECT_LT (address<9> (twelve), address<10> (twelve));
  EXPECT_LT (address<10> (twelve), address<11> (twelve));
}

TEST (LumexFieldNamesGetTest,
      GivenRegisteredArityOfSelectedSizes_WhenGetFirstAndLast_ThenSampleValues)
{
  expect_arity_ends<1> ();
  expect_arity_ends<2> ();
  expect_arity_ends<3> ();
  expect_arity_ends<12> ();
  expect_arity_ends<16> ();
  expect_arity_ends<17> ();
  expect_arity_ends<31> ();
  expect_arity_ends<32> ();
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
