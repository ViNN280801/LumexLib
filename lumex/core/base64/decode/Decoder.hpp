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
 * @file Decoder.hpp
 * @brief Base64 decoding: the `decoder` class of the `lumex::base64` library.
 * @details `decoder::decode()` turns Base64 text in the standard alphabet
 * (with `+` and `/`, padding optional) into bytes, either into a caller's
 * vector with a success flag or as a returned vector that is empty for invalid
 * input. The overloads that take a pointer and a size are compiled into the
 * library and have the same signature in every C++ standard; the string
 * overloads are inline wrappers over them (they take the `lumex_string_view`
 * of `lumex::string_view` in every standard; a `std::string_view` converts to
 * it). Like the other Base64
 * headers, it brings the names of the codec's `Types` namespace (`byte_type`,
 * `string_type_t`) into the global namespace with a using-directive.
 */
#ifndef LUMEX_CORE_BASE64_DECODE_HPP
#define LUMEX_CORE_BASE64_DECODE_HPP

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
#include <vector>

#include "lumex/core/base64/codec/Base64.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
/**
 * @brief Provides Base64 decoding functionalities.
 *
 * This namespace encapsulates functions for converting Base64 strings to
 * binary data.
 */
namespace base64
{
namespace decode
{
using namespace lumex::core::base64::codec::Types;

/**
 * @brief Base64 decoder.
 * @details The two functions that take a pointer and a size are exported and
 * have the same signature in every C++ standard, so a consumer built at
 * another standard than the library links. The overloads that take a string
 * (`string_type_t`) are inline wrappers over them: `string_type_t` is
 * `lumex_string_view` in every standard; a string literal, a `char const *`,
 * a `std::string` and, from C++17, a `std::string_view` convert to it. The
 * class itself is not exported: a dllimport class makes clang-cl emit an
 * import for an inline member it does not inline, and the library does not
 * provide the standard-dependent overload.
 */
class decoder final
{
public:
  /**
   * @brief Decodes `size` Base64 characters starting at `encoded` into `out`.
   *
   * The range needs no terminating NUL and nothing after it is read; a NUL
   * inside the range is an invalid character. The input may omit the
   * trailing padding (`"SGk"` decodes like `"SGk="`). `out` is cleared first
   * and stays empty on failure.
   *
   * @param[in] encoded First character of the input; never valid as
   * `nullptr`, even with a `size` of 0 (pass `""` for an empty input).
   * @param[in] size Number of characters to decode.
   * @param[out] out Receives the decoded bytes.
   * @return `true` if the range is valid Base64 (an empty range is),
   * `false` otherwise (invalid length, a character outside the alphabet,
   * misplaced or excess padding, `nullptr`).
   */
  LUMEX_API static bool decode (char const *encoded, std::size_t size,
                                std::vector<byte_type> &out);

  /**
   * @brief Decodes `size` Base64 characters starting at `encoded`.
   * @param[in] encoded First character of the input; never valid as
   * `nullptr`.
   * @param[in] size Number of characters to decode.
   * @return The decoded bytes, or an empty vector if the range is not valid
   * Base64.
   */
  LUMEX_API static std::vector<byte_type> decode (char const *encoded,
                                                  std::size_t size);

  /**
   * @brief Decodes a Base64 string into `out` (see the pointer and size
   * overload).
   * @param[in] encoded The Base64-encoded input; sized, so a NUL character
   * inside it is an invalid character.
   * @param[out] out Receives the decoded bytes; cleared first.
   * @return `true` if the input is valid Base64, `false` otherwise.
   */
  static bool
  decode (string_type_t encoded, std::vector<byte_type> &out)
  {
    // An empty input is valid: a default-constructed view has no data
    // pointer, which the core rejects.
    return decode (encoded.empty () ? "" : encoded.data (), encoded.size (),
                   out);
  }

  /**
   * @brief Decodes a Base64 string.
   * @param[in] encoded The Base64-encoded input; sized, so a NUL character
   * inside it is an invalid character.
   * @return The decoded bytes, or an empty vector if the input is invalid.
   */
  static std::vector<byte_type>
  decode (string_type_t encoded)
  {
    return decode (encoded.empty () ? "" : encoded.data (), encoded.size ());
  }
};
} // namespace decode
} // namespace base64
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_BASE64_DECODE_HPP
