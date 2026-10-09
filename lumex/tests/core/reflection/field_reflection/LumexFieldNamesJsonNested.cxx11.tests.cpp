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

// LumexFieldNamesJsonNested.cxx11.tests.cpp
//
// The recursion of to_json into nested aggregates, from C++11: an aggregate
// field without a to_json of its own is written as a JSON object by
// field_reflection::to_json itself; a container of aggregates as an array
// (a map with string keys as an object), each element by the same rule; an
// optional-like field holding an aggregate is omitted when empty and
// otherwise written the same way. An aggregate with a to_json that ADL finds
// keeps its own: that overload has priority. Every suite of the module
// compiles this file. Built when LUMEX_WITH_FIELD_REFLECTION is ON; the macro
// gates the body as in the other field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionNestedFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace
{
// Qualified: the namespace of the fixtures has its own two-argument to_json
// overloads, which hide the one of field_reflection from an unqualified call.
nlohmann::json
dump_of (NestPath const &value)
{
  return lumex::core::reflection::field_reflection::to_json (value);
}

template <typename Struct>
std::string
text_of (Struct const &value)
{
  return lumex::core::reflection::field_reflection::to_json (value).dump ();
}

// The rule that applies to a value of the type, by the number of the kind.
template <typename T>
int
kind_of ()
{
  return detail::value_kind<T>::value;
}
} // namespace

