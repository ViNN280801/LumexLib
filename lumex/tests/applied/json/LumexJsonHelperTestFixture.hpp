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

#ifndef LUMEX_TESTS_APPLIED_JSON_HPP
#define LUMEX_TESTS_APPLIED_JSON_HPP

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"

// The fixture of the LumexJsonHelper tests and the diagnostics it captures.
// The test sources of every standard (LumexJsonHelper.cxx11.tests.cpp,
// .cxx17) add tests to the same GoogleTest suite, and every test of a suite
// must use one fixture class.

using lumex::applied::json::diagnostics::LumexJsonDiagnosticLevel;
using lumex::applied::json::diagnostics::LumexJsonDiagnosticSlot;

namespace lumex_json_test
{
struct captured_diagnostic_t
{
  LumexJsonDiagnosticLevel level;
  std::string message;
};

// The captured diagnostics, shared by every test source of a suite.
inline std::mutex &
captured_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

inline std::vector<captured_diagnostic_t> &
captured ()
{
  static std::vector<captured_diagnostic_t> entries;
  return entries;
}

inline void
capture_diagnostic (LumexJsonDiagnosticLevel level, char const *message)
{
  std::lock_guard<std::mutex> const lock (captured_mutex ());
  captured_diagnostic_t entry;
  entry.level = level;
  entry.message = message;
  captured ().push_back (entry);
}

inline std::size_t
count_level (LumexJsonDiagnosticLevel level)
{
  std::lock_guard<std::mutex> const lock (captured_mutex ());
  std::size_t count = 0;
  for (std::size_t i = 0; i < captured ().size (); ++i)
    if (captured ()[i].level == level)
      ++count;
  return count;
}

inline bool
any_message_contains (std::string const &needle)
{
  std::lock_guard<std::mutex> const lock (captured_mutex ());
  for (std::size_t i = 0; i < captured ().size (); ++i)
    if (captured ()[i].message.find (needle) != std::string::npos)
      return true;
  return false;
}

inline std::size_t
captured_count ()
{
  std::lock_guard<std::mutex> const lock (captured_mutex ());
  return captured ().size ();
}
} // namespace lumex_json_test

// Every test gets its own file, named after the standard of the suite and the
// test, so the suites of every standard can run in parallel. Diagnostics are
// captured instead of going to std::cerr, so tests can assert on them.
class LumexJsonHelperTest : public ::testing::Test
{
protected:
  std::string current_test_filename_;
  std::string test_section = "TestSection";

  void
  SetUp () override
  {
    {
      std::lock_guard<std::mutex> const lock (
          lumex_json_test::captured_mutex ());
      lumex_json_test::captured ().clear ();
    }
    LumexJsonDiagnosticSlot::set (&lumex_json_test::capture_diagnostic);

    ::testing::TestInfo const *info
        = ::testing::UnitTest::GetInstance ()->current_test_info ();
    current_test_filename_
        = std::string ("LumexJsonHelper.cxx") + std::to_string (__cplusplus)
          + "." + info->test_case_name () + "." + info->name () + ".json";
    if (std::remove (current_test_filename_.c_str ()) != 0 && errno != ENOENT)
      ADD_FAILURE () << "Cannot remove '" << current_test_filename_
                     << "' before the test: " << std::strerror (errno);
  }

  void
  TearDown () override
  {
    LumexJsonDiagnosticSlot::reset ();
    if (std::remove (current_test_filename_.c_str ()) != 0 && errno != ENOENT)
      ADD_FAILURE () << "Cannot remove '" << current_test_filename_
                     << "' after the test: " << std::strerror (errno);
  }

  void
  create_test_file (nlohmann::json const &content)
  {
    std::ofstream ofs (current_test_filename_.c_str ());
    if (!ofs.is_open ())
      {
        FAIL () << "Cannot create the test file " << current_test_filename_;
      }
    ofs << std::setw (4) << content << std::endl;
  }

  void
  create_raw_file (std::string const &text)
  {
    std::ofstream ofs (current_test_filename_.c_str ());
    ofs << text;
  }

  nlohmann::json
  read_file_content ()
  {
    std::ifstream ifs (current_test_filename_.c_str ());
    if (!ifs.is_open ())
      return nlohmann::json ();
    nlohmann::json content;
    try
      {
        ifs >> content;
      }
    catch (std::exception const &exc)
      {
        // Some tests write invalid JSON on purpose.
        std::cerr << "read_file_content(" << current_test_filename_
                  << "): " << exc.what () << std::endl;
      }
    return content;
  }
};

#endif // !LUMEX_TESTS_APPLIED_JSON_HPP
