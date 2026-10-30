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
using names_t = ::lumex::core::span::view::span<char const *const>;

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
  using type = void;
};

// Whether class T is final (a final class cannot be probed for a name).
#if __cplusplus >= 201402L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
template <typename T> struct is_final_t : std::is_final<T>
{
};
#else
template <typename T>
struct is_final_t : std::integral_constant<bool, __is_final (T)>
{
};
#endif
} // namespace detail

/// @cond LUMEX_INTERNAL
// LUMEX_PROTOCOL_DETAIL_NAME_PROBE (name_) defines, in the namespace where it
// is used, names_<name_>_t<T>: whether class T has a member named name_ of
// any kind and any access (a type, a template, a function, a data member,
// private or public). The name is ambiguous in a class derived from T and
// from a class with a data member of that name. False for a class that is
// final and for a type that is no class. A customization point detected by
// its name instead of by a valid use refuses a member it cannot use instead
// of silently ignoring it.
#define LUMEX_PROTOCOL_DETAIL_NAME_PROBE(name_)                               \
  struct name_##_holder_t                                                     \
  {                                                                           \
    int name_;                                                                \
  };                                                                          \
  template <typename LumexT>                                                  \
  struct name_##_probe_t : LumexT, name_##_holder_t                           \
  {                                                                           \
  };                                                                          \
  template <typename LumexT, typename = void>                                 \
  struct name_##_unambiguous_t : std::false_type                              \
  {                                                                           \
  };                                                                          \
  template <typename LumexT>                                                  \
  struct name_##_unambiguous_t<                                               \
      LumexT, typename ::lumex::core::protocol::dsl::detail::row_voider<      \
                  decltype (&name_##_probe_t<LumexT>::name_)>::type>          \
      : std::true_type                                                        \
  {                                                                           \
  };                                                                          \
  template <                                                                  \
      typename LumexT,                                                        \
      bool LumexProbe                                                         \
      = std::is_class<LumexT>::value                                          \
        && !::lumex::core::protocol::dsl::detail::is_final_t<LumexT>::value>  \
  struct names_##name_##_t                                                    \
      : std::integral_constant<bool, !name_##_unambiguous_t<LumexT>::value>   \
  {                                                                           \
  };                                                                          \
  template <typename LumexT>                                                  \
  struct names_##name_##_t<LumexT, false> : std::false_type                   \
  {                                                                           \
  }
/// @endcond

/**
 * @brief The fields of a row or layout, in wire order.
 * @details Every element is a field type (`field::is_field`): a field, a
 * layout, or a field type of another layer; the row and layout macros check
 * that with their own messages (`group_check`). The list is itself a group
 * of fields: its value is `field::record<Fields...>`.
 */
template <typename... Fields> struct field_list
{
  /// The fields as a type list.
  using types = type_list<Fields...>;
  /// The number of fields.
  using count = std::integral_constant<std::size_t, sizeof...(Fields)>;
  /// A group of fields.
  using lumex_protocol_field = ::lumex::core::protocol::field::field_tag;
  /// Its codec: the record codec.
  using lumex_field_impl
      = ::lumex::core::protocol::field::group_impl<field_list>;
};

/// @brief `canonical_fields<F...>::type`: the field list of a group, each
/// vocabulary name replaced by its field (`u8_` written alone is
/// `field::u8_<>`).
template <typename... F> struct canonical_fields
{
  /// The field list.
  using type = field_list<
      typename ::lumex::core::protocol::field::resolve<F>::type...>;
};

/// @brief The tag a reply kind of another layer (the dialogue's
/// `ack_nak<...>`, `two_phase<...>`) names as `lumex_protocol_reply`.
struct reply_tag
{
};

/// @brief Whether `T` is a reply kind (it names `lumex_protocol_reply`).
template <typename T, typename = void> struct is_reply_kind : std::false_type
{
};

/// @copydoc is_reply_kind
template <typename T>
struct is_reply_kind<
    T, typename detail::row_voider<typename T::lumex_protocol_reply>::type>
    : std::is_same<typename T::lumex_protocol_reply, reply_tag>
{
};

/**
 * @brief `reply_of<...>::type`: the reply type of a reply group: `no_reply`
 * for an empty group, the type (a vocabulary name resolved) for one type.
 */
template <typename... T> struct reply_of
{
  static_assert (sizeof...(T) <= 1,
                 "LUMEX_PROTOCOL: a reply group holds at most one type (a "
                 "field, a layout or a reply kind)");
  /// Not used: the group is invalid.
  using type = no_reply;
};

/// @copydoc reply_of
template <> struct reply_of<>
{
  /// No reply.
  using type = no_reply;
};

/// @copydoc reply_of
template <typename T> struct reply_of<T>
{
  /// The reply.
  using type = typename ::lumex::core::protocol::field::resolve<T>::type;
};

namespace detail
{
// The checks of one type of a group of Owner (a row or a layout), in the
// order the macros report them; a later check is not instantiated when an
// earlier one fails (Owner is incomplete while it is checked).
template <typename B>
struct group_not : std::integral_constant<bool, !B::value>
{
};

template <bool... B> struct group_bools
{
};

template <bool... B>
struct group_all
    : std::is_same<group_bools<true, B...>, group_bools<B..., true>>
{
};

template <typename T, template <typename> class Accept,
          template <typename, typename> class IsWord>
struct accept_unless_word
    : std::conditional<IsWord<T, void>::value, std::true_type, Accept<T>>::type
{
};

template <typename T, typename Owner, template <typename> class Accept,
          template <typename, typename> class IsWord>
struct group_accepts
    : std::conditional<std::is_same<T, Owner>::value, std::true_type,
                       accept_unless_word<T, Accept, IsWord>>::type
{
};

template <typename T>
struct field_accept : ::lumex::core::protocol::field::is_field<T>
{
};

template <typename T>
struct reply_accept
    : std::integral_constant<
          bool, std::is_same<T, no_reply>::value
                    || ::lumex::core::protocol::field::is_field<T>::value
                    || is_reply_kind<T>::value>
{
};
} // namespace detail

/**
 * @brief The checks of the field or reply group of a row or layout `Owner`:
 * `no_owner` (the group does not name the owner itself), `no_word` (no word
 * of the vocabulary where a field belongs), `all_fields` / `all_replies`.
 * @details `IsWord` is `is_vocabulary_word` (a parameter, so that this header
 * does not need the vocabulary).
 */
template <typename Owner, template <typename, typename> class IsWord,
          typename... T>
struct group_check
{
  /// No element is the owner.
  using no_owner = detail::group_all<!std::is_same<T, Owner>::value...>;
  /// No element is a word of the vocabulary.
  using no_word = detail::group_all<
      std::conditional<std::is_same<T, Owner>::value, std::true_type,
                       detail::group_not<IsWord<T, void>>>::type::value...>;
  /// Every element is a field.
  using all_fields = detail::group_all<
      detail::group_accepts<T, Owner, detail::field_accept, IsWord>::value...>;
  /// Every element is a field, a layout or a reply kind.
  using all_replies = detail::group_all<
      detail::group_accepts<T, Owner, detail::reply_accept, IsWord>::value...>;
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
  using type = typename P::template lumex_resolve<P>::type;
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
