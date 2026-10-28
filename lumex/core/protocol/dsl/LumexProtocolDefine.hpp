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
 * @file LumexProtocolDefine.hpp
 * @brief The table macros: `LUMEX_PROTOCOL_DEFINE`, `LUMEX_PROTOCOL_EXTEND`,
 * `LUMEX_PROTOCOL_EXTEND_FRAMED` and `LUMEX_PROTOCOL_LAYOUT`.
 * @details A protocol is one X-macro list that takes two row macros:
 * @code
 * #define WIDGET_ROWS(X, X_AS) \
 *   X (PING, 0x01, "Liveness check", (), (), (), (idempotent)) \
 *   X (SET_RATE, 0x02, "Set the sample rate", \
 *      (u16_le_<scale<std::ratio<1, 100> > >), ("rate"), (state_t), \
 *      (timeout (500), retries (2))) \
 *   X_AS (ERASE_LOG, "DELETE", 0x03, "Erase the log", (), (), (state_t), \
 *         (dangerous))
 * LUMEX_PROTOCOL_DEFINE (widget_v1, my_codec, my_header, WIDGET_ROWS);
 * @endcode
 * `X (id, key, description, (fields...), (field names...), (reply),
 * (options...))`; `X_AS` takes the specification name after `id`, for a
 * name that is not a valid identifier or that collides with a platform
 * macro. Every group is parenthesized and may hold any number of elements
 * with any commas inside them. Each row gives a command type `id_t` (fields,
 * reply, key, options, names, description) and a method `id (args...)` of
 * the client mixin `lumex_methods<Client>`. The macros only name things;
 * the templates of `LumexProtocolRow.hpp` and `LumexProtocolOptions.hpp` do
 * the work. Every macro is used with a semicolon after it.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_DEFINE_HPP
#define LUMEX_CORE_PROTOCOL_DSL_DEFINE_HPP

#include <utility>

#include "lumex/core/protocol/dsl/LumexProtocolKey.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolPlatformMacros.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolVocabulary.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/// @cond LUMEX_INTERNAL
// The row identifier is never passed on to another macro as it is (a macro
// argument is expanded before it is substituted, and a platform macro of
// that name would be expanded): only id_##_t and #id_ are passed on.

// The names array starts with a null pointer, so that an empty name group
// gives a valid array; names_after_first skips it.
#define LUMEX_PROTOCOL_DETAIL_NAMES(names_)                                   \
  static ::lumex::core::protocol::dsl::names_t field_names () LUMEX_NOEXCEPT  \
  {                                                                           \
    static char const *const lumex_names[]                                    \
        = { nullptr, LUMEX_PP_UNWRAP names_ };                                \
    return ::lumex::core::protocol::dsl::detail::names_after_first (          \
        lumex_names);                                                         \
  }

// The checks of a field group of an owner (a row or a layout); the messages
// name the owner.
#define LUMEX_PROTOCOL_DETAIL_GROUP_CHECKS(owner_text_, fields_check_)        \
  static_assert (fields_check_::no_owner::value,                              \
                 "LUMEX_PROTOCOL: a field of the " owner_text_                \
                 " names its own type (a layout of that name is hidden by "   \
                 "the type the row or layout makes): rename the layout or "   \
                 "write it with its namespace (::my_namespace::name_t)");     \
  static_assert (fields_check_::no_word::value,                               \
                 "LUMEX_PROTOCOL: a field group of the " owner_text_          \
                 " names a word of the table vocabulary (an option such as "  \
                 "stream or timeout, level, unit), which hides a type of "    \
                 "that name of your namespace: write the type with its "      \
                 "namespace (::my_namespace::stream) or rename it");          \
  static_assert (fields_check_::all_fields::value,                            \
                 "LUMEX_PROTOCOL: every element of a field group must be a "  \
                 "field type (u8_, u16_le_<...>, a LUMEX_PROTOCOL_LAYOUT, "   \
                 "...)")

