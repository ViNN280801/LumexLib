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

/**
 * @file XmlText.hpp
 * @brief `XmlText`, typed access to the text content of a node.
 * @details `XmlNode::text()` returns an `XmlText` for an element or a text
 * node. It reads the node's first PCDATA or CDATA child, or the element's own
 * value when the document was parsed with `kparse_embed_pcdata`, and converts
 * it to numbers or a boolean; the given default is returned only when there is
 * no text. Setting a value creates a PCDATA child when there is none. The
 * handle does not own the text and is valid as long as the document.
 *
 * The `std::string_view` overloads exist only when the including code is
 * compiled as C++17 or later. The file also declares the logical AND and OR
 * operators with a `bool`. Consumers include it through `lumex/xml/LumexXml`.
 */
#ifndef LUMEX_XML_TEXT_XML_TEXT_HPP
#define LUMEX_XML_TEXT_XML_TEXT_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/attr/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
// Forward declaration
namespace node
{
class XmlNode;
struct XmlNodeBase;
}
using namespace node;

namespace text
{
using namespace lumex::xml::types;
using namespace lumex::xml::types::Types;

/**
 * @brief Represents the text content of an XML node, providing type-safe
 * access and modification.
 * @details `XmlText` acts as a facade over internal `node_pcdata` or
 * `node_cdata` nodes, or the value of element nodes (if `kparse_embed_pcdata`
 * was used during parsing). It allows retrieving the text as various C++ types
 * (int, double, bool, string) and setting it with automatic type conversion.
 * @note This object does not own the memory for the text content; its lifetime
 * is tied to the `XmlDocument` from which its underlying node originates.
 * @see XmlNode::text()
 */
class LUMEX_API XmlText
{
  friend class node::XmlNode;

public:
  /// @brief Type definition for safe boolean conversion, preventing
  /// problematic implicit conversions.
  using unspecified_bool_type = void (*) (XmlText ***);

  /**
   * @brief Default constructor. Constructs an empty `XmlText` object.
   * @details An empty `XmlText` object does not point to any valid text
   * content.
   */
  XmlText ();

  /**
   * @brief Safe boolean conversion operator.
   * @details Allows an `XmlText` object to be used in boolean contexts (e.g.,
   * `if (text)`). It evaluates to `true` if the `XmlText` object points to
   * valid text content, and `false` otherwise.
   * @return A pointer to a dummy function if the internal text data is not
   * null, otherwise `nullptr`.
   */
  operator unspecified_bool_type () const;

  /**
   * @brief Logical NOT operator.
   * @details Returns `true` if the `XmlText` object is empty (does not point
   * to valid text content), `false` otherwise.
   * @return `true` if the text object is empty, `false` otherwise.
   */
  bool operator!() const;

  /**
   * @brief Checks if the text object is empty (null).
   * @return `true` if the text object is empty, `false` otherwise.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The returned boolean indicates whether the text "
                             "object is empty; discarding it "
                             "negates the purpose of the getter.")
  bool empty () const;

  /**
   * @brief Retrieves the text content as a C-style string.
   * @return A null-terminated C-style string representing the text content, or
   * `LUMEX_XML_TEXT("")` if the object is empty.
   * @note The returned pointer points to internal memory and should not be
   * deallocated or modified. Its lifetime is tied to the XML document.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned C-style string text should be used; discarding it negates "
      "the purpose of the getter.")
  char_t const *get () const;

  /**
   * @brief Retrieves the text content as a C-style string, or a default value
   * if empty.
   * @param[in] def The default C-style string to return if the text object is
   * empty. Defaults to `LUMEX_XML_TEXT("")`.
   * @return A null-terminated C-style string representing the text content, or
   * `def` if the object is empty.
   */
  char_t const *as_string (char_t const *def = LUMEX_XML_TEXT ("")) const;

