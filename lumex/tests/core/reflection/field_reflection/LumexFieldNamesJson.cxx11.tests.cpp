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

// LumexFieldNamesJson.cxx11.tests.cpp
//
// to_json of registered aggregates, from C++11 (nlohmann needs C++11):
// strings, numbers, vectors, optionals, nested aggregates. Every suite of the
// module compiles this file. Built when LUMEX_WITH_FIELD_REFLECTION is ON; the
// macro gates the body as in the other field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <array>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionRegisteredFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
// The registered aggregate of N members: the first and the last key are
// there, with the sample values, and no other key.
template <std::size_t N>
void
expect_arity_json ()
{
  typedef typename reg_fields_of<N>::type agg_t;
  typedef typename field_type_at<0>::type first_t;
  typedef typename field_type_at<N - 1>::type last_t;
  agg_t obj = agg_t ();
  get<0> (obj) = make_sample<first_t> (0);
  get<N - 1> (obj) = make_sample<last_t> (N - 1);
  // Qualified: this namespace has its own to_json for RegPoint, which hides
  // the one of field_reflection.
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (obj);
  EXPECT_EQ (j.size (), N);
  std::string const last_key = "f" + std::to_string (N - 1);
  ASSERT_TRUE (j.contains ("f0")) << N;
  ASSERT_TRUE (j.contains (last_key)) << N;
  expect_sample_eq (j.at ("f0").template get<first_t> (),
                    make_sample<first_t> (0));
  expect_sample_eq (j.at (last_key).template get<last_t> (),
                    make_sample<last_t> (N - 1));
}
} // namespace lumex_field_reflection_tests

TEST (LumexFieldNamesJsonTest, GivenEmptyAggregate_WhenToJson_ThenEmptyObject)
{
  nlohmann::json const j = to_json (Empty ());
  EXPECT_TRUE (j.is_object ());
  EXPECT_TRUE (j.empty ());
}

TEST (LumexFieldNamesJsonTest, GivenOneField_WhenToJson_ThenSingleKey)
{
  RegOneField const obj = { 5 };
  nlohmann::json const j = to_json (obj);
  ASSERT_TRUE (j.contains ("only"));
  EXPECT_EQ (j["only"].get<int> (), 5);
  EXPECT_EQ (j.size (), 1u);
}

TEST (LumexFieldNamesJsonTest,
      GivenRegisteredPlain_WhenToJson_ThenExactDocument)
{
  RegPlain const obj = { 42, "hello" };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j.dump (), "{\"id\":42,\"name\":\"hello\"}");
}

TEST (LumexFieldNamesJsonTest,
      GivenScalars_WhenToJson_ThenTypesAndEmptyStringKept)
{
  RegScalars const obj = { true, 1.5, "" };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j["flag"].get<bool> (), true);
  EXPECT_DOUBLE_EQ (j["ratio"].get<double> (), 1.5);
  EXPECT_EQ (j["empty"].get<std::string> (), "");
  EXPECT_EQ (j.size (), 3u);
}

TEST (LumexFieldNamesJsonTest, GivenContainerField_WhenToJson_ThenJsonArray)
{
  RegWithVector obj;
  obj.values.push_back (1);
  obj.values.push_back (2);
  obj.values.push_back (3);
  nlohmann::json const j = to_json (obj);
  ASSERT_TRUE (j.contains ("values"));
  EXPECT_TRUE (j["values"].is_array ());
  EXPECT_EQ (j["values"].size (), 3u);
  EXPECT_EQ (j["values"][2].get<int> (), 3);
}

TEST (LumexFieldNamesJsonTest, GivenOptionalWithValue_WhenToJson_ThenWritten)
{
  RegWithOptional obj;
  obj.id = 1;
  obj.label = std::string ("present");
  nlohmann::json const j = to_json (obj);
  ASSERT_TRUE (j.contains ("label"));
  EXPECT_EQ (j["label"].get<std::string> (), "present");
}

TEST (LumexFieldNamesJsonTest,
      GivenOptionalWithoutValue_WhenToJson_ThenOmitted)
{
  RegWithOptional obj;
  obj.id = 2;
  obj.label = nullopt;
  nlohmann::json const j = to_json (obj);
  EXPECT_TRUE (j.contains ("id"));
  EXPECT_FALSE (j.contains ("label"));
}

