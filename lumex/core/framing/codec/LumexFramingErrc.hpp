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
 * @file LumexFramingErrc.hpp
 * @brief The error codes of the frame codecs: `framing_errc` and its
 * `std::error_category`.
 * @details `framing_errc` converts to `std::error_code` implicitly
 * (`std::is_error_code_enum`). The category object lives in the framing
 * library alone (`LUMEX_FRAMING_API`), so an error code made in one shared
 * library compares equal to the enumerator in another module of the process.
 */
#ifndef LUMEX_CORE_FRAMING_CODEC_ERRC_HPP
#define LUMEX_CORE_FRAMING_CODEC_ERRC_HPP

#include "lumex/LumexExport.hpp"

#include <system_error>
#include <type_traits>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace codec
{
/**
 * @brief Why a frame could not be encoded or decoded.
 * @details The values start at 1: a value-initialized `std::error_code` of
 * the category means success.
 */
enum class framing_errc : int
{
  /// A received frame grew past `max_frame_size`; it was dropped.
  frame_too_large = 1,
  /// The check value of a frame did not match its contents.
  bad_checksum,
  /// An escape sequence (SLIP) or a transfer encoding (a hex digit) was
  /// invalid.
  bad_escape,
  /// A header or a fixed field of a frame was invalid.
  bad_header,
  /// A frame ended before its declared or minimal length.
  truncated,
  /// A payload to encode was larger than the codec can carry.
  payload_too_large
};

/**
 * @brief The category of `framing_errc`, named `"lumex.framing"`.
 * @return One object for the whole process.
 */
LUMEX_FRAMING_API std::error_category const &
framing_category () LUMEX_NOEXCEPT;

/// @brief Makes the `std::error_code` of a `framing_errc`.
inline std::error_code
make_error_code (framing_errc value) LUMEX_NOEXCEPT
{
  return std::error_code (static_cast<int> (value), framing_category ());
}

/// @brief Makes the `std::error_condition` of a `framing_errc`.
inline std::error_condition
make_error_condition (framing_errc value) LUMEX_NOEXCEPT
{
  return std::error_condition (static_cast<int> (value), framing_category ());
}
} // namespace codec
} // namespace framing
} // namespace core
} // namespace lumex

namespace std
{
template <>
struct is_error_code_enum<lumex::core::framing::codec::framing_errc>
    : true_type
{
};
} // namespace std

#endif // !LUMEX_CORE_FRAMING_CODEC_ERRC_HPP
