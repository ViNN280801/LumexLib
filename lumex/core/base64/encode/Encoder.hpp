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

#ifndef LUMEX_CORE_BASE64_ENCODE_HPP
#define LUMEX_CORE_BASE64_ENCODE_HPP

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
 * @brief Provides Base64 encoding functionalities.
 *
 * This namespace encapsulates functions for converting binary data to Base64
 * strings.
 */
namespace base64
{
namespace encode
{
class LUMEX_API Encoder final
{
public:
  /**
   * @brief Encodes raw binary data into a Base64 string.
   *
   * This function takes a pointer to raw binary data and its size, then
   * converts it into a Base64-encoded string. The resulting string can be
   * safely used for transmission or storage in text-based formats.
   *
   * @param[in] data Pointer to the binary data to be encoded.
   * @param[in] size The size of the binary data in bytes.
   * @return A `std::string` containing the Base64-encoded representation of
   * the input data. Returns an empty string if `data` is `nullptr` or `size`
   * is `0`.
   */
  static std::string encode (void const *data, std::size_t size);

  /**
   * @brief Encodes binary data from a `std::vector<byte_type>` into a Base64
   * string.
   *
   * This is an overloaded function that provides a convenient way to encode
   * binary data stored in a `std::vector<byte_type>`. It internally calls the
   * `encode` function that takes a pointer and size.
   *
   * @param[in] data A `std::vector<byte_type>` containing the binary data to
   * be encoded.
   * @return A `std::string` containing the Base64-encoded representation of
   * the input vector's data. Returns an empty string if the input vector is
   * empty.
   */
  static std::string encode (std::vector<byte_type> const &data);

#if __cplusplus >= 201703L
  /**
   * @brief Encodes binary data from a `std::string_view` into a Base64 string.
   *
   * This overload provides an efficient way to encode string data without
   * copying, leveraging `std::string_view` for read-only access to character
   * sequences.
   *
   * @param[in] data A `std::string_view` containing the binary data to be
   * encoded.
   * @return A `std::string` containing the Base64-encoded representation of
   * the input data. Returns an empty string if the input `string_view` is
   * empty.
   */
  static std::string encode (std::string_view data);
#endif

#if __cplusplus >= 202002L
  /**
   * @brief Encodes binary data from a `std::span<const byte_type>` into a
   * Base64 string.
   *
   * This overload offers the most generic and efficient way to encode
   * contiguous sequences of bytes without ownership, suitable for C++20 and
   * later.
   *
   * @param[in] data A `std::span<const byte_type>` containing the binary data
   * to be encoded.
   * @return A `std::string` containing the Base64-encoded representation of
   * the input data. Returns an empty string if the input `span` is empty.
   */
  static std::string encode (std::span<byte_type const> data);
#endif
};
} // namespace encode
} // namespace base64
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_BASE64_ENCODE_HPP
