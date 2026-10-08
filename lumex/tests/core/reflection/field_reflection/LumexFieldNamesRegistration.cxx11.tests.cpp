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

// LumexFieldNamesRegistration.cxx11.tests.cpp
//
// The registration (LUMEX_DEFINE_FIELD_NAMES) from C++11: which names
// names_as_array gives, for which kinds of scope, how the source of the names
// is chosen, and the preprocessor part (up to 32 names). Every suite of the
// module compiles this file. The aggregates are in
// LumexFieldReflectionRegisteredFixtures.hpp. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the other
// field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <array>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionRegisteredFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
// The registered aggregate of N members names them f0..f<N-1>.
template <std::size_t N>
void
expect_arity_names ()
{
  typedef typename reg_fields_of<N>::type agg_t;
  std::array<char const *, N> const names = names_as_array<agg_t> ();
  EXPECT_EQ (tuple_size<agg_t>::value, N);
  for (std::size_t i = 0; i < N; ++i)
    EXPECT_EQ (std::string (names[i]), "f" + std::to_string (i))
        << N << " " << i;
}
} // namespace lumex_field_reflection_tests

// The count of the names, at compile time, from 1 to 33 (33 is the count that
// selects the macro that does not compile).
static_assert (LUMEX_FIELD_NAMES_COUNT (f0) == 1, "count of 1 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1) == 2, "count of 2 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2) == 3, "count of 3 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3) == 4,
               "count of 4 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4) == 5,
               "count of 5 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5) == 6,
               "count of 6 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6) == 7,
               "count of 7 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7) == 8,
               "count of 8 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8)
                   == 9,
               "count of 9 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9)
                   == 10,
               "count of 10 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10)
                   == 11,
               "count of 11 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11)
                   == 12,
               "count of 12 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12)
                   == 13,
               "count of 13 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13)
                   == 14,
               "count of 14 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14)
                   == 15,
               "count of 15 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15)
                   == 16,
               "count of 16 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16)
                   == 17,
               "count of 17 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17)
                   == 18,
               "count of 18 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18)
                   == 19,
               "count of 19 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19)
                   == 20,
               "count of 20 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20)
                   == 21,
               "count of 21 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21)
                   == 22,
               "count of 22 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22)
                   == 23,
               "count of 23 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23)
                   == 24,
               "count of 24 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24)
                   == 25,
               "count of 25 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25)
                   == 26,
               "count of 26 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25,
                                        f26)
                   == 27,
               "count of 27 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25,
                                        f26, f27)
                   == 28,
               "count of 28 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25,
                                        f26, f27, f28)
                   == 29,
               "count of 29 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25,
                                        f26, f27, f28, f29)
                   == 30,
               "count of 30 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25,
                                        f26, f27, f28, f29, f30)
                   == 31,
               "count of 31 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25,
                                        f26, f27, f28, f29, f30, f31)
                   == 32,
               "count of 32 names");
static_assert (LUMEX_FIELD_NAMES_COUNT (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9,
                                        f10, f11, f12, f13, f14, f15, f16, f17,
                                        f18, f19, f20, f21, f22, f23, f24, f25,
                                        f26, f27, f28, f29, f30, f31, f32)
                   == 33,
               "count of 33 names");

TEST (LumexFieldNamesRegistrationTest,
      GivenRegisteredPlain_WhenNamed_ThenExactFieldNames)
{
  std::array<char const *, 2> const names = names_as_array<RegPlain> ();
  ASSERT_EQ (names.size (), 2u);
  EXPECT_STREQ (names[0], "id");
  EXPECT_STREQ (names[1], "name");
  EXPECT_EQ (tuple_size<RegPlain>::value, names.size ());
}

TEST (LumexFieldNamesRegistrationTest,
      GivenRegisteredOneField_WhenNamed_ThenSingleName)
{
  std::array<char const *, 1> const names = names_as_array<RegOneField> ();
  EXPECT_STREQ (names[0], "only");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenRegisteredPadded_WhenNamed_ThenNamesInDeclarationOrder)
{
  std::array<char const *, 3> const names = names_as_array<RegPadded> ();
  EXPECT_STREQ (names[0], "a");
  EXPECT_STREQ (names[1], "b");
  EXPECT_STREQ (names[2], "c");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenUnderscoreNames_WhenNamed_ThenIdentifiersAreKept)
{
  std::array<char const *, 2> const names
      = names_as_array<RegUnderscoreNames> ();
  EXPECT_STREQ (names[0], "field_a");
  EXPECT_STREQ (names[1], "field_b");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenTwelveRegisteredFields_WhenNamed_ThenAllTwelveNames)
{
  std::array<char const *, 12> const names
      = names_as_array<RegTwelveFields> ();
  for (std::size_t i = 0; i < names.size (); ++i)
    EXPECT_EQ (std::string (names[i]), "a" + std::to_string (i)) << i;
}

