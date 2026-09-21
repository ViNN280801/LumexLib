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

#ifndef LUMEX_XML_XPATH_EXCEPTION_HPP
#define LUMEX_XML_XPATH_EXCEPTION_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <exception>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/xml/xpath/parser/XPathParseResult.hpp"

using namespace lumex::xml::xpath::parser;

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace exception
{
/**
 * @brief Exception class for XPath parsing and evaluation errors.
 * @details This class inherits from `std::exception` and provides detailed
 *          information about errors encountered during XPath operations,
 *          including the error message and the `xpath_parse_result_t` which
 *          contains context about the error location.
 *
 * @note This exception is thrown when XPath parsing fails or when
 *       XPath evaluation encounters an unrecoverable error.
 * @see lumex::xml::xpath::parser::xpath_parse_result_t for detailed error
 * information.
 */
class XPathException : public std::exception
{
public:
  /**
   * @brief Constructs an XPathException from a parse result.
   * @details Initializes the exception with an `xpath_parse_result_t` that
   *          describes the XPath error. An assertion ensures that the
   *          provided `result_` indicates an actual error.
   * @param result_ A constant reference to an `xpath_parse_result_t` object
   *                containing the error details.
   * @throws `LUMEX_ASSERT` if `result_.error` is `nullptr`.
   */
  explicit XPathException (xpath_parse_result_t const &result_)
      : m_result (result_)
  {
    LUMEX_ASSERT (m_result.error);
  }

  /**
   * @brief Returns a null-terminated character string describing the
   * exception.
   * @details This function overrides `std::exception::what()` to provide
   *          the error message from the encapsulated `xpath_parse_result_t`.
   * @return A C-style string containing the error message. The pointer
   *         is guaranteed to be valid as long as the exception object exists.
   * @note The returned string is owned by the exception object and should not
   * be freed.
   */
  // Error message should not be discarded.
  LUMEX_ATTRIBUTE_NODISCARD ("Error message should not be discarded.")
  char const *
  what () const LUMEX_NOEXCEPT override
  {
    return m_result.error;
  }

  /**
   * @brief Retrieves the parse result associated with this exception.
   * @details Provides access to the `xpath_parse_result_t` object that was
   *          used to construct this exception, allowing callers to
   *          inspect more detailed error information, such as the error
   *          offset within the XPath expression.
   * @return A constant reference to the `xpath_parse_result_t` object.
   */
  // Parse result should not be discarded.
  LUMEX_ATTRIBUTE_NODISCARD ("Parse result should not be discarded.")
  xpath_parse_result_t const &
  result () const
  {
    return m_result;
  }

private:
  /// @brief The parse result containing details about the XPath error.
  xpath_parse_result_t m_result;
};
} // namespace exception
} // namespace xpath
} // namespace xml
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_XML_XPATH_EXCEPTION_HPP
