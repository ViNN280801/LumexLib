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
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LUMEX_XML_XPATH_PARSER_XPATH_PARSE_RESULT_HPP
#define LUMEX_XML_XPATH_PARSER_XPATH_PARSE_RESULT_HPP

#include "lumex/LumexExport.hpp"

#include <cstddef>

#include "lumex/core/utility/attr/LumexAttributes.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace parser
{
/**
 * @brief Represents the result of an XPath parsing operation.
 * @details This structure encapsulates whether an XPath expression was parsed
 *          successfully, and if not, provides details about the error,
 * including an error message and the offset in the query string where the
 * error occurred.
 */
struct LUMEX_API xpath_parse_result_t
{
  /// @brief A null-terminated C-string containing the error message if parsing
  /// failed, or `nullptr` if successful.
  char const *error; // NOLINT(misc-non-private-member-variables-in-classes)

  /// @brief The offset (in `char_t` units) from the beginning of the XPath
  /// query string where the error occurred.
  ptrdiff_t offset; // NOLINT(misc-non-private-member-variables-in-classes)

  /**
   * @brief Default constructor. Initializes the object to a failed state.
   * @details By default, a newly constructed `xpath_parse_result_t` indicates
   * an "Internal error" at offset 0, implying an unsuccessful parsing attempt
   * unless explicitly updated.
   */
  xpath_parse_result_t ();

  /**
   * @brief Conversion operator to `bool`.
   * @details Allows `xpath_parse_result_t` objects to be used in boolean
   * contexts. It evaluates to `true` if `error` is `nullptr` (no error), and
   * `false` otherwise.
   * @return `true` if parsing was successful, `false` if an error occurred.
   */
  operator bool () const;

  /**
   * @brief Gets the error description.
   * @details Returns the error message if parsing failed, or "No error" if
   * successful.
   * @return A null-terminated C-string describing the parsing result.
   */
  // Error description should not be discarded.
  LUMEX_ATTRIBUTE_NODISCARD ("Error description should not be discarded.")
  char const *description () const;
};
} // namespace parser
} // namespace xpath
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_XPATH_PARSER_XPATH_PARSE_RESULT_HPP