  /**
   * @brief Converts the text object's value to an `int`.
   * @details Skips leading white space, then reads an optional sign and either
   * decimal digits or `0x` and hexadecimal digits, up to the first other
   * character. A value without digits gives `0`, and a value out of range
   * gives `INT_MIN` or `INT_MAX`. `def` is returned only when the object is
   * empty or its text has no value, not for a value that is not a number.
   * @param[in] def The value returned when the object is empty or its text has
   * no value. Defaults to `0`.
   * @return The converted value, or `def`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned integer value should be used; discarding it negates the "
      "purpose of the getter.")
  int as_int (int def = 0) const;

  /**
   * @brief Converts the text object's value to an `unsigned int`.
   * @details Skips leading white space, then reads an optional sign and either
   * decimal digits or `0x` and hexadecimal digits, up to the first other
   * character. A value without digits or a negative value gives `0`, and a
   * value above the range gives `UINT_MAX`. `def` is returned only when the
   * object is empty or its text has no value, not for a value that is not a
   * number.
   * @param[in] def The value returned when the object is empty or its text has
   * no value. Defaults to `0`.
   * @return The converted value, or `def`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned unsigned integer value should be used; discarding it "
      "negates the purpose of the getter.")
  unsigned int as_uint (unsigned int def = 0) const;

  /**
   * @brief Converts the text object's value to a `double`.
   * @details Converts the value as `strtod` does (`wcstod` in wide-character
   * mode); a value that is not a number gives `0`. `def` is returned only when
   * the object is empty or its text has no value, not for a value that is not
   * a number.
   * @param[in] def The value returned when the object is empty or its text has
   * no value. Defaults to `0.0`.
   * @return The converted value, or `def`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned double value should be used; discarding it negates the "
      "purpose of the getter.")
  double as_double (double def = 0) const;

  /**
   * @brief Converts the text object's value to a `float`.
   * @details Converts the value as `strtod` does (`wcstod` in wide-character
   * mode), then narrows it to `float`; a value that is not a number gives `0`.
   * `def` is returned only when the object is empty or its text has no value,
   * not for a value that is not a number.
   * @param[in] def The value returned when the object is empty or its text has
   * no value. Defaults to `0.0f`.
   * @return The converted value, or `def`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned float value should be used; discarding it negates the "
      "purpose of the getter.")
  float as_float (float def = 0) const;

  /**
   * @brief Converts the text object's value to a `long long`.
   * @details Skips leading white space, then reads an optional sign and either
   * decimal digits or `0x` and hexadecimal digits, up to the first other
   * character. A value without digits gives `0`, and a value out of range
   * gives `LLONG_MIN` or `LLONG_MAX`. `def` is returned only when the object
   * is empty or its text has no value, not for a value that is not a number.
   * @param[in] def The value returned when the object is empty or its text has
   * no value. Defaults to `0LL`.
   * @return The converted value, or `def`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned long long value should be used; discarding it negates the "
      "purpose of the getter.")
  long long as_llong (long long def = 0) const;

  /**
   * @brief Converts the text object's value to an `unsigned long long`.
   * @details Skips leading white space, then reads an optional sign and either
   * decimal digits or `0x` and hexadecimal digits, up to the first other
   * character. A value without digits or a negative value gives `0`, and a
   * value above the range gives `ULLONG_MAX`. `def` is returned only when the
   * object is empty or its text has no value, not for a value that is not a
   * number.
   * @param[in] def The value returned when the object is empty or its text has
   * no value. Defaults to `0ULL`.
   * @return The converted value, or `def`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned unsigned long long value should be used; discarding it "
      "negates the purpose of the getter.")
  unsigned long long as_ullong (unsigned long long def = 0) const;

  /**
   * @brief Converts the text object's value to a `bool`.
   * @details The result is `true` when the first character of the value is
   * '1', 't', 'T', 'y' or 'Y', and `false` for any other value, including an
   * empty one. `def` is returned only when the object is empty or its text has
   * no value.
   * @param[in] def The value returned when the object is empty or its text has
   * no value. Defaults to `false`.
   * @return The converted value, or `def`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned boolean value should be used; discarding it negates the "
      "purpose of the getter.")
  bool as_bool (bool def = false) const;

  /**
   * @brief Sets the text content from a null-terminated C-style string.
   * @param[in] rhs The new text content.
   * @return `true` if the text was successfully set, `false` otherwise (e.g.,
   * if the object is empty or there is insufficient memory).
   * @details This function handles memory allocation/reallocation for the text
   * string.
   */
  bool set (char_t const *rhs);
  /**
   * @brief Sets the text content from a C-style string with a specified size.
   * @param[in] rhs The new text content.
   * @param[in] size The number of characters in `rhs` (excluding null
   * terminator).
   * @return `true` if the text was successfully set, `false` otherwise.
   * @details This function handles memory allocation/reallocation for the text
   * string.
   */
  bool set (char_t const *rhs, std::size_t size);
#if __cplusplus >= 201703L
  /**
   * @brief Sets the text content from a `std::string_view`.
   * @param[in] rhs The new text content as a `std::string_view`.
   * @return `true` if the text was successfully set, `false` otherwise.
   * @note Available only when compiled with C++17 or later.
   */
  bool
  set (string_view_t rhs)
  {
    return set (rhs.data (), rhs.size ());
  }
#endif

