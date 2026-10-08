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
// format::stringify at C++20: the stream traits are the Streamable /
// AllStreamable concepts instead of the SFINAE structs
// (LumexString.cxx11.tests.cpp), and the constraint of stringify_v2
// (LumexStringifyV2.cxx11.tests.cpp), the condition of stringify_v2, is the
// concept AllStringifiable, and the SFINAE trait all_streamable agrees with it
// on the same argument lists (except for wchar_t, below). The C++20
// suite compiles this file together with LumexString.cxx11.tests.cpp. The
// library declares the concepts only when the compiler has them
// (LUMEX_HAS_CONCEPTS); GCC 8 has none even with -std=c++2a. Without them
// TypeTraits_Dirty of the C++11 file runs instead: the test name is the same,
// so there is no skipped stand-in here.
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string/LumexString"

#include "lumex/tests/core/string/LumexStringTestFixtures.hpp"

#if LUMEX_HAS_CONCEPTS

namespace stream_traits = lumex::core::utility::traits::stream;

namespace
{
// stringify_v2 (declval<Args> ()...) is well-formed.
template <typename... Args>
concept accepts_stringify_v2 = requires {
  lumex::core::string::utility::stringify_v2 (std::declval<Args> ()...);
};

// The condition of the constraint, the concept of stringify, the SFINAE trait
// all_streamable and the call itself all give `expected`.
template <bool expected, typename... Args>
bool
constraint_is ()
{
  bool const gate = lumex::core::string::utility::detail::are_stringifiable<
      Args...>::value;
  bool const concept_answer = stream_traits::detail::AllStringifiable<Args...>;
  bool const trait = stream_traits::all_streamable<Args...>::value;
  bool const call = accepts_stringify_v2<Args...>;
  return gate == expected && concept_answer == expected && trait == expected
         && call == expected;
}
} // namespace

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

TEST (
    LumexStringifyV2ConceptTest,
    GivenWideChar_WhenConstraintCompared_ThenTheCallFollowsTheStandardLibrary)
{
  // all_streamable names wchar_t streamable (an explicit specialization of
  // is_streamable), but from C++20 the standard library deletes
  // `operator<<` of a narrow stream for it: the concept, and so the call of
  // stringify_v2 and of stringify, follow the library.
  EXPECT_TRUE (stream_traits::all_streamable<wchar_t>::value);
  EXPECT_EQ (stream_traits::detail::AllStringifiable<wchar_t>,
             stream_traits::is_ostreamable<wchar_t>::value);
  EXPECT_EQ (accepts_stringify_v2<wchar_t>,
             stream_traits::detail::AllStringifiable<wchar_t>);
  EXPECT_EQ (accepts_stringify_v2<wchar_t const &>,
             stream_traits::is_ostreamable<wchar_t>::value);
}

TEST (LumexStringifyV2ConceptTest,
      GivenArgumentLists_WhenConstraintCompared_ThenTraitConceptAndCallAgree)
{
  EXPECT_TRUE ((constraint_is<true> ()));
  EXPECT_TRUE ((constraint_is<true, int> ()));
  EXPECT_TRUE ((constraint_is<true, int &> ()));
  EXPECT_TRUE ((constraint_is<true, int const &> ()));
  EXPECT_TRUE (
      (constraint_is<true, double, float, long long, unsigned char> ()));
  EXPECT_TRUE ((constraint_is<true, bool, char, signed char, short> ()));
  EXPECT_TRUE ((constraint_is<true, std::string> ()));
  EXPECT_TRUE ((constraint_is<true, std::string &, std::string const &> ()));
  EXPECT_TRUE ((constraint_is<true, char const *> ()));
  EXPECT_TRUE ((constraint_is<true, char *> ()));
  EXPECT_TRUE ((constraint_is<true, char const (&)[8]> ()));
  EXPECT_TRUE ((constraint_is<true, char (&)[8]> ()));
  EXPECT_TRUE ((constraint_is<true, void *> ()));
  EXPECT_TRUE ((constraint_is<true, int *> ()));
  EXPECT_TRUE ((constraint_is<true, std::nullptr_t> ()));
  EXPECT_TRUE ((constraint_is<true, CustomStreamable> ()));
  EXPECT_TRUE ((constraint_is<true, CustomStreamable const &, int> ()));
  EXPECT_TRUE ((constraint_is<true, std::unique_ptr<int>> ()));
  EXPECT_TRUE ((constraint_is<true, std::unique_ptr<int> const &> ()));
  EXPECT_TRUE ((constraint_is<true, std::unique_ptr<int[]>> ()));
  EXPECT_TRUE ((constraint_is<true, std::shared_ptr<int>> ()));
  EXPECT_TRUE ((constraint_is<true, std::shared_ptr<std::string> &> ()));
  EXPECT_TRUE ((constraint_is<false, std::weak_ptr<int>> ()));
  EXPECT_TRUE ((constraint_is<false, NonStreamable> ()));
  EXPECT_TRUE ((constraint_is<false, NonStreamable &> ()));
  EXPECT_TRUE ((constraint_is<false, int, NonStreamable> ()));
  EXPECT_TRUE ((constraint_is<false, NonStreamable, int> ()));
  EXPECT_TRUE (
      (constraint_is<false, int, std::string, NonStreamable, double> ()));
  EXPECT_TRUE ((constraint_is<false, std::vector<int>> ()));
  EXPECT_TRUE ((constraint_is<false, std::vector<int> &, int> ()));
}

#endif
