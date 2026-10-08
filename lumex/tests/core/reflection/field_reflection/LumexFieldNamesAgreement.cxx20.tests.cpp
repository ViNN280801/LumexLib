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

// LumexFieldNamesAgreement.cxx20.tests.cpp
//
// At C++20 an aggregate has two sources of names: the registration
// (LUMEX_DEFINE_FIELD_NAMES) and the compiler (pointer NTTP pretty names).
// The registration wins, and the two must agree for a correct registration.
// These tests call the compiler's builder directly (detail::names_builder)
// and compare it with names_as_array, which reads the registration. The
// library declares the compiler's names only when __cplusplus is at least
// 202002L (GCC 8 reports 201709L at -std=c++2a), so the tests gate on that
// switch and skip below it. The comparison inherits the known failure of the
// compiler's names (todo item 55): where they are wrong, the agreement tests
// fail with them, and every other registered test passes. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the other
// field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <array>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionRegisteredFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

#if __cplusplus >= 202002L
namespace lumex_field_reflection_tests
{
// The names the compiler supplies, bypassing the registration.
template <typename Agg>
std::array<char const *, tuple_size<Agg>::value>
compiler_names ()
{
  return detail::names_builder<Agg,
                               typename detail::make_index_sequence<
                                   tuple_size<Agg>::value>::type>::build ();
}

template <typename Agg>
void
expect_registered_equals_compiler ()
{
  std::array<char const *, tuple_size<Agg>::value> const registered
      = names_as_array<Agg> ();
  std::array<char const *, tuple_size<Agg>::value> const compiler
      = compiler_names<Agg> ();
  ASSERT_EQ (registered.size (), compiler.size ());
  for (std::size_t i = 0; i < registered.size (); ++i)
    EXPECT_STREQ (registered[i], compiler[i])
        << "arity " << registered.size () << ", field " << i;
}

template <std::size_t N>
void
agreement_of_arity ()
{
  expect_registered_equals_compiler<typename reg_fields_of<N>::type> ();
}
} // namespace lumex_field_reflection_tests
#endif

TEST (LumexFieldNamesAgreementTest,
      GivenRegisteredPlain_WhenCompared_ThenRegistrationEqualsCompiler)
{
#if __cplusplus >= 202002L
  expect_registered_equals_compiler<RegPlain> ();
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesAgreementTest,
      GivenRegisteredFixtures_WhenCompared_ThenRegistrationEqualsCompiler)
{
#if __cplusplus >= 202002L
  expect_registered_equals_compiler<RegOneField> ();
  expect_registered_equals_compiler<RegPadded> ();
  expect_registered_equals_compiler<RegScalars> ();
  expect_registered_equals_compiler<RegUnderscoreNames> ();
  expect_registered_equals_compiler<RegWithVector> ();
  expect_registered_equals_compiler<RegWithOptional> ();
  expect_registered_equals_compiler<RegMixedOptionals> ();
  expect_registered_equals_compiler<RegPoint> ();
  expect_registered_equals_compiler<RegShape> ();
  expect_registered_equals_compiler<RegTwelveFields> ();
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesAgreementTest,
      GivenAggregatesInOtherScopes_WhenCompared_ThenRegistrationEqualsCompiler)
{
#if __cplusplus >= 202002L
  expect_registered_equals_compiler<RegHost::inner_t> ();
  expect_registered_equals_compiler<reg_inner_ns::RegInNamespace> ();
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (
    LumexFieldNamesAgreementTest,
    GivenRegisteredArityOneToThirtyTwo_WhenCompared_ThenRegistrationEqualsCompiler)
{
#if __cplusplus >= 202002L
  agreement_of_arity<1> ();
  agreement_of_arity<2> ();
  agreement_of_arity<3> ();
  agreement_of_arity<4> ();
  agreement_of_arity<5> ();
  agreement_of_arity<6> ();
  agreement_of_arity<7> ();
  agreement_of_arity<8> ();
  agreement_of_arity<9> ();
  agreement_of_arity<10> ();
  agreement_of_arity<11> ();
  agreement_of_arity<12> ();
  agreement_of_arity<13> ();
  agreement_of_arity<14> ();
  agreement_of_arity<15> ();
  agreement_of_arity<16> ();
  agreement_of_arity<17> ();
  agreement_of_arity<18> ();
  agreement_of_arity<19> ();
  agreement_of_arity<20> ();
  agreement_of_arity<21> ();
  agreement_of_arity<22> ();
  agreement_of_arity<23> ();
  agreement_of_arity<24> ();
  agreement_of_arity<25> ();
  agreement_of_arity<26> ();
  agreement_of_arity<27> ();
  agreement_of_arity<28> ();
  agreement_of_arity<29> ();
  agreement_of_arity<30> ();
  agreement_of_arity<31> ();
  agreement_of_arity<32> ();
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesAgreementTest,
      GivenSwappedRegistration_WhenNamed_ThenRegistrationWinsOverCompiler)
{
#if __cplusplus >= 202002L
  // The compiler alone would answer a, b. The registration says b, a, and it
  // is the one that names_as_array, get and to_json follow. (Whatever the
  // compiler would answer, "b" first can only come from the registration.)
  std::array<char const *, 2> const names = names_as_array<RegSwapped> ();
  EXPECT_STREQ (names[0], "b");
  EXPECT_STREQ (names[1], "a");
  EXPECT_EQ (detail::names_source<RegSwapped>::value, 0);

  RegSwapped swapped = { 1, 2 };
  EXPECT_EQ (&get<0> (swapped), &swapped.b);
  EXPECT_EQ (&get<1> (swapped), &swapped.a);

  RegSwapped const obj = { 1, 2 };
  EXPECT_EQ (to_json (obj).dump (), "{\"a\":1,\"b\":2}");
#else
  GTEST_SKIP () << "the compiler's names need __cplusplus >= 202002L";
#endif
}

TEST (LumexFieldNamesAgreementTest,
      GivenRegisteredAndUnregistered_WhenSourceAsked_ThenRegistrationFirst)
{
#if __cplusplus >= 202002L
  EXPECT_EQ (detail::names_source<RegPlain>::value, 0);
  EXPECT_EQ (detail::names_source<Plain>::value, 1);
#else
  EXPECT_EQ (detail::names_source<RegPlain>::value, 0);
  EXPECT_EQ (detail::names_source<Plain>::value, 3);
#endif
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