  /**
   * @brief Sets the text content from an integer, converting it to a string.
   * @param[in] rhs The integer value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (int rhs);
  /**
   * @brief Sets the text content from an unsigned integer, converting it to a
   * string.
   * @param[in] rhs The unsigned integer value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (unsigned int rhs);
  /**
   * @brief Sets the text content from a long integer, converting it to a
   * string.
   * @param[in] rhs The long integer value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (long rhs);
  /**
   * @brief Sets the text content from an unsigned long integer, converting it
   * to a string.
   * @param[in] rhs The unsigned long integer value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (unsigned long rhs);
  /**
   * @brief Sets the text content from a double-precision floating-point
   * number, converting it to a string.
   * @param[in] rhs The double value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (double rhs);
  /**
   * @brief Sets the text content from a double-precision floating-point number
   * with a specified precision.
   * @param[in] rhs The double value.
   * @param[in] precision The number of digits after the decimal point.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (double rhs, int precision);
  /**
   * @brief Sets the text content from a single-precision floating-point
   * number, converting it to a string.
   * @param[in] rhs The float value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (float rhs);
  /**
   * @brief Sets the text content from a single-precision floating-point number
   * with a specified precision.
   * @param[in] rhs The float value.
   * @param[in] precision The number of digits after the decimal point.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (float rhs, int precision);
  /**
   * @brief Sets the text content from a boolean, converting it to "true" or
   * "false".
   * @param[in] rhs The boolean value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (bool rhs);

  /**
   * @brief Sets the text content from a long long integer, converting it to a
   * string.
   * @param[in] rhs The long long integer value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (long long rhs);
  /**
   * @brief Sets the text content from an unsigned long long integer,
   * converting it to a string.
   * @param[in] rhs The unsigned long long integer value.
   * @return `true` if the text was successfully set, `false` otherwise.
   */
  bool set (unsigned long long rhs);

