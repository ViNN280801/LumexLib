/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

/**
 * @file LumexTextCase.hpp
 * @brief Normalizes text for case-insensitive comparison.
 */
#ifndef LUMEX_CORE_STRING_TEXT_TEXT_CASE_HPP
#define LUMEX_CORE_STRING_TEXT_TEXT_CASE_HPP

#include <cctype>
#include <string>
#include <utility>

#if __cplusplus >= 202002L
#include <ranges>
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace string
{
namespace text
{
/**
 * @brief Lowercases `str` in place (ASCII `std::tolower`), so later
 * comparisons are case-insensitive.
 * @param[in,out] str The text; the result is written back into it.
 * @param[in] need_to_remove_spaces When true (the default), every whitespace
 * character is removed as well.
 */
inline void
to_case_insensitive (std::string &str, bool need_to_remove_spaces = true)
{
#if __cplusplus >= 202002L
  auto lowered = str
                 | std::ranges::views::transform (
                     [] (unsigned char chr)
                       { return static_cast<char> (std::tolower (chr)); });

  if (need_to_remove_spaces)
    {
      auto filtered
          = lowered
            | std::ranges::views::filter ([] (unsigned char chr)
                                            { return !std::isspace (chr); });
      str = std::string (filtered.begin (), filtered.end ());
    }
  else
    {
      str = std::string (lowered.begin (), lowered.end ());
    }
#else
  // No <ranges> before C++20: one explicit pass does the same work.
  std::string result;
  result.reserve (str.size ());
  for (std::string::const_iterator it = str.begin (); it != str.end (); ++it)
    {
      unsigned char const chr = static_cast<unsigned char> (*it);
      if (need_to_remove_spaces && std::isspace (chr) != 0)
        continue;
      result.push_back (static_cast<char> (std::tolower (chr)));
    }
  str = std::move (result);
#endif
}

/**
 * @brief Copy-out overload of
 * `to_case_insensitive(std::string &, bool)`.
 * @param[in] orig The original text; left untouched.
 * @param[out] out Receives the lowercased (and optionally whitespace-free)
 * text.
 * @param[in] need_to_remove_spaces When true (the default), whitespace is
 * removed as well.
 */
inline void
to_case_insensitive (std::string const &orig, std::string &out,
                     bool need_to_remove_spaces = true)
{
  out = orig;
  to_case_insensitive (out, need_to_remove_spaces);
}
} // namespace text
} // namespace string
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_STRING_TEXT_TEXT_CASE_HPP
