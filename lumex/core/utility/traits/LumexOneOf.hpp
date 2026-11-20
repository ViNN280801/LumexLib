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
 * @file LumexOneOf.hpp
 * @brief The trait `is_one_of` and the C++20 concept `OneOf`: a type is one of
 * a list of types, cv-qualifiers of the tested type ignored.
 * @details A header of its own that includes only `<type_traits>`: the
 * header-only modules (math among them) need it without the rest of
 * `LumexTypeTraits.hpp`, which includes it.
 */
#ifndef LUMEX_CORE_UTILITY_TRAITS_ONE_OF_HPP
#define LUMEX_CORE_UTILITY_TRAITS_ONE_OF_HPP

#include <type_traits>

namespace lumex
{
namespace core
{
namespace utility
{
namespace traits
{
namespace meta
{
/**
 * @brief `T` without `const` and `volatile` is the same type as at least one
 * of `Ts` (false for an empty `Ts`); `Ts` are compared as written (the C++11
 * form of the concept `OneOf`).
 * @details The variadic helper that the MSVC STL builds `is_integral_v` on
 * (`_Is_any_of_v`). Derived from `std::integral_constant` like the standard
 * traits.
 */
template <typename T, typename... Ts> struct is_one_of : std::false_type
{
};

template <typename T, typename U, typename... Ts>
struct is_one_of<T, U, Ts...>
    : std::integral_constant<
          bool, std::is_same<typename std::remove_cv<T>::type, U>::value
                    || is_one_of<T, Ts...>::value>
{
};

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
/** @brief `T` without `const` and `volatile` is one of `Ts`. */
template <typename T, typename... Ts>
concept OneOf = is_one_of<T, Ts...>::value;
#endif
} // namespace meta
} // namespace traits
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_TRAITS_ONE_OF_HPP
