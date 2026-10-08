/*
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

/**
 * @file LumexOptionalGlobals.hpp
 * @brief The global names of the optional module: `optional`, `nullopt`,
 * `make_optional` and `lumex_bad_optional_access`.
 * @details `opt/LumexOptional.hpp` declares the types in
 * `lumex::core::optional::opt` and puts nothing at global scope. The umbrella
 * `lumex/core/optional/LumexOptional` includes this header on top of it, so a
 * file that includes the umbrella can write `optional<int>` and `nullopt`. A
 * header of LumexLib that needs the type but must not inject names into every
 * file that includes it (`LumexMemRead.hpp` before C++17) includes
 * `opt/LumexOptional.hpp` and spells the namespace out.
 */
#ifndef LUMEX_CORE_OPTIONAL_OPT_GLOBALS_HPP
#define LUMEX_CORE_OPTIONAL_OPT_GLOBALS_HPP

#include "lumex/core/optional/opt/LumexOptional.hpp"

// `in_place` has no global alias: `expected` has its own `in_place`, and two
// global names for two objects would stop a file from including both modules.
// Write `lumex::core::optional::opt::in_place`.

/**
 * @brief Global alias for
 * `lumex::core::optional::opt::lumex_bad_optional_access`.
 * @details This allows `lumex_bad_optional_access` to be used without full
 * namespace qualification.
 */
using lumex::core::optional::opt::lumex_bad_optional_access;
/**
 * @brief Global alias for `lumex::core::optional::opt::make_optional`.
 * @details This allows `make_optional` to be used without full namespace
 * qualification, improving readability and mimicking `std::make_optional`.
 */
using lumex::core::optional::opt::make_optional;
/**
 * @brief Global alias for `lumex::core::optional::opt::nullopt`.
 * @details This allows `nullopt` to be used without full namespace
 * qualification, improving readability and mimicking `std::nullopt`.
 */
using lumex::core::optional::opt::nullopt;

/**
 * @brief Global type alias for `lumex::core::optional::opt::optional<T>`.
 * @details This allows `optional` to be used without full namespace
 * qualification, improving readability and making it behave more like
 * `std::optional`.
 * @tparam T The type of the value to be held.
 */
template <typename T> using optional = lumex::core::optional::opt::optional<T>;

#endif // !LUMEX_CORE_OPTIONAL_OPT_GLOBALS_HPP
