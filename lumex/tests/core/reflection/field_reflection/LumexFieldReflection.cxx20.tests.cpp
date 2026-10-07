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

// LumexFieldReflection.cxx20.tests.cpp
//
// Field names (names_as_array) and to_json, which the library declares only
// when __cplusplus is at least 202002L (pointer non-type template parameter
// pretty names). The C++20 suite compiles this file together with the
// .cxx11 and .cxx14 files. GCC 8 reports 201709L at -std=c++2a, so there the
// library has neither and the tests skip; the test gates below are that
// library switch, not a standard check of this file. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the
// C++11 file.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionTestFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
// All std::optional fields: PeakExpertCE ChannelAmqpError::error_message_t
// shape. C++17+ get uses structured bindings; C++14 loophole must not lock
// the contained type (layout mismatch) or redefine the friend (C2084).
struct AllStdOptionalFields
{
  std::optional<int> device;
  std::optional<int> faultyUnit;
  std::optional<int> issue;
  std::optional<std::string> errorLogFile;
  std::optional<std::string> message;
  std::optional<std::string> errorMessage;
};

class NestedOptionalHost
{
public:
  struct error_message_t
  {
    std::optional<int> device;
    std::optional<int> faultyUnit;
    std::optional<int> issue;
    std::optional<std::string> errorLogFile;
    std::optional<std::string> message;
    std::optional<std::string> errorMessage;
  };
};

#if __cplusplus >= 202002L
inline std::string
expected_field_name (std::size_t i)
{
  return std::string ("f") + std::to_string (i);
}

template <std::size_t I>
void
check_name_index (char const *name)
{
  EXPECT_STREQ (name, expected_field_name (I).c_str ());
}

template <std::size_t I>
void
check_json_index (nlohmann::json const &j)
{
  typedef typename field_type_at<I>::type field_t;
  std::string const key = expected_field_name (I);
  ASSERT_TRUE (j.contains (key)) << key;
  expect_sample_eq (j.at (key).get<field_t> (), make_sample<field_t> (I));
}

template <std::size_t N, std::size_t... I>
void
check_names (std::array<char const *, N> const &names,
             std::index_sequence<I...>)
{
  int swallow[] = { 0, (check_name_index<I> (names[I]), 0)... };
  (void)swallow;
}

template <std::size_t... I>
void
check_json_fields (nlohmann::json const &j, std::index_sequence<I...>)
{
  int swallow[] = { 0, (check_json_index<I> (j), 0)... };
  (void)swallow;
}
#endif
} // namespace lumex_field_reflection_tests

