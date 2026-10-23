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
 * @file LumexProtocolRow.hpp
 * @brief The templates behind a table row: `field_list`, `reply_of`,
 * `no_reply`, `names_t`, `count_names`, the traits `is_command`,
 * `is_protocol`, `is_layout`, the command list `commands_of` /
 * `command_count`, and `invoke_row` for the generated client methods.
 * @details The macros of `LumexProtocolDefine.hpp` only name things and
 * forward to these templates.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_ROW_HPP
#define LUMEX_CORE_PROTOCOL_DSL_ROW_HPP

#include <cstddef>
#include <type_traits>
#include <utility>

#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace dsl
{
/// @brief The field names of a row or layout, in field order (string
/// literals with static storage).
typedef ::lumex::core::span::view::span<char const *const> names_t;

/// @brief The tag a command type names as `lumex_protocol_command`.
struct command_tag
{
};

/// @brief The tag a protocol names as `lumex_protocol_kind`.
struct protocol_tag
{
};

/// @brief The tag a layout names as `lumex_protocol_layout`.
struct layout_tag
{
};

/// @brief The reply of a row whose reply group is empty.
struct no_reply
{
};

namespace detail
{
template <bool... B> struct row_bools
{
};

template <bool... B>
struct row_all : std::is_same<row_bools<true, B...>, row_bools<B..., true>>
{
};

template <typename T> struct row_voider
{
  typedef void type;
};
} // namespace detail

/**
 * @brief The fields of a row or layout, in wire order.
 * @details Every element must be a field type (`field::is_field`): an
 * integer field, a layout, or a field type of the protocol layer.
 */
template <typename... Fields> struct field_list
{
  static_assert (
      detail::row_all<
          ::lumex::core::protocol::field::is_field<Fields>::value...>::value,
      "LUMEX_PROTOCOL: every element of a field group must be a field type "
      "(u8_<>, u16_le_<...>, a LUMEX_PROTOCOL_LAYOUT, ...)");

  /// The fields as a type list.
  typedef type_list<Fields...> types;
  /// The number of fields.
  typedef std::integral_constant<std::size_t, sizeof...(Fields)> count;
};

/**
 * @brief `reply_of<...>::type`: the reply type of a reply group: `no_reply`
 * for an empty group, the type for one type.
 */
template <typename... T> struct reply_of
{
  static_assert (sizeof...(T) <= 1,
                 "LUMEX_PROTOCOL: a reply group holds at most one type (a "
                 "field, a layout or a reply kind)");
  /// Not used: the group is invalid.
  typedef no_reply type;
};

/// @copydoc reply_of
template <> struct reply_of<>
{
  /// No reply.
  typedef no_reply type;
};

/// @copydoc reply_of
template <typename T> struct reply_of<T>
{
  /// The reply.
  typedef T type;
};

namespace detail
{
// The field names of a row without the leading null pointer the macro puts
// before them (so that an empty group still makes an array). Here and not in
// the macro: the pointer arithmetic stays out of the user's code.
template <std::size_t N>
names_t
names_after_first (char const *const (&names)[N]) LUMEX_NOEXCEPT
{
  return names_t (names + 1, N - 1);
}
} // namespace detail

/// @brief The number of names in a name group: `count_names ("a", "b")`.
template <typename... Names>
LUMEX_CONSTEXPR std::size_t
count_names (Names const &...) LUMEX_NOEXCEPT
{
  return sizeof...(Names);
}

/// @brief Whether `T` is a command type generated for a table row.
template <typename T, typename = void> struct is_command : std::false_type
{
};

/// @copydoc is_command
template <typename T>
struct is_command<
    T, typename detail::row_voider<typename T::lumex_protocol_command>::type>
    : std::is_same<typename T::lumex_protocol_command, command_tag>
{
};

/// @brief Whether `T` is a protocol (`LUMEX_PROTOCOL_DEFINE`, `_EXTEND`).
template <typename T, typename = void> struct is_protocol : std::false_type
{
};

/// @copydoc is_protocol
template <typename T>
struct is_protocol<
    T, typename detail::row_voider<typename T::lumex_protocol_kind>::type>
    : std::is_same<typename T::lumex_protocol_kind, protocol_tag>
{
};

/// @brief Whether `T` is a layout (`LUMEX_PROTOCOL_LAYOUT`).
template <typename T, typename = void> struct is_layout : std::false_type
{
};

/// @copydoc is_layout
template <typename T>
struct is_layout<
    T, typename detail::row_voider<typename T::lumex_protocol_layout>::type>
    : std::is_same<typename T::lumex_protocol_layout, layout_tag>
{
};

/**
 * @brief `commands_of<P>::type`: the command types of protocol `P` as a
 * `type_list`, in table order.
 * @details For a layer (`LUMEX_PROTOCOL_EXTEND`): the rows of its base in
 * their order, each one replaced by the layer's row of the same identifier,
 * then the new rows of the layer.
 */
template <typename P> struct commands_of
{
  static_assert (is_protocol<P>::value,
                 "commands_of<P>: P must be a LUMEX_PROTOCOL_DEFINE or "
                 "LUMEX_PROTOCOL_EXTEND type");
  /// The command types.
  typedef typename P::template lumex_resolve<P>::type type;
};

/// @brief The number of commands of protocol `P`.
template <typename P>
struct command_count : list_size<typename commands_of<P>::type>
{
};

/**
 * @brief Calls `client.lumex_invoke<Row> (args...)`: what a generated
 * client method (`client.PING (args...)`) does.
 */
template <typename Row, typename Client, typename... Args>
auto
invoke_row (Client &client, Args &&...args)
    -> decltype (client.template lumex_invoke<Row> (
        std::forward<Args> (args)...))
{
  return client.template lumex_invoke<Row> (std::forward<Args> (args)...);
}
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_DSL_ROW_HPP
