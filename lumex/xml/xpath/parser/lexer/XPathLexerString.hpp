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

#ifndef LUMEX_XML_XPATH_PARSER_LEXER_XPATH_LEXER_STRING_HPP
#define LUMEX_XML_XPATH_PARSER_LEXER_XPATH_LEXER_STRING_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

using namespace lumex::xml::types;
using namespace lumex::xml::types::Types;

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace parser
{
namespace lexer
{
/**
 * @brief Represents a string view used by the XPath lexer.
 * @details This structure holds pointers to the beginning and end of a
 * character sequence, effectively providing a non-owning view of a string. It
 * is used by the XPath lexer to represent tokens found in the XPath
 * expression.
 *
 * @note This is a lightweight view and does not manage the lifetime of the
 * underlying character data. The character data must remain valid for the
 * lifetime of `XPathLexerString`.
 */
struct LUMEX_API XPathLexerString
{
  /// @brief Pointer to the beginning of the string data.
  char_t const
      *begin{}; // NOLINT(misc-non-private-member-variables-in-classes)
  /// @brief Pointer to one past the end of the string data.
  char_t const *end{}; // NOLINT(misc-non-private-member-variables-in-classes)

  /**
   * @brief Default constructor. Constructs an empty string view.
   * @details Initializes `begin` and `end` to `nullptr`, representing an empty
   * string.
   */
  XPathLexerString () = default;

  /**
   * @brief Equality comparison operator.
   * @details Compares the string view with a null-terminated C-style string
   * (`other`). The comparison is performed based on the character sequence and
   * length.
   * @param other A null-terminated C-style string to compare against.
   * @return `true` if the string view's content is equal to `other`, `false`
   * otherwise.
   */
  bool operator== (char_t const *other) const;
};
}
}
}
}
}

#endif // !LUMEX_XML_XPATH_PARSER_LEXER_XPATH_LEXER_STRING_HPP
