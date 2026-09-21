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

#ifndef LUMEX_CORE_BASE64_DECODE_HPP
#define LUMEX_CORE_BASE64_DECODE_HPP

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

#include <vector>

#include "lumex/core/base64/codec/Base64.hpp"

using namespace lumex::core::base64::codec::Types;

#if __cplusplus >= 201703L
#include <string_view>
#endif

#if __cplusplus >= 202002L
#include <span>
#endif

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
class LUMEX_API Decoder final
{
public:
  /**
   * @brief Decodes a Base64 string into binary data and stores it in an output
   * vector.
   *
   * This function attempts to decode the given Base64 string. The decoded
   * binary data is appended to the provided `out` vector. Before appending,
   * the `out` vector is cleared to ensure it only contains the result of the
   * current decoding operation.
   *
   * @param[in] encoded The Base64-encoded input string.
   * @param[out] out A `std::vector<byte_type>` that will store the decoded
   * binary data. It is cleared at the beginning of the function.
   * @return `true` if the decoding was successful and the input string was a
   * valid Base64 format, `false` otherwise (e.g., invalid length, contains
   * non-Base64 characters, or incorrect padding).
   */
#if __cplusplus >= 201703L
  static bool decode (std::string_view encoded, std::vector<byte_type> &out);
#else
  static bool decode (std::string const &encoded, std::vector<byte_type> &out);
#endif

  /**
   * @brief Decodes a Base64 string into binary data and returns it as a
   * `std::vector<byte_type>`.
   *
   * This is an overloaded function that provides a convenient way to decode a
   * Base64 string and receive the result directly as a returned
   * `std::vector<byte_type>`.
   *
   * @param[in] encoded The Base64-encoded input string.
   * @return A `std::vector<byte_type>` containing the decoded binary data.
   *         Returns an empty vector if the decoding fails or the input string
   * is invalid.
   */
#if __cplusplus >= 201703L
  static std::vector<byte_type> decode (std::string_view encoded);
#else
  static std::vector<byte_type> decode (std::string const &encoded);
#endif
};
} // namespace decode
} // namespace base64
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_BASE64_DECODE_HPP