#define LUMEX_PROTOCOL_DETAIL_ROW_BODY(type_, id_text_, expanded_, spec_,     \
                                       key_, desc_, fields_, names_, reply_,  \
                                       options_)                              \
  struct type_                                                                \
  {                                                                           \
    struct lumex_row : lumex_vocabulary                                       \
    {                                                                         \
      using raw_fields_type                                                   \
          = ::lumex::core::protocol::dsl::type_list<LUMEX_PP_UNWRAP fields_>; \
      using raw_reply_type                                                    \
          = ::lumex::core::protocol::dsl::type_list<LUMEX_PP_UNWRAP reply_>;  \
      using fields_type = ::lumex::core::protocol::dsl::canonical_fields<     \
          LUMEX_PP_UNWRAP fields_>::type;                                     \
      using reply_type = ::lumex::core::protocol::dsl::reply_of<              \
          LUMEX_PP_UNWRAP reply_>::type;                                      \
      using key_type                                                          \
          = decltype (::lumex::core::protocol::dsl::make_key (key_));         \
      using options_type                                                      \
          = decltype (::lumex::core::protocol::dsl::make_options (            \
              LUMEX_PP_UNWRAP options_));                                     \
      static LUMEX_CONSTEXPR key_type                                         \
      make_key_value () LUMEX_NOEXCEPT                                        \
      {                                                                       \
        return ::lumex::core::protocol::dsl::make_key (key_);                 \
      }                                                                       \
      static LUMEX_CONSTEXPR options_type                                     \
      make_options_value () LUMEX_NOEXCEPT                                    \
      {                                                                       \
        return ::lumex::core::protocol::dsl::make_options (                   \
            LUMEX_PP_UNWRAP options_);                                        \
      }                                                                       \
    };                                                                        \
    using lumex_protocol_command = ::lumex::core::protocol::dsl::command_tag; \
    using fields_type = lumex_row::fields_type;                               \
    using reply_type = lumex_row::reply_type;                                 \
    using key_type = lumex_row::key_type;                                     \
    using options_type = lumex_row::options_type;                             \
    static LUMEX_CONSTEXPR char const *                                       \
    identifier () LUMEX_NOEXCEPT                                              \
    {                                                                         \
      return id_text_;                                                        \
    }                                                                         \
    static LUMEX_CONSTEXPR char const *                                       \
    specification_name () LUMEX_NOEXCEPT                                      \
    {                                                                         \
      return spec_;                                                           \
    }                                                                         \
    static LUMEX_CONSTEXPR char const *                                       \
    description () LUMEX_NOEXCEPT                                             \
    {                                                                         \
      return desc_;                                                           \
    }                                                                         \
    static LUMEX_CONSTEXPR key_type                                           \
    key () LUMEX_NOEXCEPT                                                     \
    {                                                                         \
      return lumex_row::make_key_value ();                                    \
    }                                                                         \
    static LUMEX_CONSTEXPR options_type                                       \
    options () LUMEX_NOEXCEPT                                                 \
    {                                                                         \
      return lumex_row::make_options_value ();                                \
    }                                                                         \
    LUMEX_PROTOCOL_DETAIL_NAMES (names_)                                      \
    LUMEX_PROTOCOL_DETAIL_CHECK_TEXT (id_text_, expanded_);                   \
    using lumex_fields_check = ::lumex::core::protocol::dsl::group_check_of<  \
        type_, lumex_row::raw_fields_type>;                                   \
    using lumex_reply_check = ::lumex::core::protocol::dsl::group_check_of<   \
        type_, lumex_row::raw_reply_type>;                                    \
    LUMEX_PROTOCOL_DETAIL_GROUP_CHECKS ("row " id_text_, lumex_fields_check); \
    static_assert (                                                           \
        lumex_reply_check::no_owner::value,                                   \
        "LUMEX_PROTOCOL: the reply of the row " id_text_                      \
        " names the row's own type " id_text_ "_t (a layout of that name is " \
        "hidden inside the row): rename the layout or write it with its "     \
        "namespace (::my_namespace::" id_text_ "_t)");                        \
    static_assert (                                                           \
        lumex_reply_check::no_word::value,                                    \
        "LUMEX_PROTOCOL: the reply of the row " id_text_                      \
        " names a word of the table vocabulary (an option such as stream "    \
        "or timeout, level, unit), which hides a type of that name of your "  \
        "namespace: write the type with its namespace "                       \
        "(::my_namespace::stream) or rename it");                             \
    static_assert (                                                           \
        lumex_reply_check::all_replies::value,                                \
        "LUMEX_PROTOCOL: the reply of the row " id_text_                      \
        " is not a field, a layout or a reply kind (a type naming "           \
        "lumex_protocol_reply)");                                             \
    static_assert (                                                           \
        ::lumex::core::protocol::dsl::count_names (LUMEX_PP_UNWRAP names_)    \
            == fields_type::count::value,                                     \
        "LUMEX_PROTOCOL: the row " id_text_                                   \
        " has a different number of field names than fields");                \
  };