TEST (LumexAggregateFieldsTest,
      GivenEmptyAggregate_WhenNamed_ThenEmptyNameArray)
{
#if __cplusplus >= 202002L
  std::array<char const *, 0> const names = names_as_array<Empty> ();
  EXPECT_TRUE (names.empty ());
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexAggregateFieldsTest,
      GivenPlainAggregate_WhenNamed_ThenExactFieldNames)
{
#if __cplusplus >= 202002L
  EXPECT_EQ (tuple_size<Plain>::value, 2u);
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  ASSERT_EQ (names.size (), 2u);
  EXPECT_STREQ (names[0], "id");
  EXPECT_STREQ (names[1], "name");
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexAggregateFieldsTest,
      GivenPlainAggregate_WhenUnknownNameQueried_ThenNotFound)
{
#if __cplusplus >= 202002L
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  bool found_id = false;
  bool found_unknown = false;
  for (char const *name : names)
    {
      if (std::string (name) == "id")
        found_id = true;
      if (std::string (name) == "not_a_field")
        found_unknown = true;
    }
  EXPECT_TRUE (found_id);
  EXPECT_FALSE (found_unknown);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexAggregateFieldsTest, NamesAsArray_WhenFound_ThenContainsId)
{
#if __cplusplus >= 202002L
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  bool found_id = false;
  for (char const *name : names)
    {
      if (std::string (name) == "id")
        found_id = true;
    }
  EXPECT_TRUE (found_id);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexAggregateFieldsTest, NamesAsArray_WhenUnfound_ThenAbsent)
{
#if __cplusplus >= 202002L
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  bool found_unknown = false;
  for (char const *name : names)
    {
      if (std::string (name) == "not_a_field")
        found_unknown = true;
    }
  EXPECT_FALSE (found_unknown);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest, GivenEmptyAggregate_WhenToJson_ThenEmptyObject)
{
#if __cplusplus >= 202002L
  nlohmann::json const j = to_json (Empty{});
  EXPECT_TRUE (j.is_object ());
  EXPECT_TRUE (j.empty ());
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest, GivenOneField_WhenToJson_ThenSingleKey)
{
#if __cplusplus >= 202002L
  nlohmann::json const j = to_json (OneField{ 5 });
  ASSERT_TRUE (j.contains ("only"));
  EXPECT_EQ (j["only"].get<int> (), 5);
  EXPECT_EQ (j.size (), 1u);
  EXPECT_FALSE (j.contains ("not_a_field"));
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenPlainAggregate_WhenToJson_ThenAllFieldsAreSerializedByName)
{
#if __cplusplus >= 202002L
  Plain const obj{ 42, "hello" };
  nlohmann::json const j = to_json (obj);

  ASSERT_TRUE (j.contains ("id"));
  ASSERT_TRUE (j.contains ("name"));
  EXPECT_EQ (j["id"].get<int> (), 42);
  EXPECT_EQ (j["name"].get<std::string> (), "hello");
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenOptionalFieldWithValue_WhenToJson_ThenFieldIsWritten)
{
#if __cplusplus >= 202002L
  WithOptionalPresent obj;
  obj.id = 1;
  obj.label = std::string ("present");
  nlohmann::json const j = to_json (obj);

  ASSERT_TRUE (j.contains ("label"));
  EXPECT_EQ (j["label"].get<std::string> (), "present");
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenOptionalFieldWithoutValue_WhenToJson_ThenFieldIsOmitted)
{
#if __cplusplus >= 202002L
  WithOptionalAbsent obj;
  obj.id = 2;
  obj.label = nullopt;
  nlohmann::json const j = to_json (obj);

  EXPECT_TRUE (j.contains ("id"));
  EXPECT_FALSE (j.contains ("label"));
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenContainerField_WhenToJson_ThenSerializedAsJsonArray)
{
#if __cplusplus >= 202002L
  WithVector const obj{ { 1, 2, 3 } };
  nlohmann::json const j = to_json (obj);

  ASSERT_TRUE (j.contains ("values"));
  EXPECT_TRUE (j["values"].is_array ());
  EXPECT_EQ (j["values"].size (), 3u);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenMixedOptionalFields_WhenToJson_ThenOnlyEngagedOptionalsAreWritten)
{
#if __cplusplus >= 202002L
  MixedOptionals obj;
  obj.missing = nullopt;
  obj.always = 9;
  obj.present = 4;
  nlohmann::json const j = to_json (obj);

  EXPECT_FALSE (j.contains ("missing"));
  ASSERT_TRUE (j.contains ("always"));
  ASSERT_TRUE (j.contains ("present"));
  EXPECT_EQ (j["always"].get<int> (), 9);
  EXPECT_EQ (j["present"].get<int> (), 4);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenAllStdOptionalFields_WhenToJson_ThenEngagedKeysOnlyAndTypesSurvive)
{
#if __cplusplus >= 202002L
  AllStdOptionalFields obj;
  obj.issue = 10661;
  obj.message = std::string ("openFiles normalize error");
  nlohmann::json const j = to_json (obj);

  EXPECT_FALSE (j.contains ("device"));
  EXPECT_FALSE (j.contains ("faultyUnit"));
  EXPECT_FALSE (j.contains ("errorLogFile"));
  EXPECT_FALSE (j.contains ("errorMessage"));
  ASSERT_TRUE (j.contains ("issue"));
  ASSERT_TRUE (j.contains ("message"));
  EXPECT_EQ (j["issue"].get<int> (), 10661);
  EXPECT_EQ (j["message"].get<std::string> (), "openFiles normalize error");
  EXPECT_EQ (tuple_size<AllStdOptionalFields>::value, 6u);
  AllStdOptionalFields typed{};
  EXPECT_TRUE (
      (std::is_same<
          typename std::remove_reference<decltype (get<0> (typed))>::type,
          std::optional<int>>::value));
  EXPECT_TRUE (
      (std::is_same<
          typename std::remove_reference<decltype (get<3> (typed))>::type,
          std::optional<std::string>>::value));
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (
    LumexFieldReflectionTest,
    GivenNestedAllStdOptionalFields_WhenToJson_ThenEngagedKeysOnlyAndTypesSurvive)
{
#if __cplusplus >= 202002L
  NestedOptionalHost::error_message_t obj;
  obj.issue = 10661;
  obj.message = std::string ("openFiles normalize error");
  nlohmann::json const j = to_json (obj);

  EXPECT_FALSE (j.contains ("device"));
  ASSERT_TRUE (j.contains ("issue"));
  ASSERT_TRUE (j.contains ("message"));
  EXPECT_EQ (j["issue"].get<int> (), 10661);
  EXPECT_EQ (tuple_size<NestedOptionalHost::error_message_t>::value, 6u);
  NestedOptionalHost::error_message_t typed{};
  EXPECT_TRUE (
      (std::is_same<
          typename std::remove_reference<decltype (get<0> (typed))>::type,
          std::optional<int>>::value));
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenScalarFields_WhenToJson_ThenTypesAndEmptyStringArePreserved)
{
#if __cplusplus >= 202002L
  Scalars const obj{ true, 1.5, "" };
  nlohmann::json const j = to_json (obj);

  EXPECT_EQ (j["flag"].get<bool> (), true);
  EXPECT_DOUBLE_EQ (j["ratio"].get<double> (), 1.5);
  EXPECT_EQ (j["empty"].get<std::string> (), "");
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenUnderscoreFieldNames_WhenToJson_ThenKeysMatchIdentifiers)
{
#if __cplusplus >= 202002L
  UnderscoreNames const obj{ 1, 2 };
  nlohmann::json const j = to_json (obj);
  ASSERT_TRUE (j.contains ("field_a"));
  ASSERT_TRUE (j.contains ("field_b"));
  EXPECT_EQ (j["field_a"].get<int> (), 1);
  EXPECT_EQ (j["field_b"].get<int> (), 2);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest,
      GivenEightFields_WhenToJson_ThenEveryIndexIsPresent)
{
#if __cplusplus >= 202002L
  EightFields const obj{ 0, 1, 2, 3, 4, 5, 6, 7 };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j.size (), 8u);
  EXPECT_EQ (j["a0"].get<int> (), 0);
  EXPECT_EQ (j["a7"].get<int> (), 7);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TEST (LumexFieldReflectionTest, GivenPaddedAggregate_WhenToJson_ThenAllKeys)
{
#if __cplusplus >= 202002L
  Padded const obj{ 'q', 4, 8.0 };
  nlohmann::json const j = to_json (obj);
  ASSERT_TRUE (j.contains ("a"));
  ASSERT_TRUE (j.contains ("b"));
  ASSERT_TRUE (j.contains ("c"));
  EXPECT_EQ (j["a"].get<char> (), 'q');
  EXPECT_EQ (j["b"].get<int> (), 4);
  EXPECT_DOUBLE_EQ (j["c"].get<double> (), 8.0);
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TYPED_TEST (LumexFieldArityTest, GivenArity_WhenNamed_ThenFIndexNames)
{
#if __cplusplus >= 202002L
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  std::array<char const *, tag_t::value> const names
      = names_as_array<agg_t> ();
  ASSERT_EQ (names.size (), tag_t::value);
  check_names<tag_t::value> (names, std::make_index_sequence<tag_t::value>{});
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

TYPED_TEST (LumexFieldArityTest, GivenArity_WhenToJson_ThenEveryKey)
{
#if __cplusplus >= 202002L
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  agg_t obj{};
  fill_fields (obj, std::make_index_sequence<tag_t::value>{});
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j.size (), tag_t::value);
  check_json_fields (j, std::make_index_sequence<tag_t::value>{});
#else
  GTEST_SKIP () << "names_as_array and to_json need __cplusplus "
                   ">= 202002L";
#endif
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
