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

// LumexFieldNamesStdOptional.cxx17.tests.cpp
//
// Registered aggregates whose members are all std::optional (std::optional
// exists from C++17): the shape of ChannelAmqpError::error_message_t. From
// C++17 the automatic get<I> reads through structured bindings, which is
// where an optional-only aggregate once failed on MSVC; the registration must
// not change that. The C++17 and C++20 suites compile this file. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON; the macro gates the body as in the other
// field-reflection files.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionRegisteredFixtures.hpp"

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

namespace lumex_field_reflection_tests
{
struct RegAllStdOptional
{
  std::optional<int> device;
  std::optional<int> faultyUnit;
  std::optional<int> issue;
  std::optional<std::string> errorLogFile;
  std::optional<std::string> message;
  std::optional<std::string> errorMessage;
};
LUMEX_DEFINE_FIELD_NAMES (RegAllStdOptional, device, faultyUnit, issue,
                          errorLogFile, message, errorMessage);
} // namespace lumex_field_reflection_tests

TEST (LumexFieldNamesRegistrationTest,
      GivenAllStdOptionalFields_WhenNamed_ThenCamelCaseNamesKept)
{
  std::array<char const *, 6> const names
      = names_as_array<RegAllStdOptional> ();
  EXPECT_STREQ (names[0], "device");
  EXPECT_STREQ (names[1], "faultyUnit");
  EXPECT_STREQ (names[2], "issue");
  EXPECT_STREQ (names[3], "errorLogFile");
  EXPECT_STREQ (names[4], "message");
  EXPECT_STREQ (names[5], "errorMessage");
  EXPECT_EQ (tuple_size<RegAllStdOptional>::value, 6u);
}

TEST (LumexFieldNamesJsonTest,
      GivenAllStdOptionalFields_WhenToJson_ThenEngagedKeysOnlyAndTypesSurvive)
{
  RegAllStdOptional obj;
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
  EXPECT_EQ (j.size (), 2u);
  RegAllStdOptional typed{};
  EXPECT_TRUE (
      (std::is_same<
          typename std::remove_reference<decltype (get<0> (typed))>::type,
          std::optional<int>>::value));
  EXPECT_TRUE (
      (std::is_same<
          typename std::remove_reference<decltype (get<3> (typed))>::type,
          std::optional<std::string>>::value));
}

TEST (LumexFieldNamesGetTest,
      GivenAllStdOptionalFields_WhenStructuredBinding_ThenSameOrderAsGet)
{
  RegAllStdOptional obj;
  obj.device = 1;
  obj.errorMessage = std::string ("e");
  auto &[device, faulty, issue, log_file, message, error_message] = obj;
  EXPECT_EQ (&get<0> (obj), &device);
  EXPECT_EQ (&get<1> (obj), &faulty);
  EXPECT_EQ (&get<2> (obj), &issue);
  EXPECT_EQ (&get<3> (obj), &log_file);
  EXPECT_EQ (&get<4> (obj), &message);
  EXPECT_EQ (&get<5> (obj), &error_message);
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
