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
 * @file LumexProtocolIndex.hpp
 * @brief The run-time index of a protocol layer: `protocol_index_t`, `row_t`,
 * `build_index<P, Registries...> ()`, `protocol_index<P, Registries...> ()`,
 * `keys_unique<P>` and `describe`.
 * @details The index lists, per protocol layer, the rows (commands with their
 * replies, and events: the rows with a `stream` option), the registries
 * attached to it, and for every row its names, description, key, field
 * descriptors, reply and options. It finds a row by key, by name or by a
 * received frame (the layout of the layer gives the key, see
 * `LumexProtocolKeyReader.hpp`), and `describe` turns a frame into the log
 * line:
 * @code
 * TX widget_v2.SET_SPEED (Set the speed) speed=12.5 rpm
 * RX widget_v2.STATUS (Read the state) mode=run level=3
 * TX unknown key=0x15 len=7: 15 01 02 03 04 05 06
 * @endcode
 * The keys of the table are cut to the width of the wire
 * (`key_width_of<P>`); two rows with the same key on the wire are refused:
 * at compile time for integer keys (`keys_unique<P>`, asserted by
 * `protocol_index<P>`), and by `build_index` with a message naming both rows
 * for every kind (integer, text, the decimal text of a tokenized layout,
 * names, the keys of a registry). Everything the index points to is static
 * data of the program; a text key found in a frame points into the frame.
 */
