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
 * @file ExpectedTypes.hpp
 * @brief The tags and the placeholder type that `expected` uses: `in_place`,
 * `unexpect` and `Unit`.
 * @details `in_place_tag` and `in_place` request in-place construction of the
 * value, `unexpect_t` and `unexpect` in-place construction of the error, as
 * `std::in_place` (C++17) and `std::unexpect` (C++23) do. `Unit` is the empty
 * object that fills the success alternative of `expected<void, E>`. The
 * `is_expected` traits are not here but in `LumexTypeTraits.hpp`. The tags are
 * also visible at global scope.
 */
#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_TYPES_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_TYPES_HPP

#include <type_traits>

#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// ====================== Helper tags and types ======================

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
/**
 * @brief In-place construction tag; C++11 has no std::in_place.
 * @details Directly constructs the contained value inside `expected`, avoiding
 * extra copies or moves. Analogue of `std::in_place_t` from C++17.
 */
struct in_place_tag
{
};

/**
 * @brief Global constant of type `in_place_tag`.
 * @details Pass to an `expected` constructor to request in-place construction
 * of the success value.
 * @note Analogue of `std::in_place` from C++17.
 */
LUMEX_CONSTEXPR in_place_tag in_place{};

/**
 * @brief Placeholder success type for `expected<void, ErrorType>`.
 * @details Represents an empty successful value when `expected` holds no data
 * but is in the success state. Enables a C++23-like `std::expected<void, E>`.
 * @note Used as the success-type stub in the `expected<void, ErrorType>`
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

// is_expected / is_expected_v / is_expected_concept live in
// lumex/core/utility/traits/LumexTypeTraits.hpp (traits::value).

} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

// `in_place` has no global alias: `optional` has its own `in_place`, and two
// global names for two objects would stop a file from including both modules.
// Write `lumex::core::expected::result::in_place`.
using lumex::core::expected::result::in_place_tag;
using lumex::core::expected::result::unexpect;
using lumex::core::expected::result::unexpect_t;

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_TYPES_HPP