#define LUMEX_PROTOCOL_DETAIL_ROW(id_, key_, desc_, fields_, names_, reply_,  \
                                  options_)                                   \
  LUMEX_PROTOCOL_DETAIL_ROW_BODY (                                            \
      id_##_t, #id_, LUMEX_PROTOCOL_DETAIL_EXPANDED (id_), #id_, key_, desc_, \
      fields_, names_, reply_, options_)
#define LUMEX_PROTOCOL_DETAIL_ROW_AS(id_, spec_, key_, desc_, fields_,        \
                                     names_, reply_, options_)                \
  LUMEX_PROTOCOL_DETAIL_ROW_BODY (                                            \
      id_##_t, #id_, LUMEX_PROTOCOL_DETAIL_EXPANDED (id_), spec_, key_,       \
      desc_, fields_, names_, reply_, options_)

// One `, typename Layer::id_t` per row, after a leading void (list_of).
#define LUMEX_PROTOCOL_DETAIL_RESOLVE(id_, key_, desc_, fields_, names_,      \
                                      reply_, options_)                       \
  , typename LumexLayer::id_##_t
#define LUMEX_PROTOCOL_DETAIL_RESOLVE_AS(id_, spec_, key_, desc_, fields_,    \
                                         names_, reply_, options_)            \
  , typename LumexLayer::id_##_t

// One client method per row, named by the bare identifier (the only place
// where it is not pasted or stringified: the reason for X_AS and the check).
#define LUMEX_PROTOCOL_DETAIL_METHOD_DECLARATION(name_, type_)                \
  template <typename... LumexArgs>                                            \
  auto name_ (LumexArgs &&...lumex_args)                                      \
      -> decltype (::lumex::core::protocol::dsl::invoke_row<type_> (          \
          std::declval<LumexClient &> (),                                     \
          std::forward<LumexArgs> (lumex_args)...))                           \
  {                                                                           \
    return ::lumex::core::protocol::dsl::invoke_row<type_> (                  \
        static_cast<LumexClient &> (*this),                                   \
        std::forward<LumexArgs> (lumex_args)...);                             \
  }
#define LUMEX_PROTOCOL_DETAIL_METHOD(id_, key_, desc_, fields_, names_,       \
                                     reply_, options_)                        \
  LUMEX_PROTOCOL_DETAIL_METHOD_DECLARATION (id_, id_##_t)
#define LUMEX_PROTOCOL_DETAIL_METHOD_AS(id_, spec_, key_, desc_, fields_,     \
                                        names_, reply_, options_)             \
  LUMEX_PROTOCOL_DETAIL_METHOD_DECLARATION (id_, id_##_t)

#define LUMEX_PROTOCOL_DETAIL_LAYER(name_text_, type_, base_, framing_,       \
                                    header_, LIST_)                           \
  struct type_ : base_                                                        \
  {                                                                           \
    using framing_type = framing_;                                            \
    using header_type = header_;                                              \
    using base_protocol = base_;                                              \
    using lumex_vocabulary                                                    \
        = ::lumex::core::protocol::dsl::vocabulary_of<header_>::type;         \
    static LUMEX_CONSTEXPR char const *                                       \
    protocol_name () LUMEX_NOEXCEPT                                           \
    {                                                                         \
      return name_text_;                                                      \
    }                                                                         \
    LIST_ (LUMEX_PROTOCOL_DETAIL_ROW, LUMEX_PROTOCOL_DETAIL_ROW_AS)           \
    template <typename LumexLayer> struct lumex_resolve                       \
    {                                                                         \
      using type = typename ::lumex::core::protocol::dsl::append_unique<      \
          typename base_::template lumex_resolve<LumexLayer>::type,           \
          typename ::lumex::core::protocol::dsl::list_of<void LIST_ (         \
              LUMEX_PROTOCOL_DETAIL_RESOLVE,                                  \
              LUMEX_PROTOCOL_DETAIL_RESOLVE_AS)>::type>::type;                \
    };                                                                        \
    template <typename LumexClient>                                           \
    struct lumex_methods : base_::template lumex_methods<LumexClient>         \
    {                                                                         \
      LIST_ (LUMEX_PROTOCOL_DETAIL_METHOD, LUMEX_PROTOCOL_DETAIL_METHOD_AS)   \
    };                                                                        \
  }
/// @endcond

/**
 * @brief Defines protocol `name_`: a struct with the command type of every
 * row of `LIST_`, the codec and header types, and the client mixin.
 * @param name_ The protocol type (`protocol_name ()` is its spelling).
 * @param framing_ The frame codec type (stored as `framing_type`).
 * @param header_ The message header type (stored as `header_type`).
 * @param LIST_ The row list macro `LIST_ (X, X_AS)`.
 */