#ifndef LUMEX_CORE_PROTOCOL_INDEX_INDEX_HPP
#define LUMEX_CORE_PROTOCOL_INDEX_INDEX_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "lumex/core/expected/Expected"
#include "lumex/core/protocol/dsl/LumexProtocolEntry.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolKey.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/index/LumexProtocolDescriptor.hpp"
#include "lumex/core/protocol/index/LumexProtocolFormat.hpp"
#include "lumex/core/protocol/index/LumexProtocolKeyReader.hpp"
#include "lumex/core/protocol/index/LumexProtocolSession.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolTextLayouts.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryText.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace index
{
/// @brief Which way a frame goes.
enum class frame_direction : std::uint8_t
{
  /// To the device: a request (`TX`).
  tx,
  /// From the device: a reply or an event (`RX`).
  rx
};

/// @brief What the reply of a row is.
enum class reply_form : std::uint8_t
{
  /// The row has no reply.
  none,
  /// One field.
  single_field,
  /// A layout: its fields are the reply.
  layout,
  /// A reply kind of the dialogue (`ack_nak<...>`): the dialogue decodes it;
  /// the kind may describe its message for the log (see `row_t`).
  dialogue
};

/// @brief Decodes the payload of a row in one direction and appends its
/// fields as text to `out`.
using describe_hook_t = std::error_code (*) (
    ::lumex::core::protocol::layout::bytes_view_t payload,
    ::lumex::core::protocol::field::decode_context_t const *context,
    describe_options_t const &options, std::string &out);

/// @brief Appends the text a tokenized layout writes for the key of a row.
using text_key_hook_t = void (*) (std::string &out);

/// @brief One row of the index: a command with its reply, or an event.
struct row_t
{
  /// The names, description, key and options of the row.
  ::lumex::core::protocol::dsl::command_entry_t const *entry;
  /// The key cut to the width of the wire (`key_width_of`).
  ::lumex::core::protocol::dsl::key_value_t key;
  /// The descriptors of the request fields.
  field_descriptor_t const *request;
  /// The number of request fields.
  std::size_t request_count;
  /// What the reply is.
  reply_form reply;
  /// The name of the layout of the reply, `""` when it is not a layout.
  char const *reply_layout;
  /// The descriptors of the reply: the fields of a layout, or the one field
  /// (named `"reply"`); none otherwise.
  field_descriptor_t const *reply_fields;
  /// The number of reply descriptors.
  std::size_t reply_count;
  /// Whether the device sends the row by itself (it has a `stream` option).
  bool is_event;
  /// Decodes a request payload.
  describe_hook_t describe_request;
  /// Decodes a reply payload; for a dialogue reply the kind's own
  /// `lumex_describe_reply<P, Row>`, null when the kind has none.
  describe_hook_t describe_reply;
  /// The text key of a tokenized layout, null for another layout.
  text_key_hook_t text_key;
  /// What a dialogue reply is, for the export: the kind's own
  /// `lumex_reply_text ()` (`"ack_nak (ACK: one field; NAK: message `NAK`)"`),
  /// `""` for another reply or a kind without it.
  char const *reply_kind;
};

/// @brief A registry attached to the index.
struct registry_info_t
{
  /// The name of the registry.
  char const *name;
  /// Its entries.
  ::lumex::core::protocol::registry::entries_t entries;
};

/// @brief Why an index could not be built.
struct index_error_t
{
  /// `protocol_errc::duplicate_key`.
  std::error_code code;
  /// What is wrong: the rows or entries and the key.
  std::string message;
};

/// @brief The result of looking a frame up.
struct frame_match_t
{
  /// The row, or `nullptr` when the key matches none (or none was read).
  row_t const *row;
  /// Whether a key could be read from the frame.
  bool has_key;
  /// The key as the frame carries it (a text key points into the frame).
  ::lumex::core::protocol::dsl::key_value_t key;
  /// Where the fields start in the frame.
  std::size_t payload_offset;
};

/// @brief The reader of the key of the frames: false when the frame has no
/// readable key.
using key_read_hook_t
    = bool (*) (::lumex::core::protocol::layout::bytes_view_t frame,
                key_read_t &out);

class protocol_index_t;

/// @brief The result of building an index.
using index_result_t
    = ::lumex::core::expected::result::expected<protocol_index_t,
                                                index_error_t>;

namespace detail
{
namespace layout = ::lumex::core::protocol::layout;

template <typename P, typename Row>
std::error_code
describe_request_of (layout::bytes_view_t payload,
                     field::decode_context_t const *context,
                     describe_options_t const &options, std::string &out)
{
  layout::decode_result_t<typename layout::request_of<Row>::type> const decoded
      = layout::decode_request<P, Row> (payload, context);
  if (!decoded)
    return decoded.error ();
  append_record (out, *decoded, Row::field_names (), options);
  return std::error_code ();
}

// A row without a reply is checked as a message without fields.
template <typename Row> struct empty_reply_row
{
  using fields_type = dsl::field_list<>;

  static LUMEX_CONSTEXPR typename Row::key_type
  key () LUMEX_NOEXCEPT
  {
    return Row::key ();
  }
};

template <typename P, typename Row>
std::error_code
describe_empty_reply (layout::bytes_view_t payload,
                      field::decode_context_t const *context,
                      describe_options_t const &, std::string &)
{
  layout::decode_result_t<field::record<>> const decoded
      = layout::decode_request<P, empty_reply_row<Row>> (payload, context);
  return decoded ? std::error_code () : decoded.error ();
}

template <typename P, typename Row>
std::error_code
describe_reply_of (layout::bytes_view_t payload,
                   field::decode_context_t const *context,
                   describe_options_t const &options, std::string &out)
{
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<P, Row> (payload, context);
  if (!decoded)
    return decoded.error ();
  append_value<typename Row::reply_type> (out, *decoded, options);
  return std::error_code ();
}

// The descriptors of a reply: the fields of a layout, or the one field.
template <typename R, bool Group = field::detail::has_fields<R>::value>
struct reply_run
{
  static descriptor_run_t
  get ()
  {
    return owned_run<descriptor_role::none, R,
                     typename R::fields_type>::get ();
  }
};

template <typename R> struct reply_run<R, false>
{
  static descriptor_run_t
  get ()
  {
    static field_descriptor_t const items[2]
        = { make_descriptor_impl<R> ("reply", descriptor_role::none, 0),
            field_descriptor_t () };
    descriptor_run_t const run = { items, 1 };
    return run;
  }
};

// 0: no reply, 1: a field or a layout, 2: a reply kind of the dialogue.
template <typename Row>
struct reply_kind_of
    : std::integral_constant<
          int,
          std::is_same<typename Row::reply_type, dsl::no_reply>::value
              ? 0
              : (field::is_field<typename Row::reply_type>::value ? 1 : 2)>
{
};

// The hooks a reply kind of another layer may give the index, found by
// their names: a member of that name the index cannot use does not compile.
LUMEX_PROTOCOL_DETAIL_NAME_PROBE (lumex_describe_reply);
LUMEX_PROTOCOL_DETAIL_NAME_PROBE (lumex_reply_text);

template <typename P, typename Row, typename R, typename = void>
struct callable_describer_t : std::false_type
{
};

template <typename P, typename Row, typename R>
struct callable_describer_t<
    P, Row, R,
    typename dsl::detail::row_voider<decltype (static_cast<describe_hook_t> (
        &R::template lumex_describe_reply<P, Row>))>::type> : std::true_type
{
};

template <typename P, typename Row, typename R, bool Callable>
struct describer_pointer_t
{
  static describe_hook_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename Row, typename R>
struct describer_pointer_t<P, Row, R, true>
{
  static describe_hook_t
  get () LUMEX_NOEXCEPT
  {
    return static_cast<describe_hook_t> (
        &R::template lumex_describe_reply<P, Row>);
  }
};

// The describe hook of reply kind R of Row: the hook when the index can use
// it (a final class too), a refusal of a member of that name it cannot use,
// null without one (2, 1, 0). The name of a member of a final class cannot
// be probed: an unusable member there is ignored.
template <typename P, typename Row, typename R = typename Row::reply_type,
          int Form = callable_describer_t<P, Row, R>::value
                         ? 2
                         : (names_lumex_describe_reply_t<R>::value ? 1 : 0)>
struct kind_describer_t
{
  static describe_hook_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename Row, typename R>
struct kind_describer_t<P, Row, R, 1>
{
  static_assert (callable_describer_t<P, Row, R>::value,
                 "LUMEX_PROTOCOL: the reply kind names lumex_describe_reply, "
                 "but not as an accessible static member template "
                 "lumex_describe_reply<P, Row> (bytes_view_t payload, "
                 "decode_context_t const *, describe_options_t const &, "
                 "std::string &) that returns std::error_code");

  static describe_hook_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename Row, typename R>
struct kind_describer_t<P, Row, R, 2>
{
  static describe_hook_t
  get () LUMEX_NOEXCEPT
  {
    return describer_pointer_t<P, Row, R, true>::get ();
  }
};

template <typename R, typename = void>
struct callable_reply_text_t : std::false_type
{
};

template <typename R>
struct callable_reply_text_t<
    R, typename std::enable_if<std::is_convertible<
           decltype (R::lumex_reply_text ()), char const *>::value>::type>
    : std::true_type
{
};

template <typename R, bool Callable> struct reply_text_call_t
{
  static char const *
  get ()
  {
    return "";
  }
};

template <typename R> struct reply_text_call_t<R, true>
{
  static char const *
  get ()
  {
    return R::lumex_reply_text ();
  }
};

// The text of reply kind R for the export, as the describe hook: 2 usable,
// 1 named but unusable (refused), 0 none ("").
template <typename R,
          int Form = callable_reply_text_t<R>::value
                         ? 2
                         : (names_lumex_reply_text_t<R>::value ? 1 : 0)>
struct kind_text_t
{
  static char const *
  get ()
  {
    return "";
  }
};

template <typename R> struct kind_text_t<R, 1>
{
  static_assert (callable_reply_text_t<R>::value,
                 "LUMEX_PROTOCOL: the reply kind names lumex_reply_text, but "
                 "not as an accessible static function lumex_reply_text () "
                 "that returns char const *");

  static char const *
  get ()
  {
    return "";
  }
};

template <typename R> struct kind_text_t<R, 2>
{
  static char const *
  get ()
  {
    return reply_text_call_t<R, true>::get ();
  }
};

template <typename P, typename Row, int Kind> struct reply_support;

template <typename P, typename Row> struct reply_support<P, Row, 0>
{
  static reply_form
  form () LUMEX_NOEXCEPT
  {
    return reply_form::none;
  }

  static describe_hook_t
  hook () LUMEX_NOEXCEPT
  {
    return &describe_empty_reply<P, Row>;
  }

  static descriptor_run_t
  fields () LUMEX_NOEXCEPT
  {
    descriptor_run_t const none = { nullptr, 0 };
    return none;
  }

  static char const *
  layout_name () LUMEX_NOEXCEPT
  {
    return "";
  }

  static char const *
  kind_text ()
  {
    return "";
  }
};

template <typename P, typename Row> struct reply_support<P, Row, 1>
{
  static reply_form
  form () LUMEX_NOEXCEPT
  {
    return dsl::is_layout<typename Row::reply_type>::value
               ? reply_form::layout
               : reply_form::single_field;
  }

  static describe_hook_t
  hook () LUMEX_NOEXCEPT
  {
    return &describe_reply_of<P, Row>;
  }

  static descriptor_run_t
  fields ()
  {
    return reply_run<typename Row::reply_type>::get ();
  }

  static char const *
  layout_name () LUMEX_NOEXCEPT
  {
    return layout_name_of<typename Row::reply_type>::get ();
  }

  static char const *
  kind_text ()
  {
    return "";
  }
};

template <typename P, typename Row> struct reply_support<P, Row, 2>
{
  static reply_form
  form () LUMEX_NOEXCEPT
  {
    return reply_form::dialogue;
  }

  static describe_hook_t
  hook () LUMEX_NOEXCEPT
  {
    return kind_describer_t<P, Row>::get ();
  }

  static descriptor_run_t
  fields () LUMEX_NOEXCEPT
  {
    descriptor_run_t const none = { nullptr, 0 };
    return none;
  }

  static char const *
  layout_name () LUMEX_NOEXCEPT
  {
    return "";
  }

  static char const *
  kind_text ()
  {
    return kind_text_t<typename Row::reply_type>::get ();
  }
};

template <typename L> struct is_tokenized_layout : std::false_type
{
};

template <char Separator, char KeySeparator, unsigned Radix, bool Trim>
struct is_tokenized_layout<
    layout::tokenized_text_layout<Separator, KeySeparator, Radix, Trim>>
    : std::true_type
{
};

template <typename Row>
void
write_text_key (std::string &out)
{
  layout::detail::key_text const key (Row::key ());
  out.assign (key.data, key.size);
}

template <
    typename P, typename Row,
    bool Tokenized = is_tokenized_layout<
        typename ::lumex::core::protocol::layout::layout_of<P>::type>::value>
struct text_key_support
{
  static text_key_hook_t
  hook () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename Row> struct text_key_support<P, Row, true>
{
  static text_key_hook_t
  hook () LUMEX_NOEXCEPT
  {
    return &write_text_key<Row>;
  }
};

template <typename P, typename Row>
row_t
make_row (dsl::command_entry_t const &entry)
{
  using reply = reply_support<P, Row, reply_kind_of<Row>::value>;
  descriptor_run_t const request = request_descriptors<Row> ();
  descriptor_run_t const reply_fields = reply::fields ();
  row_t const row = { &entry,
                      dsl::normalize_key (entry.key, key_width_of<P>::value),
                      request.items,
                      request.count,
                      reply::form (),
                      reply::layout_name (),
                      reply_fields.items,
                      reply_fields.count,
                      entry.options.has_stream,
                      &describe_request_of<P, Row>,
                      reply::hook (),
                      text_key_support<P, Row>::hook (),
                      reply::kind_text () };
  return row;
}

template <typename P, typename List, typename Indices> struct row_table;

template <typename P, typename... Rows, std::size_t... I>
struct row_table<P, dsl::type_list<Rows...>,
                 ::lumex::core::utility::sequence::index_sequence<I...>>
{
  static ::lumex::core::span::view::span<row_t const>
  get ()
  {
    // One row more than commands: an empty protocol still has an array.
    static row_t const rows[sizeof...(Rows) + 1]
        = { make_row<P, Rows> (dsl::command_entries<P> ().data ()[I])...,
            row_t () };
    return ::lumex::core::span::view::span<row_t const> (rows,
                                                         sizeof...(Rows));
  }
};

template <typename P, typename Enable = void> struct base_name_of
{
  static char const *
  get () LUMEX_NOEXCEPT
  {
    return "";
  }
};

template <typename P>
struct base_name_of<P, typename std::enable_if<!std::is_void<
                           typename P::base_protocol>::value>::type>
{
  static char const *
  get () LUMEX_NOEXCEPT
  {
    return P::base_protocol::protocol_name ();
  }
};

// The check a vocabulary may ask the index to make of a protocol whose
// tables use it (the dialogue checks its row and protocol options): found by
// its name, refused when it is not a member template with a constant.
LUMEX_PROTOCOL_DETAIL_NAME_PROBE (lumex_check_protocol);

template <typename V, typename P, typename = void>
struct callable_check_t : std::false_type
{
};

template <typename V, typename P>
struct callable_check_t<
    V, P,
    typename std::enable_if<std::is_same<
        bool,
        typename std::remove_cv<decltype (V::template lumex_check_protocol<
                                          P>::value)>::type>::value>::type>
    : std::true_type
{
};

template <typename V, typename P, bool Callable>
struct check_value_t : std::true_type
{
};

template <typename V, typename P>
struct check_value_t<V, P, true>
    : std::integral_constant<bool, V::template lumex_check_protocol<P>::value>
{
};

// 2: the check is usable, 1: named but unusable (refused), 0: none.
template <typename P, typename V = typename P::lumex_vocabulary,
          int Form = callable_check_t<V, P>::value
                         ? 2
                         : (names_lumex_check_protocol_t<V>::value ? 1 : 0)>
struct vocabulary_accepts_t : std::true_type
{
};

template <typename P, typename V>
struct vocabulary_accepts_t<P, V, 1> : std::true_type
{
  static_assert (callable_check_t<V, P>::value,
                 "LUMEX_PROTOCOL: the vocabulary names lumex_check_protocol, "
                 "but not as an accessible member template "
                 "lumex_check_protocol<P> with a constant bool value");
};

template <typename P, typename V>
struct vocabulary_accepts_t<P, V, 2> : check_value_t<V, P, true>
{
};

// The key of a row as the compile-time check compares it.
struct key_arg
{
  bool integer;
  std::uint64_t bits;
  std::size_t width;
};

LUMEX_CONSTEXPR inline bool
clash (key_arg const &left, key_arg const &right) LUMEX_NOEXCEPT
{
  return left.integer && right.integer && left.bits == right.bits
         && left.width == right.width;
}

LUMEX_CONSTEXPR inline bool
clashes_with_any (key_arg const &) LUMEX_NOEXCEPT
{
  return false;
}

template <typename... Rest>
LUMEX_CONSTEXPR bool
clashes_with_any (key_arg const &first, key_arg const &second,
                  Rest const &...rest) LUMEX_NOEXCEPT
{
  return clash (first, second) || clashes_with_any (first, rest...);
}

LUMEX_CONSTEXPR inline bool
all_distinct () LUMEX_NOEXCEPT
{
  return true;
}

template <typename... Rest>
LUMEX_CONSTEXPR bool
all_distinct (key_arg const &first, Rest const &...rest) LUMEX_NOEXCEPT
{
  return !clashes_with_any (first, rest...) && all_distinct (rest...);
}

template <typename Row,
          bool Integer = std::is_integral<typename Row::key_type>::value
                         || std::is_enum<typename Row::key_type>::value>
struct key_arg_of
{
  static LUMEX_CONSTEXPR key_arg
  get (std::size_t, bool) LUMEX_NOEXCEPT
  {
    return key_arg{ false, 0, 0 };
  }
};

template <typename Row> struct key_arg_of<Row, true>
{
  // On a wire of bytes the key is its low `wire` bytes. In a tokenized
  // layout the wire carries text: a character is itself, any other integer
  // its signed decimal number, so 'A' and 65 differ and 65 and 65 do not
  // whatever the type.
  static LUMEX_CONSTEXPR key_arg
  get (std::size_t wire, bool tokenized) LUMEX_NOEXCEPT
  {
    return tokenized
               ? key_arg{ true,
                          static_cast<std::uint64_t> (
                              static_cast<std::int64_t> (Row::key ())),
                          std::is_same<typename Row::key_type, char>::value
                              ? 1u
                              : 2u }
               : key_arg{ true,
                          dsl::detail::key_bits<typename Row::key_type>::get (
                              Row::key ())
                              & dsl::width_mask (wire),
                          wire };
  }
};

// Tables above this size are only checked when the index is built: the
// constant evaluation recurses once per row.
LUMEX_CONSTEXPR std::size_t static_key_check_limit = 200;

template <typename List, std::size_t Wire, bool Tokenized> struct unique_in;

template <typename... Rows, std::size_t Wire, bool Tokenized>
struct unique_in<dsl::type_list<Rows...>, Wire, Tokenized>
    : std::integral_constant<bool, (sizeof...(Rows) > static_key_check_limit)
                                       || all_distinct (key_arg_of<Rows>::get (
                                           Wire, Tokenized)...)>{};
} // namespace detail

/**
 * @brief Whether no two integer keys of protocol `P` are the same on the
 * wire (cut to `key_width_of<P>`; in a tokenized layout: the same text, so
 * 'A' and 65 differ), as a constant. Text and composite keys are checked when
 * the index is built; tables of more than 200 rows only then too.
 */
template <typename P>
struct keys_unique
    : detail::unique_in<typename dsl::commands_of<P>::type,
                        key_width_of<P>::value,
                        detail::is_tokenized_layout<
                            typename layout::layout_of<P>::type>::value>
{
};

/// @brief The run-time index of one protocol layer (see the file
/// description).
class protocol_index_t
{
public:
  /// @brief An empty index.
  protocol_index_t ()
      : m_name (""), m_base (""), m_rows (), m_registries (), m_key_width (1),
        m_reader (nullptr)
  {
  }

  /**
   * @brief Builds the index from the rows of a protocol, the registries and
   * the reader of the key (what `build_index<P, ...>` passes).
   * @return The index, or the first duplicate found.
   */
  static index_result_t
  assemble (char const *name, char const *base,
            ::lumex::core::span::view::span<row_t const> rows,
            std::vector<registry_info_t> const &registries,
            std::size_t key_width, key_read_hook_t reader);

  /// @brief The name of the protocol layer (`"widget_v2"`).
  char const *
  protocol_name () const LUMEX_NOEXCEPT
  {
    return m_name;
  }

  /// @brief The name of the layer it extends, `""` for a base protocol.
  char const *
  base_name () const LUMEX_NOEXCEPT
  {
    return m_base;
  }

  /// @brief The rows in table order.
  ::lumex::core::span::view::span<row_t const>
  rows () const LUMEX_NOEXCEPT
  {
    return m_rows;
  }

  /// @brief The registries attached to the index.
  std::vector<registry_info_t> const &
  registries () const LUMEX_NOEXCEPT
  {
    return m_registries;
  }

  /// @brief The width of the key on the wire, in bytes.
  std::size_t
  key_width () const LUMEX_NOEXCEPT
  {
    return m_key_width;
  }

  /// @brief The row with `key` (an integer key is cut to the key width
  /// first), or `nullptr`.
  row_t const *
  find (::lumex::core::protocol::dsl::key_value_t const &key) const;

  /// @brief The row whose integer key has the bits `bits` on the wire.
  row_t const *find_integer (std::uint64_t bits) const;

  /// @brief The row whose key is the text `text` (a mnemonic, or the
  /// decimal text of an integer key in a tokenized layout).
  row_t const *find_text (char const *text, std::size_t size) const;

  /// @brief The row with the specification name or the identifier `name`.
  row_t const *find_name (char const *name) const;

  /// @brief The registry named `name`, or `nullptr`.
  registry_info_t const *find_registry (char const *name) const;

  /// @brief Reads the key of `frame` and finds its row.
  frame_match_t
  match (::lumex::core::protocol::layout::bytes_view_t frame) const;

  /**
   * @brief The log line of `frame` (see the file description).
   * @param session The values identification learned, for the decoders that
   * depend on them; may be `nullptr`.
   */
  std::string describe (::lumex::core::protocol::layout::bytes_view_t frame,
                        frame_direction direction,
                        session_context_t const *session = nullptr,
                        describe_options_t const &options
                        = describe_options_t ()) const;

private:
  char const *m_name;
  char const *m_base;
  ::lumex::core::span::view::span<row_t const> m_rows;
  std::vector<registry_info_t> m_registries;
  std::size_t m_key_width;
  key_read_hook_t m_reader;
  std::unordered_map<std::uint64_t, std::size_t> m_by_integer;
  std::unordered_map<std::string, std::size_t> m_by_text;
  std::unordered_map<std::string, std::size_t> m_by_name;
};

namespace detail
{
inline std::string
key_name (::lumex::core::protocol::dsl::key_value_t const &key)
{
  std::string out;
  if (key.kind == ::lumex::core::protocol::dsl::key_kind::text)
    {
      out += '"';
      out.append (key.text, key.text_size);
      out += '"';
    }
  else
    append_hex (out, key.integer, key.width == 0 ? 2u : 2u * key.width);
  return out;
}

inline index_result_t
duplicate (std::string const &message)
{
  index_error_t error;
  error.code = ::lumex::core::protocol::error::make_error_code (
      ::lumex::core::protocol::error::protocol_errc::duplicate_key);
  error.message = message;
  return ::lumex::core::expected::error::unexpected<index_error_t> (error);
}

inline std::string
row_name (row_t const &row)
{
  return row.entry->identifier;
}
} // namespace detail

inline index_result_t
protocol_index_t::assemble (char const *name, char const *base,
                            ::lumex::core::span::view::span<row_t const> rows,
                            std::vector<registry_info_t> const &registries,
                            std::size_t key_width, key_read_hook_t reader)
{
  namespace dsl = ::lumex::core::protocol::dsl;
  protocol_index_t built;
  built.m_name = name;
  built.m_base = base;
  built.m_rows = rows;
  built.m_registries = registries;
  built.m_key_width = key_width;
  built.m_reader = reader;
  for (std::size_t i = 0; i < rows.size (); ++i)
    {
      row_t const &row = rows.data ()[i];
      std::string const label = "protocol " + std::string (name) + ": rows ";
      if (row.key.kind == dsl::key_kind::integer
          || row.key.kind == dsl::key_kind::composite)
        {
          std::pair<std::unordered_map<std::uint64_t, std::size_t>::iterator,
                    bool> const placed
              = built.m_by_integer.insert (
                  std::make_pair (row.key.integer, i));
          // In a tokenized layout the wire carries the text of the key, which
          // is checked below: 'A' and 65 are two keys there.
          if (!placed.second && row.text_key == nullptr)
            return detail::duplicate (
                label + detail::row_name (rows.data ()[placed.first->second])
                + " and " + detail::row_name (row) + " have the same key "
                + detail::key_name (row.key) + " on the wire");
        }
      std::string text;
      if (row.key.kind == dsl::key_kind::text)
        text.assign (row.key.text, row.key.text_size);
      else if (row.text_key != nullptr)
        row.text_key (text);
      if (row.key.kind == dsl::key_kind::text || row.text_key != nullptr)
        {
          std::pair<std::unordered_map<std::string, std::size_t>::iterator,
                    bool> const placed
              = built.m_by_text.insert (std::make_pair (text, i));
          if (!placed.second)
            return detail::duplicate (
                label + detail::row_name (rows.data ()[placed.first->second])
                + " and " + detail::row_name (row) + " have the same key \""
                + text + "\"");
        }
      char const *const names[2]
          = { row.entry->specification_name, row.entry->identifier };
      for (std::size_t n = 0; n < 2; ++n)
        {
          std::pair<std::unordered_map<std::string, std::size_t>::iterator,
                    bool> const placed
              = built.m_by_name.insert (std::make_pair (names[n], i));
          if (!placed.second && placed.first->second != i)
            return detail::duplicate (
                label + detail::row_name (rows.data ()[placed.first->second])
                + " and " + detail::row_name (row) + " have the same name "
                + names[n]);
        }
    }
  for (std::size_t r = 0; r < registries.size (); ++r)
    {
      std::size_t first = 0;
      std::size_t second = 0;
      if (::lumex::core::protocol::registry::find_duplicate_key (
              registries[r].entries, first, second))
        return detail::duplicate (
            std::string ("registry ") + registries[r].name + ": entries "
            + registries[r].entries.data ()[first].identifier + " and "
            + registries[r].entries.data ()[second].identifier
            + " have the same key "
            + ::lumex::core::protocol::registry::key_to_string (
                registries[r].entries.data ()[first].key));
    }
  return built;
}

inline row_t const *
protocol_index_t::find_integer (std::uint64_t bits) const
{
  std::unordered_map<std::uint64_t, std::size_t>::const_iterator const found
      = m_by_integer.find (bits);
  return found == m_by_integer.end () ? nullptr
                                      : m_rows.data () + found->second;
}

inline row_t const *
protocol_index_t::find_text (char const *text, std::size_t size) const
{
  std::unordered_map<std::string, std::size_t>::const_iterator const found
      = m_by_text.find (std::string (text, size));
  return found == m_by_text.end () ? nullptr : m_rows.data () + found->second;
}

inline row_t const *
protocol_index_t::find_name (char const *name) const
{
  std::unordered_map<std::string, std::size_t>::const_iterator const found
      = m_by_name.find (name);
  return found == m_by_name.end () ? nullptr : m_rows.data () + found->second;
}

inline row_t const *
protocol_index_t::find (
    ::lumex::core::protocol::dsl::key_value_t const &key) const
{
  namespace dsl = ::lumex::core::protocol::dsl;
  if (key.kind == dsl::key_kind::text)
    return find_text (key.text, key.text_size);
  if (key.kind == dsl::key_kind::integer
      || key.kind == dsl::key_kind::composite)
    return find_integer (dsl::normalize_key (key, m_key_width).integer);
  return nullptr;
}

inline registry_info_t const *
protocol_index_t::find_registry (char const *name) const
{
  for (std::size_t i = 0; i < m_registries.size (); ++i)
    {
      std::string const wanted = name;
      if (wanted == m_registries[i].name)
        return &m_registries[i];
    }
  return nullptr;
}

inline frame_match_t
protocol_index_t::match (
    ::lumex::core::protocol::layout::bytes_view_t frame) const
{
  frame_match_t result
      = { nullptr, false, ::lumex::core::protocol::dsl::key_value_t (), 0 };
  key_read_t read = key_read_t ();
  if (m_reader == nullptr || !m_reader (frame, read))
    return result;
  result.has_key = true;
  result.key = read.key;
  result.payload_offset = read.payload_offset;
  result.row = find (read.key);
  return result;
}

inline std::string
protocol_index_t::describe (
    ::lumex::core::protocol::layout::bytes_view_t frame,
    frame_direction direction, session_context_t const *session,
    describe_options_t const &options) const
{
  std::string out = direction == frame_direction::tx ? "TX " : "RX ";
  frame_match_t const found = match (frame);
  std::string tail = " len=" + std::to_string (frame.size ());
  if (frame.size () != 0)
    {
      tail += ": ";
      append_dump (tail, frame.data (), frame.size (), options.dump_limit);
    }
  if (found.row == nullptr)
    {
      out += "unknown";
      if (found.has_key)
        {
          out += " key=";
          out += detail::key_name (found.key);
        }
      out += tail;
      return out;
    }
  row_t const &row = *found.row;
  out += m_name;
  out += '.';
  out += row.entry->specification_name;
  out += " (";
  out += row.entry->description;
  out += ')';
  describe_hook_t const hook = direction == frame_direction::tx
                                   ? row.describe_request
                                   : row.describe_reply;
  if (hook == nullptr)
    {
      out += tail;
      return out;
    }
  std::size_t const offset = found.payload_offset < frame.size ()
                                 ? found.payload_offset
                                 : frame.size ();
  ::lumex::core::protocol::field::decode_context_t const context
      = session != nullptr
            ? session->decode_context ()
            : ::lumex::core::protocol::field::decode_context_t ();
  std::string fields;
  std::error_code const error
      = hook (frame.subspan (offset), session != nullptr ? &context : nullptr,
              options, fields);
  if (error)
    {
      out += " [decode error: " + error.message () + "]";
      out += tail;
    }
  else if (!fields.empty ())
    {
      out += ' ';
      out += fields;
    }
  return out;
}

/**
 * @brief Builds the index of protocol layer `P` with the registries
 * `Registries...` attached (`LUMEX_PROTOCOL_REGISTRY` types).
 * @return The index, or `duplicate_key` with a message that names the rows
 * or entries and the key.
 */
template <typename P, typename... Registries>
index_result_t
build_index ()
{
  static_assert (::lumex::core::protocol::dsl::is_protocol<P>::value,
                 "build_index<P>: P is a LUMEX_PROTOCOL_DEFINE or "
                 "LUMEX_PROTOCOL_EXTEND type");
  static_assert (detail::vocabulary_accepts_t<P>::value,
                 "LUMEX_PROTOCOL: the vocabulary of the protocol refuses its "
                 "table (lumex_check_protocol<P>::value is false)");
  std::vector<registry_info_t> registries;
  int const placed[]
      = { 0, (registries.push_back (registry_info_t{
                  Registries::registry_name (), Registries::entries () }),
              0)... };
  (void)placed;
  return protocol_index_t::assemble (
      P::protocol_name (), detail::base_name_of<P>::get (),
      detail::row_table<P, typename dsl::commands_of<P>::type,
                        ::lumex::core::utility::sequence::make_index_sequence<
                            dsl::command_count<P>::value>>::get (),
      registries, key_width_of<P>::value, &key_reader_of<P>::type::read);
}

/**
 * @brief The index of protocol layer `P`, built once and kept for the
 * life of the program (the result is the same on every call).
 * @details Does not compile when two integer keys of the table are the same
 * on the wire (`keys_unique<P>`).
 */
template <typename P, typename... Registries>
index_result_t const &
protocol_index ()
{
  static_assert (keys_unique<P>::value,
                 "LUMEX_PROTOCOL: two rows of the protocol have the same "
                 "integer key on the wire (cut to key_width_of<P>)");
  static index_result_t const result = build_index<P, Registries...> ();
  return result;
}
} // namespace index
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_INDEX_INDEX_HPP
