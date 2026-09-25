/**
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

#ifndef LUMEX_TESTS_CORE_FMT_FORMAT_TEST_HELPERS_HPP
#define LUMEX_TESTS_CORE_FMT_FORMAT_TEST_HELPERS_HPP

#include <string>

#include "lumex/core/fmt/LumexFormat.hpp"

/**
 * @file LumexFormatTestHelpers.hpp
 * @brief Run-time entry points for the LumexFormat suites.
 * @details A format string passed through these helpers skips the C++20
 *          compile-time check, so the same error cases run in every suite
 *          (C++11 / C++17 / C++20) and reach the run-time checker.
 */
namespace format_test_helpers
{
/** @brief Formats `text` with `args`, checking the string at run time. */
template <typename... Args>
std::string
runtime_format (std::string const &text, Args const &...args)
{
  return lumex::core::fmt::vformat (
      text, lumex::core::fmt::make_format_args (args...));
}

/** @brief Wide counterpart of `runtime_format`. */
template <typename... Args>
std::wstring
runtime_wformat (std::wstring const &text, Args const &...args)
{
  return lumex::core::fmt::vformat (
      text, lumex::core::fmt::make_wformat_args (args...));
}

/**
 * @brief Message of the `FormatError` thrown for `text` / `args`, or
 * `"<no error>"` when formatting succeeds.
 */
template <typename... Args>
std::string
format_error (std::string const &text, Args const &...args)
{
  try
    {
      (void)runtime_format (text, args...);
    }
  catch (lumex::core::fmt::FormatError const &error)
    {
      return error.what ();
    }
  return "<no error>";
}
} // namespace format_test_helpers

#endif // !LUMEX_TESTS_CORE_FMT_FORMAT_TEST_HELPERS_HPP