TEST (LumexFieldNamesRegistrationTest,
      GivenAggregateNestedInClass_WhenNamed_ThenRegisteredNames)
{
  std::array<char const *, 2> const names
      = names_as_array<RegHost::inner_t> ();
  EXPECT_STREQ (names[0], "device");
  EXPECT_STREQ (names[1], "label");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenAggregateInInnerNamespace_WhenNamed_ThenRegisteredNames)
{
  std::array<char const *, 2> const names
      = names_as_array<reg_inner_ns::RegInNamespace> ();
  EXPECT_STREQ (names[0], "big");
  EXPECT_STREQ (names[1], "flag");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenAggregateInGlobalNamespace_WhenNamed_ThenRegisteredNames)
{
  std::array<char const *, 2> const names = names_as_array<RegGlobalType> ();
  EXPECT_STREQ (names[0], "g0");
  EXPECT_STREQ (names[1], "g1");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenAggregateInUnnamedNamespace_WhenNamed_ThenRegisteredNames)
{
  std::array<char const *, 2> const names = names_as_array<RegHiddenType> ();
  EXPECT_STREQ (names[0], "v");
  EXPECT_STREQ (names[1], "flag");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenClassTemplateInstanceThroughTypedef_WhenNamed_ThenRegisteredNames)
{
  std::array<char const *, 2> const names = names_as_array<reg_pair_t> ();
  EXPECT_STREQ (names[0], "first");
  EXPECT_STREQ (names[1], "second");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenConstQualifiedType_WhenNamed_ThenSameNames)
{
  std::array<char const *, 2> const names = names_as_array<RegPlain const> ();
  EXPECT_STREQ (names[0], "id");
  EXPECT_STREQ (names[1], "name");
  EXPECT_EQ (tuple_size<RegPlain const>::value, 2u);
}

TEST (LumexFieldNamesRegistrationTest,
      GivenRepeatedCalls_WhenNamed_ThenSameStringsLiveOn)
{
  std::array<char const *, 2> const first = names_as_array<RegPlain> ();
  std::array<char const *, 2> const second = names_as_array<RegPlain> ();
  EXPECT_EQ (first[0], second[0]);
  EXPECT_EQ (first[1], second[1]);
  (void)names_as_array<RegPadded> ();
  EXPECT_STREQ (first[0], "id");
  EXPECT_STREQ (first[1], "name");
}

TEST (LumexFieldNamesRegistrationTest,
      GivenRegisteredPlain_WhenNamesSearched_ThenFoundOnlyTheRealOnes)
{
  std::array<char const *, 2> const names = names_as_array<RegPlain> ();
  bool found_id = false;
  bool found_unknown = false;
  for (std::size_t i = 0; i < names.size (); ++i)
    {
      if (std::string (names[i]) == "id")
        found_id = true;
      if (std::string (names[i]) == "not_a_field")
        found_unknown = true;
    }
  EXPECT_TRUE (found_id);
  EXPECT_FALSE (found_unknown);
}

TEST (LumexFieldNamesRegistrationTest,
      GivenAggregateWithoutFields_WhenNamedWithoutRegistration_ThenEmptyArray)
{
  std::array<char const *, 0> const names = names_as_array<Empty> ();
  EXPECT_TRUE (names.empty ());
  EXPECT_EQ (tuple_size<Empty>::value, 0u);
}

TEST (
    LumexFieldNamesRegistrationTest,
    GivenRegistrationAndNone_WhenSourceAsked_ThenRegisteredOrAutomaticOrMissing)
{
  // The source of the names: 0 the registration, 1 the compiler (C++20),
  // 2 no fields, 3 missing (a static_assert in names_as_array).
  EXPECT_TRUE (detail::registry_of<RegPlain>::value);
  EXPECT_FALSE (detail::registry_of<Plain>::value);
  EXPECT_FALSE (detail::registry_of<Empty>::value);
  EXPECT_EQ (detail::names_source<RegPlain>::value, 0);
  EXPECT_EQ (detail::names_source<Plain>::value,
             detail::k_automatic_names ? 1 : 3);
  EXPECT_EQ (detail::names_source<Empty>::value,
             detail::k_automatic_names ? 1 : 2);
}

TEST (
    LumexFieldNamesRegistrationTest,
    GivenRegisteredTypes_WhenCountedAutomatically_ThenCountMatchesRegistration)
{
  EXPECT_EQ (tuple_size<RegShape>::value, 4u);
  EXPECT_EQ (tuple_size<RegMixedOptionals>::value, 3u);
  EXPECT_EQ (tuple_size<RegWithVector>::value, 1u);
  EXPECT_EQ (tuple_size<RegHost::inner_t>::value, 2u);
  EXPECT_EQ (tuple_size<reg_pair_t>::value, 2u);
  EXPECT_EQ (tuple_size<RegTwelveFields>::value, 12u);
}

