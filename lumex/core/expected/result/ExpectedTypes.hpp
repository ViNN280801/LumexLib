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

#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_TYPES_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_TYPES_HPP

#include <type_traits>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

// ====================== Helper tags and types ======================
/**
 * @brief In-place construction tag; C++11 has no std::in_place.
 * @details Directly constructs the contained value inside `Expected`, avoiding
 * extra copies or moves. Analogue of `std::in_place_t` from C++17.
 */

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
struct in_place_tag
{
};

/**
 * @brief Global constant of type `in_place_tag`.
 * @details Pass to an `Expected` constructor to request in-place construction
 * of the success value.
 * @note Analogue of `std::in_place` from C++17.
 */
LUMEX_CONSTEXPR in_place_tag in_place{};

/**
 * @brief Placeholder success type for `Expected<void, ErrorType>`.
 * @details Represents an empty successful value when `Expected` holds no data
 * but is in the success state. Enables a C++23-like `std::expected<void, E>`.
 * @note Used as the success-type stub in the `Expected<void, ErrorType>`
 * specialization.
 */
struct Unit
{
};

/// @brief C++23-compatible tag: construct the error in place.
struct unexpect_t
{
};

/// @brief Global tag constant (like std::unexpect).
LUMEX_CONSTEXPR unexpect_t unexpect{};

// Forward declaration for is_expected type trait
template <typename SuccessType, typename ErrorType> class Expected;

/// @brief Type trait: whether T is an Expected. Default is false.
template <typename T> struct is_expected : std::false_type
{
};

/// @brief Type trait: `Expected<S, E>` is an Expected.
template <typename S, typename E>
struct is_expected<Expected<S, E>> : std::true_type
{
};

/// @brief Type trait: `Expected<void, E>` is an Expected.
template <typename E> struct is_expected<Expected<void, E>> : std::true_type
{
};

#if __cplusplus >= 202002L
// C++20 helper type traits and concepts
template <typename T>
concept is_expected_concept = is_expected<T>::value;

template <typename T>
LUMEX_CONSTEXPR bool is_expected_v = is_expected<T>::value;
#else
// Pre-C++20 helper type traits
template <typename T>
LUMEX_CONSTEXPR bool is_expected_v = is_expected<T>::value;
#endif

} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

using lumex::core::expected::result::in_place;
using lumex::core::expected::result::in_place_tag;
using lumex::core::expected::result::unexpect;
using lumex::core::expected::result::unexpect_t;

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_TYPES_HPP
