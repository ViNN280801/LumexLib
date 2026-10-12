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

#ifndef LUMEX_TESTS_CORE_EXCEPTIONS_EXCEPTION_HPP
#define LUMEX_TESTS_CORE_EXCEPTIONS_EXCEPTION_HPP

#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/exceptions/LumexException"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/time/LumexTime"

// Fixture of the lumex_base_exception tests. The test sources of every
// standard (LumexException.cxx11.tests.cpp, .cxx17) add tests to the same
// GoogleTest suite, and every test of a suite must use one fixture class.

class LumexExceptionTest : public ::testing::Test
{
protected:
  lumex::path test_crash_dir;

  void
  SetUp () override
  {
    // Create a unique temporary directory for each test fixture run
    // to avoid conflicts with crash reports from other tests.
    test_crash_dir
        = lumex::core::filesystem::fs::lumex_filesystem::temp_directory_path ()
              .value ()
          / ("LumexTestCrashes_" + lumex_time::get_timestamp_ns ());
    lumex::core::filesystem::fs::lumex_filesystem::create_directories (
        test_crash_dir);

    // This is important for to_crash_report to create new files in each test.
    // It relies on
    // lumex::core::filesystem::fs::lumex_filesystem::get_exe_path().parent_path()
    // / KDEFAULT_CRASHES_DIR_PATH We need to ensure KDEFAULT_CRASHES_DIR_PATH
    // is relative to our test_crash_dir For testing purposes, we need to
    // temporarily redirect the "default" crash path. This cannot be easily
    // done without modifying the `DefaultPaths.hpp` directly or using
    // environment variables, which the current `to_crash_report`
    // implementation does not take into account for its static initialization.
    // As a workaround for testing, we'll clean up the default crashes
    // directory before each test, or just check the presence of new files. For
    // concurrent tests, we'll check for the total number of entries.
  }

  void
  TearDown () override
  {
    // Clean up the temporary directory
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (test_crash_dir))
      lumex::core::filesystem::fs::lumex_filesystem::remove_all (
          test_crash_dir);
    // Also clean up the global default crash directory if it's not the same as
    // test_crash_dir
    lumex::path default_crash_path
        = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ()
              .parent_path ()
          / KDEFAULT_CRASHES_DIR_PATH;
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (
            default_crash_path)
        && default_crash_path != test_crash_dir)
      lumex::core::filesystem::fs::lumex_filesystem::remove_all (
          default_crash_path);
  }

  // Helper to read content of a file
  std::string
  read_file_content (lumex::path const &path)
  {
    std::ifstream file (path.string ());
    if (!file.is_open ())
      return "";
    std::stringstream buffer;
    buffer << file.rdbuf ();
    return buffer.str ();
  }
};

#endif // !LUMEX_TESTS_CORE_EXCEPTIONS_EXCEPTION_HPP
