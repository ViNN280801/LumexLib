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
 * @file LumexProtocolErrc.hpp
 * @brief The error codes of the protocol layer: `protocol_errc` and its
 * `std::error_category`.
 * @details `protocol_errc` converts to `std::error_code` implicitly
 * (`std::is_error_code_enum`). The category object lives in the protocol
 * library alone (`LUMEX_PROTOCOL_API`), so an error code made in one shared
 * library compares equal to the enumerator in another module of the process.
 * New codes are appended; the values of the existing ones never change.
 */
#ifndef LUMEX_CORE_PROTOCOL_ERROR_ERRC_HPP
#define LUMEX_CORE_PROTOCOL_ERROR_ERRC_HPP

#include "lumex/LumexExport.hpp"

#include <system_error>
#include <type_traits>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace protocol
{
namespace error
{
/**
 * @brief Why a message of a protocol could not be encoded or decoded.
 * @details The values start at 1: a value-initialized `std::error_code` of
 * the category means success.
 */
enum class protocol_errc : int
{
  /// A message ended before its last field.
  truncated = 1,
  /// Bytes were left after the last field of a message.
  trailing_bytes,
  /// A field held a value its type cannot represent (a digit, an enumerator,
  /// a range).
  invalid_value,
  /// No row of the protocol has the key of a message.
  unknown_key
};

/**
 * @brief The category of `protocol_errc`, named `"lumex.protocol"`.
 * @return One object for the whole process.
 */
LUMEX_PROTOCOL_API std::error_category const &
protocol_category () LUMEX_NOEXCEPT;

/// @brief Makes the `std::error_code` of a `protocol_errc`.
inline std::error_code
make_error_code (protocol_errc value) LUMEX_NOEXCEPT
{
  return std::error_code (static_cast<int> (value), protocol_category ());
}

/// @brief Makes the `std::error_condition` of a `protocol_errc`.
inline std::error_condition
make_error_condition (protocol_errc value) LUMEX_NOEXCEPT
{
  return std::error_condition (static_cast<int> (value), protocol_category ());
}
} // namespace error
} // namespace protocol
} // namespace core
} // namespace lumex

namespace std
{
template <>
struct is_error_code_enum<lumex::core::protocol::error::protocol_errc>
    : true_type
{
};
} // namespace std

#endif // !LUMEX_CORE_PROTOCOL_ERROR_ERRC_HPP
