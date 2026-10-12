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

// LumexFieldNamesJsonNested.cxx17.tests.cpp
//
// The recursion of to_json into nested aggregates through std::optional and
// std::unordered_map (the C++17 additions to the cases of the C++11 file).
// nlohmann converts a std::optional of a type it can convert (null when
// empty) but not one of an aggregate; to_json then takes over: as a field an
// empty std::optional is omitted, as an element of a container it is null,
// and an engaged one holding an aggregate is written as the object. The C++17
// and C++20 suites compile this file. Built when LUMEX_WITH_FIELD_REFLECTION
// is ON; the macro gates the body as in the other field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionNestedFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
struct NestStdOptionals
{
  std::optional<NestPoint> anchor;
  std::vector<std::optional<NestPoint>> slots;
  std::unordered_map<std::string, NestPoint> by_name;
  std::optional<std::string> label;
};
LUMEX_DEFINE_FIELD_NAMES (NestStdOptionals, anchor, slots, by_name, label);
} // namespace lumex_field_reflection_tests

TEST (LumexFieldNamesJsonNestedTest,
      GivenStdOptionalTypes_WhenKindAsked_ThenNlohmannKeepsWhatItConverts)
{
  // nlohmann converts std::optional<int> (and so does the direct rule); it
  // does not convert an std::optional of an aggregate, which to_json writes.
  EXPECT_EQ (detail::value_kind<std::optional<int>>::value,
             detail::k_kind_direct);
  EXPECT_EQ (detail::value_kind<std::optional<NestPoint>>::value,
             detail::k_kind_optional);
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenEmptyStdOptionals_WhenToJson_ThenFieldsOmittedAndSlotsNull)
{
  NestStdOptionals value;
  value.slots.push_back (std::nullopt);
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (value);
  EXPECT_EQ (j.dump (), "{\"by_name\":{},\"slots\":[null]}");
}

TEST (LumexFieldNamesJsonNestedTest,
      GivenEngagedStdOptionals_WhenToJson_ThenNestedObjects)
{
  NestStdOptionals value;
  NestPoint const point = { 1, 2 };
  value.anchor = point;
  value.slots.push_back (point);
  value.slots.push_back (std::nullopt);
  value.by_name["only"] = point;
  value.label = std::string ("l");
  nlohmann::json const j
      = lumex::core::reflection::field_reflection::to_json (value);
  EXPECT_EQ (j.dump (),
             "{\"anchor\":{\"x\":1,\"y\":2},"
             "\"by_name\":{\"only\":{\"x\":1,\"y\":2}},\"label\":\"l\","
             "\"slots\":[{\"x\":1,\"y\":2},null]}");
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