  /**
   * @brief Assignment operator for a C-style string.
   * @param[in] rhs The null-terminated C-style string to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Equivalent to `set(rhs)` but without error checking in the return
   * value.
   */
  XmlText &operator= (char_t const *rhs);
  /**
   * @brief Assignment operator for an integer.
   * @param[in] rhs The integer to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the integer to a string and assigns it.
   */
  XmlText &operator= (int rhs);
  /**
   * @brief Assignment operator for an unsigned integer.
   * @param[in] rhs The unsigned integer to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the unsigned integer to a string and assigns it.
   */
  XmlText &operator= (unsigned int rhs);
  /**
   * @brief Assignment operator for a long integer.
   * @param[in] rhs The long integer to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the long integer to a string and assigns it.
   */
  XmlText &operator= (long rhs);
  /**
   * @brief Assignment operator for an unsigned long integer.
   * @param[in] rhs The unsigned long integer to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the unsigned long integer to a string and assigns it.
   */
  XmlText &operator= (unsigned long rhs);
  /**
   * @brief Assignment operator for a double-precision floating-point number.
   * @param[in] rhs The double value to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the double to a string and assigns it.
   */
  XmlText &operator= (double rhs);
  /**
   * @brief Assignment operator for a single-precision floating-point number.
   * @param[in] rhs The float value to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the float to a string and assigns it.
   */
  XmlText &operator= (float rhs);
  /**
   * @brief Assignment operator for a boolean.
   * @param[in] rhs The boolean value to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the boolean to "true" or "false" and assigns it.
   */
  XmlText &operator= (bool rhs);

#if __cplusplus >= 201703L
  /**
   * @brief Assignment operator for a `std::string_view`.
   * @param[in] rhs The `std::string_view` to assign.
   * @return A reference to the modified `XmlText` object.
   * @note Available only when compiled with C++17 or later.
   */
  XmlText &
  operator= (string_view_t rhs)
  {
    set (rhs.data (), rhs.size ());
    return *this;
  }
#endif

  /**
   * @brief Assignment operator for a long long integer.
   * @param[in] rhs The long long integer to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the long long integer to a string and assigns it.
   */
  XmlText &operator= (long long rhs);
  /**
   * @brief Assignment operator for an unsigned long long integer.
   * @param[in] rhs The unsigned long long integer to assign.
   * @return A reference to the modified `XmlText` object.
   * @details Converts the unsigned long long integer to a string and assigns
   * it.
   */
  XmlText &operator= (unsigned long long rhs);

  /**
   * @brief Retrieves the underlying data node (`node_pcdata` or `node_cdata`)
   * for this text object.
   * @return An `XmlNode` object representing the data node, or an empty
   * `XmlNode` if no such node exists.
   * @details This allows direct interaction with the node that actually holds
   * the text content.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned data node should be used; discarding it negates the "
      "purpose of the getter.")
  XmlNode data () const;

private:
  /**
   * @brief Private constructor. Used internally to create an `XmlText` object
   * from an `XmlNodeBase` pointer.
   * @param[in] root The `XmlNodeBase` pointer that this `XmlText` object will
   * wrap.
   */
  explicit XmlText (XmlNodeBase *root);

  /// @brief Pointer to the underlying `XmlNodeBase` that contains or points to
  /// the text data.
  XmlNodeBase *m_root;

  /**
   * @brief Internal utility to get or create a data node for setting text.
   * @details If a text node already exists, it returns it. Otherwise, it
   * appends a new `node_pcdata` child to `m_root` and returns it.
   * @return A pointer to the `XmlNodeBase` that can hold the text data.
   */
  XmlNodeBase *_data_new ();

  /**
   * @brief Internal utility to retrieve the raw `XmlNodeBase` pointer
   * containing the text data.
   * @return A pointer to the internal `XmlNodeBase` that stores the text, or
   * `nullptr` if no text node is found.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned internal pointer should be used; discarding it negates "
      "the purpose of the getter.")
  XmlNodeBase *_data () const;
};

/**
 * @brief Overload for logical AND operator with `XmlText` on the left-hand
 * side.
 * @param[in] lhs The `XmlText` object.
 * @param[in] rhs The boolean value.
 * @return `true` if `lhs` is valid and `rhs` is `true`, `false` otherwise.
 * @details This enables expressions like `if (xml_text &&
 * some_boolean_condition)`.
 */
LUMEX_API
bool operator&& (XmlText const &lhs, bool rhs);

/**
 * @brief Overload for logical OR operator with `XmlText` on the left-hand
 * side.
 * @param[in] lhs The `XmlText` object.
 * @param[in] rhs The boolean value.
 * @return `true` if `lhs` is valid or `rhs` is `true`, `false` otherwise.
 * @details This enables expressions like `if (xml_text ||
 * some_boolean_condition)`.
 */
LUMEX_API
bool operator|| (XmlText const &lhs, bool rhs);
} // namespace text
} // namespace xml
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_XML_TEXT_XML_TEXT_HPP
