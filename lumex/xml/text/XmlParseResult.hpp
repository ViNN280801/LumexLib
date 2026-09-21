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

#ifndef LUMEX_XML_TEXT_XML_PARSE_RESULT_HPP
#define LUMEX_XML_TEXT_XML_PARSE_RESULT_HPP

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

#include "lumex/LumexExport.hpp"

#include <cstddef>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/xml/types/XmlTypes.hpp"

using namespace lumex::xml;
using namespace lumex::xml::types;
using namespace lumex::xml::types::Types;

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace document
{
struct XmlDocumentBase;
}

namespace node
{
struct XmlNodeBase;
}

namespace text
{
/**
 * @brief Represents the result of an XML parsing operation, including status,
 * error offset, and encoding.
 * @details This structure provides comprehensive feedback on whether a parsing
 * operation succeeded or failed, and if it failed, details about the error
 * type and its location within the input data.
 */
struct LUMEX_API xml_parse_result_t
{
  /// @brief Parsing status (see `xml_parse_status` enumeration for possible
  /// values).
  Types::xml_parse_status
      status; // NOLINT(misc-non-private-member-variables-in-classes)

  /// @brief Last parsed offset in `char_t` units from the start of the input
  /// data.
  ///        Indicates the position of the error if `status` is not
  ///        `status_ok`.
  ptrdiff_t offset; // NOLINT(misc-non-private-member-variables-in-classes)

  /// @brief The detected or specified character encoding of the source
  /// document.
  Types::xml_encoding
      encoding; // NOLINT(misc-non-private-member-variables-in-classes)

  /**
   * @brief Default constructor. Initializes the object to a failed state
   * (`status_internal_error`).
   */
  xml_parse_result_t ();

  /**
   * @brief Constructs an `xml_parse_result_t` with a specified status.
   * @param[in] status The parsing status to set for this result.
   */
  xml_parse_result_t (Types::xml_parse_status status);

  /**
   * @brief Conversion operator to `bool`.
   * @details Allows `xml_parse_result_t` objects to be used in boolean
   * contexts (e.g., `if (result)`). It evaluates to `true` if `status` is
   * `status_ok`, and `false` otherwise.
   * @return `true` if parsing was successful, `false` otherwise.
   */
  operator bool () const;

  /**
   * @brief Retrieves a human-readable description of the parsing status.
   * @return A null-terminated C-style string describing the parsing status.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned value is the same as the one returned by the "
      "description() method; "
      "discarding it negates the purpose of the getter.")
  char const *description () const;
};

/**
 * @brief Helper function to create an `xml_parse_result_t` object.
 * @param[in] status The parsing status.
 * @param[in] offset The offset in the input data where the status occurred.
 * Defaults to 0.
 * @return An `xml_parse_result_t` object initialized with the provided status
 * and offset.
 */
LUMEX_API
inline xml_parse_result_t
make_parse_result (Types::xml_parse_status status, ptrdiff_t offset = 0)
{
  xml_parse_result_t result;
  result.status = status;
  result.offset = offset;

  return result;
}

/**
 * @brief Internal implementation for loading an XML document from a buffer.
 * @param[in,out] doc The `XmlDocumentBase` object to populate with parsed
 * data.
 * @param[in,out] root The root `XmlNodeBase` where the parsed document
 * structure will be attached.
 * @param[in] contents A pointer to the raw input data buffer.
 * @param[in] size The size of the input buffer in bytes.
 * @param[in] options A bitmask of `xml_parse_option` flags controlling parsing
 * behavior.
 * @param[in] encoding The expected character encoding of the buffer.
 * @param[in] is_mutable `true` if the input buffer can be modified by the
 * parser, `false` otherwise.
 * @param[in] own `true` if the parser should take ownership of `contents` and
 * deallocate it, `false` otherwise.
 * @param[out] out_buffer A pointer to a `char_t*` that will receive the
 * pointer to the internal buffer used for parsing.
 * @return An `xml_parse_result_t` object indicating the parsing outcome.
 * @details This function handles buffer conversion, BOM skipping, memory
 * ownership, and dispatches to the main XML parsing logic. It is an internal
 * utility function called by `XmlDocument::load`.
 */
LUMEX_API
xml_parse_result_t load_buffer_impl (
    document::XmlDocumentBase *doc, node::XmlNodeBase *root, void *contents,
    std::size_t size, // NOLINT(bugprone-easily-swappable-parameters)
    unsigned int options, Types::xml_encoding encoding, bool is_mutable,
    bool own, Types::char_t **out_buffer);
} // namespace text
} // namespace xml
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_XML_TEXT_XML_PARSE_RESULT_HPP
