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

/**
 * @file Validator.hpp
 * @brief Base64 syntax check: the `Validator` class of the `lumex::base64`
 * library.
 * @details `Validator::is_valid_base64()` tells whether text is well-formed
 * Base64 without decoding it: alphabet characters followed by at most two `=`,
 * with or without padding. The pointer and size overload is compiled into the
 * library and has the same signature in every C++ standard; the string
 * overload is an inline wrapper over it (`std::string_view` from C++17,
 * `std::string` before). Like the other Base64 headers, it brings the names of
 * the codec's `Types` namespace into the global namespace with a
 * using-directive.
 */
#ifndef LUMEX_CORE_BASE64_VALIDATE_HPP
#define LUMEX_CORE_BASE64_VALIDATE_HPP

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

#include <cstddef>

#include "lumex/core/base64/codec/Base64.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace base64
{
namespace validate
{
using namespace lumex::core::base64::codec::Types;

/**
 * @brief Base64 syntax check.
 * @details The pointer and size overload is exported and has the same
 * signature in every C++ standard; the string overload is an inline wrapper
 * over it (`string_type_t` is `std::string_view` from C++17 and
 * `std::string const &` below).
 */
class LUMEX_API Validator final
{
public:
  /**
   * @brief Checks whether `size` characters starting at `str` are valid
   * Base64.
   *
   * Valid Base64 consists of alphabet characters followed by at most two
   * `'='`; with padding the length is a multiple of 4, without it the last
   * group has two or three characters. The range needs no terminating NUL
   * and nothing after it is read; a NUL inside the range is invalid.
   *
   * @param[in] str First character; never valid as `nullptr`, even with a
   * `size` of 0 (pass `""` for an empty input).
   * @param[in] size Number of characters to check.
   * @return `true` if the range is valid Base64 (an empty range is), `false`
   * otherwise, and always for `nullptr`.
   */
  static bool is_valid_base64 (char const *str, std::size_t size);

  /**
   * @brief Checks if a given string is a valid Base64 encoded string (see
   * the pointer and size overload for the rules).
   *
   * @param[in] str The string to be checked for Base64 validity.
   * @return `true` if the string adheres to Base64 formatting rules, `false`
   * otherwise.
   */
  static bool
  is_valid_base64 (string_type_t str)
  {
    // An empty input is valid: a default-constructed std::string_view has no
    // data pointer, which the core rejects.
    return is_valid_base64 (str.empty () ? "" : str.data (), str.size ());
  }
};
} // namespace validate
} // namespace base64
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_BASE64_VALIDATE_HPP