TEST (LumexFieldNamesJsonNestedTest,
      GivenTypes_WhenKindAsked_ThenEachTypeTakesItsRule)
{
  EXPECT_EQ (kind_of<int> (), detail::k_kind_direct);
  EXPECT_EQ (kind_of<std::string> (), detail::k_kind_direct);
  EXPECT_EQ (kind_of<std::vector<int>> (), detail::k_kind_direct);
  EXPECT_EQ (kind_of<nlohmann::json> (), detail::k_kind_direct);
  // An aggregate with a to_json of its own is converted by nlohmann.
  EXPECT_EQ (kind_of<NestHooked> (), detail::k_kind_direct);
  EXPECT_EQ (kind_of<NestNoNames> (), detail::k_kind_direct);
  EXPECT_EQ (kind_of<std::vector<NestHooked>> (), detail::k_kind_direct);
  // An aggregate without one is written by to_json itself.
  EXPECT_EQ (kind_of<NestPoint> (), detail::k_kind_aggregate);
  EXPECT_EQ (kind_of<NestSegment> (), detail::k_kind_aggregate);
  EXPECT_EQ (kind_of<NestVoid> (), detail::k_kind_aggregate);
  // Containers of aggregates are written element by element.
  EXPECT_EQ (kind_of<std::vector<NestPoint>> (), detail::k_kind_array);
  EXPECT_EQ ((kind_of<std::array<NestPoint, 2>> ()), detail::k_kind_array);
  EXPECT_EQ (kind_of<std::vector<std::vector<NestPoint>>> (),
             detail::k_kind_array);
  EXPECT_EQ ((kind_of<std::map<std::string, NestPoint>> ()),
             detail::k_kind_map);
  // An optional-like value that nlohmann cannot convert.
  EXPECT_EQ (kind_of<optional<NestPoint>> (), detail::k_kind_optional);
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenAggregateInAggregate_WhenToJson_ThenNestedObject)
{
  NestSegment segment = { { 1, 2 }, { 3, 4 } };
  EXPECT_EQ (text_of (segment),
             "{\"from\":{\"x\":1,\"y\":2},\"to\":{\"x\":3,\"y\":4}}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenThreeLevels_WhenToJson_ThenEveryLevelIsAnObject)
{
  NestPath path = { "p", { { 1, 2 }, { 3, 4 } }, 7 };
  nlohmann::json const j = dump_of (path);
  EXPECT_TRUE (j["first"].is_object ());
  EXPECT_TRUE (j["first"]["from"].is_object ());
  EXPECT_EQ (j["first"]["to"]["y"].get<int> (), 4);
  EXPECT_EQ (j.dump (),
             "{\"first\":{\"from\":{\"x\":1,\"y\":2},"
             "\"to\":{\"x\":3,\"y\":4}},\"length\":7,\"name\":\"p\"}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenContainersOfAggregates_WhenToJson_ThenArraysAndObject)
{
  NestCollections value;
  NestPoint a = { 1, 2 };
  NestPoint b = { 3, 4 };
  value.list.push_back (a);
  value.list.push_back (b);
  value.pair[0] = b;
  value.pair[1] = a;
  value.named["one"] = a;
  value.named["two"] = b;
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (value);
  EXPECT_EQ (j.dump (),
             "{\"list\":[{\"x\":1,\"y\":2},{\"x\":3,\"y\":4}],"
             "\"named\":{\"one\":{\"x\":1,\"y\":2},\"two\":{\"x\":3,\"y\":4}},"
             "\"pair\":[{\"x\":3,\"y\":4},{\"x\":1,\"y\":2}]}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenEmptyContainersOfAggregates_WhenToJson_ThenEmptyArraysAndObject)
{
  NestCollections value = NestCollections ();
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (value);
  EXPECT_TRUE (j["list"].is_array ());
  EXPECT_TRUE (j["list"].empty ());
  EXPECT_TRUE (j["named"].is_object ());
  EXPECT_TRUE (j["named"].empty ());
  EXPECT_EQ (j["pair"].size (), 2u);
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenContainerOfContainers_WhenToJson_ThenArrayOfArraysOfObjects)
{
  NestMatrix value;
  NestPoint a = { 1, 2 };
  NestPoint b = { 3, 4 };
  std::vector<NestPoint> row;
  row.push_back (a);
  value.rows.push_back (row);
  row.push_back (b);
  value.rows.push_back (row);
  EXPECT_EQ (text_of (value), "{\"rows\":[[{\"x\":1,\"y\":2}],"
                              "[{\"x\":1,\"y\":2},{\"x\":3,\"y\":4}]]}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenOptionalAggregates_WhenToJson_ThenEmptyOmittedAndEngagedNested)
{
  NestOptionals value;
  value.id = 5;
  EXPECT_EQ (text_of (value), "{\"id\":5,\"slots\":[]}");

  NestPoint point = { 8, 9 };
  value.anchor = point;
  value.trail = std::vector<NestPoint> (2, point);
  value.slots.push_back (point);
  value.slots.push_back (optional<NestPoint> ());
  EXPECT_EQ (text_of (value),
             "{\"anchor\":{\"x\":8,\"y\":9},\"id\":5,"
             "\"slots\":[{\"x\":8,\"y\":9},null],"
             "\"trail\":[{\"x\":8,\"y\":9},{\"x\":8,\"y\":9}]}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenNestedAggregateWithoutFields_WhenToJson_ThenEmptyObject)
{
  NestWithVoid value = { 3, NestVoid () };
  EXPECT_EQ (text_of (value), "{\"nothing\":{},\"value\":3}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenNestedAggregateWithToJson_WhenToJson_ThenItsOwnOverloadWins)
{
  NestUsesHooked value;
  value.single.a = 1;
  value.single.b = 2;
  NestHooked item = { 3, 4 };
  value.list.push_back (item);
  value.list.push_back (item);
  // The numbers come from the overload of the fixture; the recursion would
  // have written {"a":..,"b":..}.
  EXPECT_EQ (text_of (value), "{\"list\":[7,7],\"single\":3}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenNestedAggregateWithToJsonAndNoNames_WhenToJson_ThenItNeedsNoNames)
{
  NestUsesNoNames value = { { 6, 7 } };
  EXPECT_EQ (text_of (value), "{\"pair\":[6,7]}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenTopLevelAggregateWithToJson_WhenToJson_ThenFieldsAreStillWalked)
{
  // field_reflection::to_json of the type that has the overload writes the
  // object of its fields; the overload only decides how it is written as a
  // member of another aggregate.
  NestHooked value = { 1, 2 };
  EXPECT_EQ (text_of (value), "{\"a\":1,\"b\":2}");
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