#define LUMEX_PROTOCOL_DEFINE(name_, framing_, header_, LIST_)                \
  struct name_                                                                \
  {                                                                           \
    using lumex_protocol_kind = ::lumex::core::protocol::dsl::protocol_tag;   \
    using framing_type = framing_;                                            \
    using header_type = header_;                                              \
    using base_protocol = void;                                               \
    using lumex_vocabulary                                                    \
        = ::lumex::core::protocol::dsl::vocabulary_of<header_>::type;         \
    static LUMEX_CONSTEXPR char const *                                       \
    protocol_name () LUMEX_NOEXCEPT                                           \
    {                                                                         \
      return #name_;                                                          \
    }                                                                         \
    LIST_ (LUMEX_PROTOCOL_DETAIL_ROW, LUMEX_PROTOCOL_DETAIL_ROW_AS)           \
    template <typename LumexLayer> struct lumex_resolve                       \
    {                                                                         \
      using type =                                                            \
          typename ::lumex::core::protocol::dsl::list_of<void LIST_ (         \
              LUMEX_PROTOCOL_DETAIL_RESOLVE,                                  \
              LUMEX_PROTOCOL_DETAIL_RESOLVE_AS)>::type;                       \
    };                                                                        \
    template <typename LumexClient> struct lumex_methods                      \
    {                                                                         \
      LIST_ (LUMEX_PROTOCOL_DETAIL_METHOD, LUMEX_PROTOCOL_DETAIL_METHOD_AS)   \
    };                                                                        \
  }

/**
 * @brief Defines layer `name_` of protocol `base_` (family, device, model,
 * firmware range): the rows of `LIST_` replace the rows of the base with the
 * same identifier and add the others.
 * @details The layer derives from `base_`, so a row it does not override is
 * the base's type (`name_::PING_t` is `base_::PING_t`). The codec and header
 * types are those of the base.
 */
#define LUMEX_PROTOCOL_EXTEND(name_, base_, LIST_)                            \
  LUMEX_PROTOCOL_DETAIL_LAYER (#name_, name_, base_, base_::framing_type,     \
                               base_::header_type, LIST_)

/// @brief `LUMEX_PROTOCOL_EXTEND` with another codec and header type (the
/// same commands over another encoding).
#define LUMEX_PROTOCOL_EXTEND_FRAMED(name_, base_, framing_, header_, LIST_)  \
  LUMEX_PROTOCOL_DETAIL_LAYER (#name_, name_, base_, framing_, header_, LIST_)

/**
 * @brief Declares layout `name_`: a named group of fields, written once and
 * used as a field or a reply in any row (`(name_)`).
 * @param name_ The layout type.
 * @param fields_ `(field, ...)`: any number of fields.
 * @param names_ `("name", ...)`: one name per field.
 */
#define LUMEX_PROTOCOL_LAYOUT(name_, fields_, names_)                         \
  struct name_                                                                \
  {                                                                           \
    struct lumex_row : ::lumex::core::protocol::dsl::vocabulary               \
    {                                                                         \
      using raw_fields_type                                                   \
          = ::lumex::core::protocol::dsl::type_list<LUMEX_PP_UNWRAP fields_>; \
      using fields_type = ::lumex::core::protocol::dsl::canonical_fields<     \
          LUMEX_PP_UNWRAP fields_>::type;                                     \
    };                                                                        \
    using lumex_protocol_field = ::lumex::core::protocol::field::field_tag;   \
    using lumex_protocol_layout = ::lumex::core::protocol::dsl::layout_tag;   \
    using fields_type = lumex_row::fields_type;                               \
    static LUMEX_CONSTEXPR char const *                                       \
    layout_name () LUMEX_NOEXCEPT                                             \
    {                                                                         \
      return #name_;                                                          \
    }                                                                         \
    LUMEX_PROTOCOL_DETAIL_NAMES (names_)                                      \
    using lumex_fields_check = ::lumex::core::protocol::dsl::group_check_of<  \
        name_, lumex_row::raw_fields_type>;                                   \
    LUMEX_PROTOCOL_DETAIL_GROUP_CHECKS ("layout " #name_,                     \
                                        lumex_fields_check);                  \
    static_assert (                                                           \
        ::lumex::core::protocol::dsl::count_names (LUMEX_PP_UNWRAP names_)    \
            == fields_type::count::value,                                     \
        "LUMEX_PROTOCOL: the layout " #name_                                  \
        " has a different number of field names than fields");                \
  }

#endif // !LUMEX_CORE_PROTOCOL_DSL_DEFINE_HPP