TEST (LumexFieldNamesRegistrationTest,
      GivenEverySizeFromOneToThirtyTwo_WhenNamed_ThenFIndexNames)
{
  expect_arity_names<1> ();
  expect_arity_names<2> ();
  expect_arity_names<3> ();
  expect_arity_names<4> ();
  expect_arity_names<5> ();
  expect_arity_names<6> ();
  expect_arity_names<7> ();
  expect_arity_names<8> ();
  expect_arity_names<9> ();
  expect_arity_names<10> ();
  expect_arity_names<11> ();
  expect_arity_names<12> ();
  expect_arity_names<13> ();
  expect_arity_names<14> ();
  expect_arity_names<15> ();
  expect_arity_names<16> ();
  expect_arity_names<17> ();
  expect_arity_names<18> ();
  expect_arity_names<19> ();
  expect_arity_names<20> ();
  expect_arity_names<21> ();
  expect_arity_names<22> ();
  expect_arity_names<23> ();
  expect_arity_names<24> ();
  expect_arity_names<25> ();
  expect_arity_names<26> ();
  expect_arity_names<27> ();
  expect_arity_names<28> ();
  expect_arity_names<29> ();
  expect_arity_names<30> ();
  expect_arity_names<31> ();
  expect_arity_names<32> ();
}

TEST (LumexFieldNamesRegistrationTest,
      GivenEveryNameCountFromOneToThirtyTwo_WhenExpanded_ThenAsManyStrings)
{
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_1_t, f0) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 1u);
    EXPECT_STREQ (strings[0], "f0");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_2_t, f0, f1) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 2u);
    EXPECT_STREQ (strings[1], "f1");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_3_t, f0, f1, f2) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 3u);
    EXPECT_STREQ (strings[2], "f2");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_4_t, f0, f1, f2, f3) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 4u);
    EXPECT_STREQ (strings[3], "f3");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_5_t, f0, f1, f2, f3, f4) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 5u);
    EXPECT_STREQ (strings[4], "f4");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_6_t, f0, f1, f2, f3, f4, f5) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 6u);
    EXPECT_STREQ (strings[5], "f5");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_7_t, f0, f1, f2, f3, f4, f5,
        f6) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 7u);
    EXPECT_STREQ (strings[6], "f6");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_8_t, f0, f1, f2, f3, f4, f5, f6,
        f7) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 8u);
    EXPECT_STREQ (strings[7], "f7");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_9_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 9u);
    EXPECT_STREQ (strings[8], "f8");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_10_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 10u);
    EXPECT_STREQ (strings[9], "f9");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_11_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 11u);
    EXPECT_STREQ (strings[10], "f10");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_12_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 12u);
    EXPECT_STREQ (strings[11], "f11");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_13_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 13u);
    EXPECT_STREQ (strings[12], "f12");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_14_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 14u);
    EXPECT_STREQ (strings[13], "f13");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_15_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 15u);
    EXPECT_STREQ (strings[14], "f14");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_16_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 16u);
    EXPECT_STREQ (strings[15], "f15");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_17_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 17u);
    EXPECT_STREQ (strings[16], "f16");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_18_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 18u);
    EXPECT_STREQ (strings[17], "f17");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_19_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 19u);
    EXPECT_STREQ (strings[18], "f18");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_20_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 20u);
    EXPECT_STREQ (strings[19], "f19");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_21_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 21u);
    EXPECT_STREQ (strings[20], "f20");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_22_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20,
        f21) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 22u);
    EXPECT_STREQ (strings[21], "f21");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_23_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 23u);
    EXPECT_STREQ (strings[22], "f22");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_24_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 24u);
    EXPECT_STREQ (strings[23], "f23");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_25_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 25u);
    EXPECT_STREQ (strings[24], "f24");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_26_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 26u);
    EXPECT_STREQ (strings[25], "f25");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_27_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 27u);
    EXPECT_STREQ (strings[26], "f26");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_28_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26, f27) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 28u);
    EXPECT_STREQ (strings[27], "f27");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_29_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26, f27, f28) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 29u);
    EXPECT_STREQ (strings[28], "f28");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_30_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26, f27, f28, f29) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 30u);
    EXPECT_STREQ (strings[29], "f29");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_31_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26, f27, f28, f29, f30) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 31u);
    EXPECT_STREQ (strings[30], "f30");
  }
  {
    char const *const strings[] = { LUMEX_FIELD_NAMES_FOR_EACH (
        LUMEX_FIELD_NAMES_STRING, reg_fields_32_t, f0, f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26, f27, f28, f29, f30, f31) };
    EXPECT_EQ (sizeof (strings) / sizeof (strings[0]), 32u);
    EXPECT_STREQ (strings[31], "f31");
  }
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
