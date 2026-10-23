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
 * @file LumexProtocolFieldTraits.hpp
 * @brief What makes a type a field of a protocol row: `field_tag`,
 * `is_field`, and the modifier tag `modifier_tag` / `is_modifier`.
 * @details A field type declares `typedef field_tag lumex_protocol_field;`.
 * Integer fields, layouts (`LUMEX_PROTOCOL_LAYOUT`) and the field types of
 * the later packages all do; the DSL refuses anything else in a field group.
 * A modifier (`scale<Ratio>`, units) declares `typedef modifier_tag
 * lumex_protocol_modifier;`.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_TRAITS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_TRAITS_HPP

#include <type_traits>

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
/// @brief The tag a field type names as `lumex_protocol_field`.
struct field_tag
{
};

/// @brief The tag a field modifier names as `lumex_protocol_modifier`.
struct modifier_tag
{
};

namespace detail
{
template <typename T> struct voider
{
  typedef void type;
};
} // namespace detail

/// @brief Whether `T` is a field type (it names `lumex_protocol_field`).
template <typename T, typename = void> struct is_field : std::false_type
{
};

/// @copydoc is_field
template <typename T>
struct is_field<
    T, typename detail::voider<typename T::lumex_protocol_field>::type>
    : std::is_same<typename T::lumex_protocol_field, field_tag>
{
};

/// @brief Whether `T` is a field modifier (it names
/// `lumex_protocol_modifier`).
template <typename T, typename = void> struct is_modifier : std::false_type
{
};

/// @copydoc is_modifier
template <typename T>
struct is_modifier<
    T, typename detail::voider<typename T::lumex_protocol_modifier>::type>
    : std::is_same<typename T::lumex_protocol_modifier, modifier_tag>
{
};
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_PROTOCOL_FIELD_TRAITS_HPP