TEST (LumexFieldNamesJsonTest,
      GivenMixedOptionals_WhenToJson_ThenOnlyEngagedOnes)
{
  RegMixedOptionals obj;
  obj.missing = nullopt;
  obj.always = 9;
  obj.present = 4;
  nlohmann::json const j = to_json (obj);
  EXPECT_FALSE (j.contains ("missing"));
  EXPECT_EQ (j["always"].get<int> (), 9);
  EXPECT_EQ (j["present"].get<int> (), 4);
  EXPECT_EQ (j.size (), 2u);
}

TEST (LumexFieldNamesJsonTest,
      GivenNestedAggregates_WhenToJson_ThenNestedDocument)
{
  RegShape shape;
  shape.title = "box";
  shape.origin.x = 5;
  shape.origin.y = 6;
  RegPoint corner = { 1, 2 };
  shape.corners.push_back (corner);
  corner.x = 3;
  corner.y = 4;
  shape.corners.push_back (corner);
  shape.scale = 1.5;
  nlohmann::json const j = to_json (shape);
  EXPECT_EQ (j.dump (), "{\"corners\":[{\"x\":1,\"y\":2},{\"x\":3,\"y\":4}],"
                        "\"origin\":{\"x\":5,\"y\":6},\"scale\":1.5,"
                        "\"title\":\"box\"}");
}

TEST (LumexFieldNamesJsonTest, GivenTwelveFields_WhenToJson_ThenEveryKey)
{
  RegTwelveFields const obj = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j.size (), 12u);
  EXPECT_EQ (j["a0"].get<int> (), 0);
  EXPECT_EQ (j["a11"].get<int> (), 11);
}

TEST (LumexFieldNamesJsonTest, GivenPadded_WhenToJson_ThenAllKeys)
{
  RegPadded const obj = { 'q', 4, 8.0 };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j["a"].get<char> (), 'q');
  EXPECT_EQ (j["b"].get<int> (), 4);
  EXPECT_DOUBLE_EQ (j["c"].get<double> (), 8.0);
}

TEST (LumexFieldNamesJsonTest,
      GivenUnderscoreNames_WhenToJson_ThenKeysMatchIdentifiers)
{
  RegUnderscoreNames const obj = { 1, 2 };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j["field_a"].get<int> (), 1);
  EXPECT_EQ (j["field_b"].get<int> (), 2);
}

TEST (LumexFieldNamesJsonTest, GivenAggregateNestedInClass_WhenToJson_ThenKeys)
{
  RegHost::inner_t const obj = { 4, "dev" };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j.dump (), "{\"device\":4,\"label\":\"dev\"}");
}

TEST (LumexFieldNamesJsonTest, GivenGlobalAndHiddenTypes_WhenToJson_ThenKeys)
{
  RegGlobalType const global = { 5, 0.5 };
  EXPECT_EQ (to_json (global).dump (), "{\"g0\":5,\"g1\":0.5}");
  RegHiddenType const hidden = { 3, true };
  EXPECT_EQ (to_json (hidden).dump (), "{\"flag\":true,\"v\":3}");
}

TEST (LumexFieldNamesJsonTest,
      GivenRegistrationInOtherOrder_WhenToJson_ThenNamesBelongToTheirValues)
{
  std::array<char const *, 2> const names = names_as_array<RegSwapped> ();
  EXPECT_STREQ (names[0], "b");
  EXPECT_STREQ (names[1], "a");
  RegSwapped const obj = { 1, 2 };
  EXPECT_EQ (to_json (obj).dump (), "{\"a\":1,\"b\":2}");
}

TEST (LumexFieldNamesJsonTest,
      GivenRegisteredArityOfSelectedSizes_WhenToJson_ThenFirstAndLastKey)
{
  expect_arity_json<1> ();
  expect_arity_json<2> ();
  expect_arity_json<3> ();
  expect_arity_json<12> ();
  expect_arity_json<17> ();
  expect_arity_json<32> ();
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
