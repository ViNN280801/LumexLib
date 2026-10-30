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
 * @file LumexDialogueVocabulary.hpp
 * @brief The words protocol tables use for the dialogue of
 * `applied/transport/protocol`: the reply kinds (`ack_nak`, `two_phase`,
 * `echo_set`, `echo_zero_on_error`, `state_instead_of_refusal`), the row
 * options (`correlate`, `reply_row`, `min_interval`, `edge_event`,
 * `match_with`), the protocol options a header names (`dialogue_options`),
 * the vocabulary `dialogue::vocabulary_t` that brings them into a table, and
 * the traits the dialogue engine and the client read (`row_traits_t`,
 * `protocol_traits_t`, `reply_value_of_t`, `check_protocol_t`).
 * @details A table takes the words by its header:
 * @code
 * struct pump_header_t
 * {
 *   using lumex_vocabulary = dialogue::vocabulary_t;
 *   static constexpr std::size_t key_width = 1;
 *   // Protocol options; the header comes before the protocol, so the rows
 *   // are named through the layer P the options are read for.
 *   template <typename P>
 *   using dialogue_options = dialogue::options<
 *       dialogue::keepalive<typename P::CLOCK_t, 1000>,
 *       dialogue::watchdog_ms<3000>,
 *       dialogue::state_from<typename P::STATUS_t, 1> >;
 * };
 * #define PUMP_ROWS(X, X_AS) \
 *   X (ACK, 0x06, "Accepted", (), (), (), ()) \
 *   X (NAK, 0x15, "Refused", (), (), (u8_), ()) \
 *   X (STOP, 0x02, "Stop", (), (), (ack_nak<ACK_t, NAK_t>), ()) \
 *   X (START, 0x01, "Start", (u8_), ("channel"), \
 *      (ack_nak<u8_, refusal_t>), (min_interval (50))) \
 *   X (WRITE, 0x08, "Write", (u8_, u16_le_), ("parameter", "value"), (), \
 *      (reply_row<CHANGE_t> ()))
 * @endcode
 * The part of a reply kind (the ACK, the NAK, the completion) is either a
 * field type (a field or a layout: the reply message of the row itself, its
 * header and key as the core frames a reply, with these fields) or a row of
 * the protocol (another message with a key of its own, as a separate
 * acknowledgement character). A row named by a reply kind or an option is
 * written before the row that names it (or comes from the base layer).
 *
 * The index (`core/protocol`) describes the reply message of a row with a
 * reply kind through the kind (`RX pump_t.START (Start) NAK code=5
 * detail=4660`), names the kind for the export, lists the row options in
 * `options.extra` and makes the checks of `check_protocol_t<P>` when it is
 * built: a table the dialogue cannot use does not compile once its index is
 * built, with the message of the check.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DSL_DIALOGUE_VOCABULARY_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DSL_DIALOGUE_VOCABULARY_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <system_error>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/protocol/index/LumexProtocolFormat.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryFields.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding: the matcher stored by
// match_with_t is a function pointer).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
#if __has_warning("-Wlifetime-safety-invalidation")
// Clang 23 reports a reference parameter to a string as "invalidated" by its
// own append (a false positive: only references into the string are).
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#if __has_warning("-Wlifetime-safety-intra-tu-constructor-suggestions")
#pragma clang diagnostic ignored                                              \
    "-Wlifetime-safety-intra-tu-constructor-suggestions"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
// snprintf is the standard function; the secure variants are not portable.
#pragma warning(disable : 4996)
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
namespace dialogue
{
/// @brief How the dialogue matches a reply to its request.
enum class correlation : std::uint8_t
{
  /// The reply carries the key of the request (the default).
  by_key,
  /// The reply carries no key of the request: the next reply answers the
  /// request in flight (one at a time).
  by_order,
  /// The reply is the message of another row (`reply_row<R>`): a write
  /// answered by a change report.
  by_reply_key,
  /// A function of the table decides (`match_with (function)`).
  custom
};

/// @brief The name of a correlation (`"by_order"`), `"unknown"` for a value
/// outside the enumeration.
LUMEX_CONSTEXPR inline char const *
correlation_name (correlation how) LUMEX_NOEXCEPT
{
  return how == correlation::by_key         ? "by_key"
         : how == correlation::by_order     ? "by_order"
         : how == correlation::by_reply_key ? "by_reply_key"
         : how == correlation::custom       ? "custom"
                                            : "unknown";
}

/// @brief What the reply of a row is to the dialogue.
enum class reply_kind : std::uint8_t
{
  /// The row has no reply.
  none,
  /// A field or a layout: the core decodes the reply message.
  message,
  /// `ack_nak<Ack, Nak>`.
  ack_nak,
  /// `two_phase<Ack, Completion>`.
  two_phase,
  /// `echo_set<T>`.
  echo_set,
  /// `echo_zero_on_error<T>`.
  echo_zero_on_error,
  /// `state_instead_of_refusal<T>`.
  state_instead_of_refusal,
  /// A reply kind of another layer.
  other
};

/**
 * @brief The matcher of a row with a custom correlation: whether `message`
 * (its row `reply`, `nullptr` for an unknown key) answers the request
 * `request` in flight.
 */
using reply_matcher_t
    = bool (*) (::lumex::core::protocol::index::row_t const *reply,
                ::lumex::core::protocol::layout::bytes_view_t message,
                ::lumex::core::protocol::layout::bytes_view_t request);

namespace detail
{
namespace dsl = ::lumex::core::protocol::dsl;
namespace field = ::lumex::core::protocol::field;
namespace index = ::lumex::core::protocol::index;
namespace layout = ::lumex::core::protocol::layout;

template <typename T> struct voider_t
{
  using type = void;
};

template <typename T> struct type_is_t
{
  using type = T;
};

template <typename T> struct dependent_false_t : std::false_type
{
};

template <bool... B> struct bools_t
{
};

template <bool... B>
struct all_t : std::is_same<bools_t<true, B...>, bools_t<B..., true>>
{
};

// A part of a reply kind: 0 a field type (the reply message of the row
// itself), 1 a row of the protocol (another message), 2 neither.
template <typename T>
struct part_form_t
    : std::integral_constant<int, dsl::is_command<T>::value
                                      ? 1
                                      : (field::is_field<T>::value ? 0 : 2)>
{
};

// The part with its vocabulary name resolved (u8_ is field::u8_<>).
template <typename T, int Form = part_form_t<T>::value> struct part_type_t
{
  using type = T;
};

template <typename T> struct part_type_t<T, 0>
{
  using type = typename field::resolve<T>::type;
};

// 0: no reply, 1: a field or a layout, 2: a reply kind.
template <typename Row>
struct reply_form_t
    : std::integral_constant<
          int,
          std::is_same<typename Row::reply_type, dsl::no_reply>::value
              ? 0
              : (field::is_field<typename Row::reply_type>::value ? 1 : 2)>
{
};

// The value of the message of a row as a reply (what a row named as a part
// or as reply_row<R> gives the client).
template <typename Row, int Form = reply_form_t<Row>::value>
struct message_value_t
{
  using type = field::record<>;
};

template <typename Row> struct message_value_t<Row, 1>
{
  using type = typename layout::reply_value_of<Row>::type;
};

template <typename Row> struct message_value_t<Row, 2>
{
  static_assert (dependent_false_t<Row>::value,
                 "LUMEX_PROTOCOL: a reply kind or reply_row<R> names a row "
                 "whose reply is itself a reply kind: name a row whose reply "
                 "is a field, a layout or none");
  using type = field::record<>;
};

template <typename T, int Form = part_form_t<T>::value> struct part_value_t
{
  using type = typename field::value_of<T>::type;
};

template <typename T> struct part_value_t<T, 1> : message_value_t<T>
{
};

template <typename T> struct part_value_t<T, 2>
{
  using type = void;
};

// The part as the export names it.
template <typename T, int Form = part_form_t<T>::value,
          bool Layout = dsl::is_layout<typename part_type_t<T>::type>::value>
struct part_text_t
{
  static std::string
  get ()
  {
    return "one field";
  }
};

template <typename T, bool Layout> struct part_text_t<T, 1, Layout>
{
  static std::string
  get ()
  {
    return std::string ("message `") + T::specification_name () + "`";
  }
};

template <typename T> struct part_text_t<T, 0, true>
{
  static std::string
  get ()
  {
    return std::string ("layout `") + part_type_t<T>::type::layout_name ()
           + "`";
  }
};

// The reply message of Row read as field type F: the key of Row (a tokenized
// layer reads it as the first token), then F.
template <typename Row, typename F> struct as_reply_t
{
  using fields_type = typename Row::fields_type;
  using reply_type = F;
  using key_type = typename Row::key_type;

  static LUMEX_CONSTEXPR key_type
  key () LUMEX_NOEXCEPT
  {
    return Row::key ();
  }
};

// The word of a part (ACK, NAK), then its fields when it has any.
template <typename F>
void
append_part (std::string &out, char const *word,
             typename field::value_of<F>::type const &value,
             index::describe_options_t const &options, char const *label)
{
  out += word;
  std::string fields;
  index::append_value<F> (fields, value, options, label);
  if (!fields.empty ())
    {
      out += ' ';
      out += fields;
    }
}

// A part of a reply kind describes the payload when the payload is that
// part; a row is another message, so the payload of Row is never it.
template <typename P, typename Row, typename T,
          int Form = part_form_t<T>::value>
struct part_describer_t
{
  static std::error_code
  apply (layout::bytes_view_t, field::decode_context_t const *,
         index::describe_options_t const &, std::string &, char const *,
         char const *)
  {
    return ::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::unexpected_reply);
  }
};

template <typename P, typename Row, typename T>
struct part_describer_t<P, Row, T, 0>
{
  using part = typename part_type_t<T>::type;

  static std::error_code
  apply (layout::bytes_view_t payload, field::decode_context_t const *context,
         index::describe_options_t const &options, std::string &out,
         char const *word, char const *label)
  {
    layout::decode_result_t<typename field::value_of<part>::type> const decoded
        = layout::decode_reply<P, as_reply_t<Row, part>> (payload, context);
    if (!decoded)
      return decoded.error ();
    append_part<part> (out, word, *decoded, options, label);
    return std::error_code ();
  }
};

// The first of two parts the payload is; the error of the last field part
// tried when it is none (unexpected_reply when both are rows).
template <typename P, typename Row, typename A, typename B>
std::error_code
describe_either (layout::bytes_view_t payload,
                 field::decode_context_t const *context,
                 index::describe_options_t const &options, std::string &out,
                 char const *word_a, char const *label_a, char const *word_b,
                 char const *label_b)
{
  std::error_code const first = part_describer_t<P, Row, A>::apply (
      payload, context, options, out, word_a, label_a);
  if (!first)
    return std::error_code ();
  std::error_code const second = part_describer_t<P, Row, B>::apply (
      payload, context, options, out, word_b, label_b);
  if (!second)
    return std::error_code ();
  return part_form_t<B>::value == 1 ? std::error_code (first)
                                    : std::error_code (second);
}

// Whether a value is zero: a number, a flag, an enumeration or a scaled
// number by its raw value; known is false for any other value.
template <typename V, typename = void> struct zero_test_t
{
  using known = std::false_type;

  static bool
  apply (V const &) LUMEX_NOEXCEPT
  {
    return false;
  }
};

template <typename V>
struct zero_test_t<V,
                   typename std::enable_if<std::is_integral<V>::value>::type>
{
  using known = std::true_type;

  static bool
  apply (V const &value) LUMEX_NOEXCEPT
  {
    return value == V (0);
  }
};

template <typename V>
struct zero_test_t<
    V, typename std::enable_if<std::is_floating_point<V>::value>::type>
{
  using known = std::true_type;

  // 0.0 and -0.0 are zero, a NaN is not.
  static bool
  apply (V const &value) LUMEX_NOEXCEPT
  {
    return value <= V (0) && value >= V (0);
  }
};

template <typename V>
struct zero_test_t<V, typename std::enable_if<std::is_enum<V>::value>::type>
{
  using known = std::true_type;

  static bool
  apply (V const &value) LUMEX_NOEXCEPT
  {
    return static_cast<typename std::underlying_type<V>::type> (value) == 0;
  }
};

template <typename Raw, typename Ratio>
struct zero_test_t<field::fixed_point<Raw, Ratio>, void>
{
  using known = std::true_type;

  static bool
  apply (field::fixed_point<Raw, Ratio> const &value) LUMEX_NOEXCEPT
  {
    return value.raw == Raw (0);
  }
};

template <typename T, bool Field = field::is_field<T>::value>
struct zero_known_t : zero_test_t<typename field::value_of<T>::type>::known
{
};

template <typename T> struct zero_known_t<T, false> : std::false_type
{
};

// The echo of a value: ECHO, or REFUSED for a zero when ZeroRefuses.
template <typename P, typename Row, typename T, bool ZeroRefuses>
std::error_code
describe_echo (layout::bytes_view_t payload,
               field::decode_context_t const *context,
               index::describe_options_t const &options, std::string &out)
{
  using part = typename field::resolve<T>::type;
  using value_type = typename field::value_of<part>::type;
  layout::decode_result_t<value_type> const decoded
      = layout::decode_reply<P, as_reply_t<Row, part>> (payload, context);
  if (!decoded)
    return decoded.error ();
  bool const refused
      = ZeroRefuses && zero_test_t<value_type>::apply (*decoded);
  append_part<part> (out, refused ? "REFUSED" : "ECHO", *decoded, options,
                     "value");
  return std::error_code ();
}
} // namespace detail

/**
 * @brief Reply kind: a positive (`Ack`) or a negative (`Nak`)
 * acknowledgement, `ack_nak<Ack, Nak>` in a table.
 * @details Each part is a field type (the reply message of the row with
 * these fields) or a row of the protocol (a message of its own, as an
 * acknowledgement character: the reply then carries no key of the request
 * and the default correlation is `by_order`, the only one allowed besides
 * `custom`). Two field parts are told
 * apart by their shape: the first that the whole message decodes as wins,
 * so give them different shapes. `describe` writes `ACK value=7` or `NAK
 * code=5` (a layout by its field names). The client returns the value of
 * `Ack`; a NAK ends the call with `protocol_errc::nak`.
 * @tparam Ack The positive acknowledgement.
 * @tparam Nak The negative acknowledgement.
 */
template <typename Ack, typename Nak> struct ack_nak_t
{
  static_assert (detail::part_form_t<Ack>::value != 2,
                 "LUMEX_PROTOCOL: ack_nak<Ack, Nak>: Ack is a field, a "
                 "layout or a row of the protocol (written before the row "
                 "that names it)");
  static_assert (detail::part_form_t<Nak>::value != 2,
                 "LUMEX_PROTOCOL: ack_nak<Ack, Nak>: Nak is a field, a "
                 "layout or a row of the protocol (written before the row "
                 "that names it)");
  static_assert (!std::is_same<typename detail::part_type_t<Ack>::type,
                               typename detail::part_type_t<Nak>::type>::value,
                 "LUMEX_PROTOCOL: ack_nak<Ack, Nak>: Ack and Nak are "
                 "different, so that the dialogue tells them apart");

  /// A reply kind of the dialogue.
  using lumex_protocol_reply = ::lumex::core::protocol::dsl::reply_tag;
  /// The positive acknowledgement (a vocabulary name resolved).
  using ack_type = typename detail::part_type_t<Ack>::type;
  /// The negative acknowledgement.
  using nak_type = typename detail::part_type_t<Nak>::type;
  /// What the client returns: the value of the ACK.
  using value_type = typename detail::part_value_t<Ack>::type;
  /// The value of the NAK (for the log and the error).
  using nak_value_type = typename detail::part_value_t<Nak>::type;

  /// @brief The describe hook of the index: `ACK ...` or `NAK ...`.
  template <typename P, typename Row>
  static std::error_code
  lumex_describe_reply (
      ::lumex::core::protocol::layout::bytes_view_t payload,
      ::lumex::core::protocol::field::decode_context_t const *context,
      ::lumex::core::protocol::index::describe_options_t const &options,
      std::string &out)
  {
    return detail::describe_either<P, Row, Ack, Nak> (
        payload, context, options, out, "ACK", "value", "NAK", "code");
  }

  /// @brief The kind and its parts for the export.
  static char const *
  lumex_reply_text ()
  {
    static std::string const text
        = "ack_nak (ACK: " + detail::part_text_t<Ack>::get ()
          + "; NAK: " + detail::part_text_t<Nak>::get () + ")";
    return text.c_str ();
  }
};

/**
 * @brief Reply kind: an acknowledgement now, a completion later,
 * `two_phase<Ack, Completion>` in a table.
 * @details The parts are field types or rows, as in `ack_nak_t`; the first
 * reply is the acknowledgement, the next one the completion (`describe`
 * writes the first part a message decodes as: `ACK`, `COMPLETION
 * value=1000`). The client returns the value of the completion.
 * @tparam Ack The acknowledgement.
 * @tparam Completion The completion.
 */
template <typename Ack, typename Completion> struct two_phase_t
{
  static_assert (detail::part_form_t<Ack>::value != 2,
                 "LUMEX_PROTOCOL: two_phase<Ack, Completion>: Ack is a field, "
                 "a layout or a row of the protocol (written before the row "
                 "that names it)");
  static_assert (detail::part_form_t<Completion>::value != 2,
                 "LUMEX_PROTOCOL: two_phase<Ack, Completion>: Completion is a "
                 "field, a layout or a row of the protocol (written before "
                 "the row that names it)");

  /// A reply kind of the dialogue.
  using lumex_protocol_reply = ::lumex::core::protocol::dsl::reply_tag;
  /// The acknowledgement (a vocabulary name resolved).
  using ack_type = typename detail::part_type_t<Ack>::type;
  /// The completion.
  using completion_type = typename detail::part_type_t<Completion>::type;
  /// What the client returns: the value of the completion.
  using value_type = typename detail::part_value_t<Completion>::type;
  /// The value of the acknowledgement.
  using ack_value_type = typename detail::part_value_t<Ack>::type;

  /// @brief The describe hook of the index: `ACK ...` or `COMPLETION ...`.
  template <typename P, typename Row>
  static std::error_code
  lumex_describe_reply (
      ::lumex::core::protocol::layout::bytes_view_t payload,
      ::lumex::core::protocol::field::decode_context_t const *context,
      ::lumex::core::protocol::index::describe_options_t const &options,
      std::string &out)
  {
    return detail::describe_either<P, Row, Ack, Completion> (
        payload, context, options, out, "ACK", "value", "COMPLETION", "value");
  }

  /// @brief The kind and its parts for the export.
  static char const *
  lumex_reply_text ()
  {
    static std::string const text
        = "two_phase (ACK: " + detail::part_text_t<Ack>::get ()
          + "; completion: " + detail::part_text_t<Completion>::get () + ")";
    return text.c_str ();
  }
};

/**
 * @brief Reply kind: the device echoes the value it took, `echo_set<T>` in
 * a table; the client compares it with the value it sent
 * (`protocol_errc::not_confirmed` when they differ).
 * @tparam T The field or layout of the echo.
 */
template <typename T> struct echo_set_t
{
  static_assert (::lumex::core::protocol::field::is_field<T>::value,
                 "LUMEX_PROTOCOL: echo_set<T>: T is a field or a layout (the "
                 "value the device echoes)");

  /// A reply kind of the dialogue.
  using lumex_protocol_reply = ::lumex::core::protocol::dsl::reply_tag;
  /// The echo (a vocabulary name resolved).
  using echo_type = typename ::lumex::core::protocol::field::resolve<T>::type;
  /// What the client returns: the value the device took.
  using value_type =
      typename ::lumex::core::protocol::field::value_of<echo_type>::type;

  /// @brief The describe hook of the index: `ECHO value=...`.
  template <typename P, typename Row>
  static std::error_code
  lumex_describe_reply (
      ::lumex::core::protocol::layout::bytes_view_t payload,
      ::lumex::core::protocol::field::decode_context_t const *context,
      ::lumex::core::protocol::index::describe_options_t const &options,
      std::string &out)
  {
    return detail::describe_echo<P, Row, T, false> (payload, context, options,
                                                    out);
  }

  /// @brief The kind and its echo for the export.
  static char const *
  lumex_reply_text ()
  {
    static std::string const text
        = "echo_set (" + detail::part_text_t<T>::get () + ")";
    return text.c_str ();
  }
};

/**
 * @brief Reply kind: the echo of the value taken, zero when the device
 * refused, `echo_zero_on_error<T>` in a table (`protocol_errc::refused` for
 * a zero).
 * @tparam T One field whose value is a number, a flag or an enumeration.
 */
template <typename T> struct echo_zero_on_error_t
{
  static_assert (detail::zero_known_t<T>::value,
                 "LUMEX_PROTOCOL: echo_zero_on_error<T> takes one field "
                 "whose value is a number, a flag or an enumeration (its "
                 "zero is the refusal)");

  /// A reply kind of the dialogue.
  using lumex_protocol_reply = ::lumex::core::protocol::dsl::reply_tag;
  /// The echo (a vocabulary name resolved).
  using echo_type = typename ::lumex::core::protocol::field::resolve<T>::type;
  /// What the client returns: the value the device took.
  using value_type =
      typename ::lumex::core::protocol::field::value_of<echo_type>::type;

  /// @brief The describe hook of the index: `ECHO value=...`, `REFUSED
  /// value=0`.
  template <typename P, typename Row>
  static std::error_code
  lumex_describe_reply (
      ::lumex::core::protocol::layout::bytes_view_t payload,
      ::lumex::core::protocol::field::decode_context_t const *context,
      ::lumex::core::protocol::index::describe_options_t const &options,
      std::string &out)
  {
    return detail::describe_echo<P, Row, T, true> (payload, context, options,
                                                   out);
  }

  /// @brief The kind and its echo for the export.
  static char const *
  lumex_reply_text ()
  {
    static std::string const text
        = "echo_zero_on_error (" + detail::part_text_t<T>::get () + ")";
    return text.c_str ();
  }
};

/**
 * @brief Reply kind: the echo of the value taken, or the current state of the
 * device instead of a refusal, `state_instead_of_refusal<T>` in a table; a
 * reply other than the value sent ends the call with
 * `protocol_errc::refused` (both values in the log).
 * @tparam T The field or layout of the echo and of the state.
 */
template <typename T> struct state_instead_of_refusal_t
{
  static_assert (::lumex::core::protocol::field::is_field<T>::value,
                 "LUMEX_PROTOCOL: state_instead_of_refusal<T>: T is a field "
                 "or a layout (the echo, or the state instead of a refusal)");

  /// A reply kind of the dialogue.
  using lumex_protocol_reply = ::lumex::core::protocol::dsl::reply_tag;
  /// The echo (a vocabulary name resolved).
  using echo_type = typename ::lumex::core::protocol::field::resolve<T>::type;
  /// What the client returns: the value the device took.
  using value_type =
      typename ::lumex::core::protocol::field::value_of<echo_type>::type;

  /// @brief The describe hook of the index: `ECHO value=...`.
  template <typename P, typename Row>
  static std::error_code
  lumex_describe_reply (
      ::lumex::core::protocol::layout::bytes_view_t payload,
      ::lumex::core::protocol::field::decode_context_t const *context,
      ::lumex::core::protocol::index::describe_options_t const &options,
      std::string &out)
  {
    return detail::describe_echo<P, Row, T, false> (payload, context, options,
                                                    out);
  }

  /// @brief The kind and its echo for the export.
  static char const *
  lumex_reply_text ()
  {
    static std::string const text
        = "state_instead_of_refusal (" + detail::part_text_t<T>::get () + ")";
    return text.c_str ();
  }
};

/// @brief Row option `correlate (correlation::...)`: how the reply of the
/// row is matched to it (the kind of `dsl::get_option<correlation_t>`).
struct correlation_t
{
  /// The kind of the option.
  using option_kind = correlation_t;
  /// A word of the table vocabulary.
  using lumex_vocabulary_word
      = ::lumex::core::protocol::dsl::vocabulary_word_tag;
  /// The correlation.
  correlation how;

  /// @brief `correlate (how)`.
  LUMEX_CONSTEXPR_CTOR explicit correlation_t (correlation chosen)
      LUMEX_NOEXCEPT : how (chosen)
  {
  }

  /// @brief `"correlation"`.
  static LUMEX_CONSTEXPR char const *
  option_name () LUMEX_NOEXCEPT
  {
    return "correlation";
  }

  /// @brief The enumerator as a number.
  std::int64_t
  view_value () const LUMEX_NOEXCEPT
  {
    return static_cast<std::int64_t> (how);
  }

  /// @brief The name of the correlation.
  char const *
  view_text () const LUMEX_NOEXCEPT
  {
    return correlation_name (how);
  }
};

/// @brief The kind of `reply_row<R>` (one per row, whatever `R`).
struct reply_row_kind_t
{
};

/**
 * @brief Row option `reply_row<R> ()`: the reply of the row is the message
 * of row `R` (a write answered by a change report); the correlation is
 * `by_reply_key` and the reply group of the row stays empty.
 * @tparam Row The row whose message answers, of this protocol or its base.
 */
template <typename Row> struct reply_row_t
{
  static_assert (::lumex::core::protocol::dsl::is_command<Row>::value,
                 "LUMEX_PROTOCOL: reply_row<R>: R is a row of the protocol (a "
                 "type R_t of the table, written before the row that names "
                 "it)");

  /// The kind of the option.
  using option_kind = reply_row_kind_t;
  /// A word of the table vocabulary.
  using lumex_vocabulary_word
      = ::lumex::core::protocol::dsl::vocabulary_word_tag;
  /// The row whose message answers.
  using row_type = Row;

  /// @brief `reply_row<R> ()`.
  LUMEX_CONSTEXPR_CTOR
  reply_row_t () LUMEX_NOEXCEPT {}

  /// @brief `"reply_row"`.
  static LUMEX_CONSTEXPR char const *
  option_name () LUMEX_NOEXCEPT
  {
    return "reply_row";
  }

  /// @brief No number: 0.
  std::int64_t
  view_value () const LUMEX_NOEXCEPT
  {
    return 0;
  }

  /// @brief The specification name of the row.
  char const *
  view_text () const LUMEX_NOEXCEPT
  {
    return Row::specification_name ();
  }
};

/// @brief Row option `min_interval (ms)`: at least this many milliseconds
/// between two requests of the row (a rate limit).
struct min_interval_t
{
  /// The kind of the option.
  using option_kind = min_interval_t;
  /// A word of the table vocabulary.
  using lumex_vocabulary_word
      = ::lumex::core::protocol::dsl::vocabulary_word_tag;
  /// Milliseconds.
  std::uint32_t milliseconds;

  /// @brief `min_interval (ms)`.
  LUMEX_CONSTEXPR_CTOR explicit min_interval_t (std::uint32_t ms)
      LUMEX_NOEXCEPT : milliseconds (ms)
  {
  }

  /// @brief `"min_interval"`.
  static LUMEX_CONSTEXPR char const *
  option_name () LUMEX_NOEXCEPT
  {
    return "min_interval";
  }

  /// @brief The milliseconds.
  std::int64_t
  view_value () const LUMEX_NOEXCEPT
  {
    return static_cast<std::int64_t> (milliseconds);
  }
};

namespace detail
{
// The text of an edge for the entry, made once without allocating.
template <std::size_t Field, std::int64_t From, std::int64_t To>
struct edge_text_t
{
  struct text_t
  {
    char data[80];

    text_t () LUMEX_NOEXCEPT
    {
      std::snprintf (data, sizeof (data), "field %llu: %lld -> %lld",
                     static_cast<unsigned long long> (Field),
                     static_cast<long long> (From),
                     static_cast<long long> (To));
    }
  };

  static char const *
  get () LUMEX_NOEXCEPT
  {
    static text_t const text;
    return text.data;
  }
};
} // namespace detail

/**
 * @brief Row option `edge_event<Field, From, To> ()` of a `stream` row: the
 * dialogue raises an event when reply field `Field` goes from `From` to
 * `To` (an edge, not a level). A row may name several edges.
 * @tparam Field The index of the field in the reply of the row.
 * @tparam From The value before the edge.
 * @tparam To The value after it.
 */
template <std::size_t Field, std::int64_t From, std::int64_t To>
struct edge_event_t
{
  static_assert (From != To,
                 "LUMEX_PROTOCOL: edge_event<Field, From, To> needs From and "
                 "To different");

  /// The kind of the option: the edge itself (several edges per row).
  using option_kind = edge_event_t;
  /// A row may hold several edges: the JSON export writes an array.
  using option_repeats = std::true_type;
  /// A word of the table vocabulary.
  using lumex_vocabulary_word
      = ::lumex::core::protocol::dsl::vocabulary_word_tag;

  /// @brief `edge_event<Field, From, To> ()`.
  LUMEX_CONSTEXPR_CTOR
  edge_event_t () LUMEX_NOEXCEPT {}

  /// @brief The index of the field in the reply.
  static LUMEX_CONSTEXPR std::size_t
  field () LUMEX_NOEXCEPT
  {
    return Field;
  }

  /// @brief The value before the edge.
  static LUMEX_CONSTEXPR std::int64_t
  from () LUMEX_NOEXCEPT
  {
    return From;
  }

  /// @brief The value after the edge.
  static LUMEX_CONSTEXPR std::int64_t
  to () LUMEX_NOEXCEPT
  {
    return To;
  }

  /// @brief `"edge_event"`.
  static LUMEX_CONSTEXPR char const *
  option_name () LUMEX_NOEXCEPT
  {
    return "edge_event";
  }

  /// @brief The index of the field.
  std::int64_t
  view_value () const LUMEX_NOEXCEPT
  {
    return static_cast<std::int64_t> (Field);
  }

  /// @brief `"field 0: 0 -> 1"`.
  char const *
  view_text () const LUMEX_NOEXCEPT
  {
    return detail::edge_text_t<Field, From, To>::get ();
  }
};

/// @brief Row option `match_with (function)`: the correlation is `custom`,
/// the function decides which message answers the row.
struct match_with_t
{
  /// The kind of the option.
  using option_kind = match_with_t;
  /// A word of the table vocabulary.
  using lumex_vocabulary_word
      = ::lumex::core::protocol::dsl::vocabulary_word_tag;
  /// The matcher.
  reply_matcher_t matcher;

  /// @brief `match_with (function)`.
  LUMEX_CONSTEXPR_CTOR explicit match_with_t (reply_matcher_t chosen)
      LUMEX_NOEXCEPT : matcher (chosen)
  {
  }

  /// @brief `"match_with"`.
  static LUMEX_CONSTEXPR char const *
  option_name () LUMEX_NOEXCEPT
  {
    return "match_with";
  }

  /// @brief No number: 0.
  std::int64_t
  view_value () const LUMEX_NOEXCEPT
  {
    return 0;
  }

  /// @brief `"custom"`.
  char const *
  view_text () const LUMEX_NOEXCEPT
  {
    return "custom";
  }
};

/// @brief The tag a protocol option of the dialogue names as
/// `lumex_dialogue_option`.
struct protocol_option_tag_t
{
};

namespace detail
{
struct keepalive_kind_t
{
};
struct watchdog_kind_t
{
};
struct min_interval_kind_t
{
};
struct state_from_kind_t
{
};
struct identify_with_kind_t
{
};
struct max_in_flight_kind_t
{
};
struct default_timeout_kind_t
{
};
struct default_retries_kind_t
{
};
struct late_reply_window_kind_t
{
};
} // namespace detail

/**
 * @brief Protocol option `keepalive<Row, PeriodMs>`: the dialogue sends row
 * `Row` every `PeriodMs` milliseconds, with arguments the client supplies.
 * @tparam Row The row sent (of the protocol or its base).
 * @tparam PeriodMs The period, above 0.
 */
template <typename Row, std::uint32_t PeriodMs> struct keepalive_t
{
  static_assert (::lumex::core::protocol::dsl::is_command<Row>::value,
                 "LUMEX_PROTOCOL: keepalive<Row, PeriodMs>: Row is a row of "
                 "the protocol");
  static_assert (PeriodMs > 0, "LUMEX_PROTOCOL: keepalive<Row, PeriodMs> "
                               "needs PeriodMs above 0");
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::keepalive_kind_t;
  /// The row sent.
  using row_type = Row;

  /// @brief The period in milliseconds.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return PeriodMs;
  }
};

/// @brief Protocol option `watchdog_ms<Ms>`: the link is lost
/// (`protocol_errc::link_lost`) after `Ms` milliseconds without a frame.
template <std::uint32_t Ms> struct watchdog_ms_t
{
  static_assert (Ms > 0, "LUMEX_PROTOCOL: watchdog_ms<Ms> needs Ms above 0");
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::watchdog_kind_t;
  /// No row.
  using row_type = void;

  /// @brief The milliseconds.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return Ms;
  }
};

/// @brief Protocol option `min_interval_ms<Ms>`: at least `Ms` milliseconds
/// between two requests of any row (the rate limit of the device).
template <std::uint32_t Ms> struct min_interval_ms_t
{
  static_assert (Ms > 0,
                 "LUMEX_PROTOCOL: min_interval_ms<Ms> needs Ms above 0");
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::min_interval_kind_t;
  /// No row.
  using row_type = void;

  /// @brief The milliseconds.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return Ms;
  }
};

/**
 * @brief Protocol option `state_from<Row, Field>`: the dialogue takes the
 * state of the device (for `accepted_in`) from reply field `Field` of every
 * reply and event of row `Row`.
 * @tparam Row A row with a reply of the core (a field or a layout).
 * @tparam Field The index of the field, an integer or an enumeration.
 */
template <typename Row, std::size_t Field> struct state_from_t
{
  static_assert (::lumex::core::protocol::dsl::is_command<Row>::value,
                 "LUMEX_PROTOCOL: state_from<Row, Field>: Row is a row of the "
                 "protocol");
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::state_from_kind_t;
  /// The row of the state.
  using row_type = Row;

  /// @brief The index of the field.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return Field;
  }
};

/// @brief Protocol option `identify_with<Row>`: `identify ()` sends row
/// `Row` and reads the identity from its reply.
template <typename Row> struct identify_with_t
{
  static_assert (::lumex::core::protocol::dsl::is_command<Row>::value,
                 "LUMEX_PROTOCOL: identify_with<Row>: Row is a row of the "
                 "protocol");
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::identify_with_kind_t;
  /// The row of the identity.
  using row_type = Row;

  /// @brief No number: 0.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return 0;
  }
};

/// @brief Protocol option `max_in_flight<N>`: at most `N` requests wait for
/// their replies at once (1 without the option).
template <std::size_t N> struct max_in_flight_t
{
  static_assert (N >= 1,
                 "LUMEX_PROTOCOL: max_in_flight<N> needs N of at least 1");
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::max_in_flight_kind_t;
  /// No row.
  using row_type = void;

  /// @brief The count.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return N;
  }
};

/// @brief Protocol option `default_timeout_ms<Ms>`: the timeout of a row
/// without `timeout (ms)`.
template <std::uint32_t Ms> struct default_timeout_ms_t
{
  static_assert (Ms > 0,
                 "LUMEX_PROTOCOL: default_timeout_ms<Ms> needs Ms above 0");
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::default_timeout_kind_t;
  /// No row.
  using row_type = void;

  /// @brief The milliseconds.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return Ms;
  }
};

/// @brief Protocol option `default_retries<N>`: the retries of a row
/// without `retries (n)` (`idempotent` does not add any: it decides what is
/// sent again after a reconnect).
template <std::uint32_t N> struct default_retries_t
{
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::default_retries_kind_t;
  /// No row.
  using row_type = void;

  /// @brief The count.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return N;
  }
};

/// @brief Protocol option `late_reply_window_ms<Ms>`: after a timeout of a
/// `by_order` row the frames of the next `Ms` milliseconds are dropped as
/// late replies (0 without the option: a late reply cannot be told from the
/// next one).
template <std::uint32_t Ms> struct late_reply_window_ms_t
{
  /// A protocol option of the dialogue.
  using lumex_dialogue_option = protocol_option_tag_t;
  /// Its kind.
  using dialogue_option_kind = detail::late_reply_window_kind_t;
  /// No row.
  using row_type = void;

  /// @brief The milliseconds.
  static LUMEX_CONSTEXPR std::uint64_t
  number () LUMEX_NOEXCEPT
  {
    return Ms;
  }
};

namespace detail
{
template <typename O, typename = void>
struct is_protocol_option_t : std::false_type
{
};

template <typename O>
struct is_protocol_option_t<
    O, typename voider_t<typename O::lumex_dialogue_option>::type>
    : std::is_same<typename O::lumex_dialogue_option, protocol_option_tag_t>
{
};

// The kind of a protocol option; any other type is a kind of its own.
template <typename O, bool Option = is_protocol_option_t<O>::value>
struct protocol_kind_t
{
  using type = O;
};

template <typename O> struct protocol_kind_t<O, true>
{
  using type = typename O::dialogue_option_kind;
};

template <typename K, typename... O> struct count_kind_t;

template <typename K>
struct count_kind_t<K> : std::integral_constant<std::size_t, 0>
{
};

template <typename K, typename Head, typename... Tail>
struct count_kind_t<K, Head, Tail...>
    : std::integral_constant<
          std::size_t,
          (std::is_same<typename protocol_kind_t<Head>::type, K>::value ? 1u
                                                                        : 0u)
              + count_kind_t<K, Tail...>::value>
{
};
} // namespace detail

/**
 * @brief The protocol options of the dialogue, `dialogue::options<...>`,
 * named by the header type of a protocol as `dialogue_options`.
 * @details Each option at most once. Without `dialogue_options` (or with
 * an empty list) the dialogue takes one request in flight, no keepalive,
 * no watchdog and no rate limit.
 * @tparam Options Protocol options of the dialogue.
 */
template <typename... Options> struct options_t
{
  // Every option is complete, so that its own checks run.
  static_assert (detail::all_t<(sizeof (Options) != 0)...>::value,
                 "LUMEX_PROTOCOL: the protocol options are complete types");
  static_assert (
      detail::all_t<detail::is_protocol_option_t<Options>::value...>::value,
      "LUMEX_PROTOCOL: every element of dialogue::options<...> is a protocol "
      "option of the dialogue (keepalive, watchdog_ms, min_interval_ms, "
      "state_from, identify_with, max_in_flight, default_timeout_ms, "
      "default_retries, late_reply_window_ms)");
  static_assert (
      detail::all_t<(
          detail::count_kind_t<typename detail::protocol_kind_t<Options>::type,
                               Options...>::value
          == 1)...>::value,
      "LUMEX_PROTOCOL: a protocol option is given twice in "
      "dialogue::options<...>");

  /// The options in the order given.
  using types = ::lumex::core::protocol::dsl::type_list<Options...>;
};

/// @brief `dialogue::options<...>` of a header.
template <typename... Options> using options = options_t<Options...>;
/// @brief `keepalive<Row, PeriodMs>`.
template <typename Row, std::uint32_t PeriodMs>
using keepalive = keepalive_t<Row, PeriodMs>;
/// @brief `watchdog_ms<Ms>`.
template <std::uint32_t Ms> using watchdog_ms = watchdog_ms_t<Ms>;
/// @brief `min_interval_ms<Ms>`.
template <std::uint32_t Ms> using min_interval_ms = min_interval_ms_t<Ms>;
/// @brief `state_from<Row, Field>`.
template <typename Row, std::size_t Field>
using state_from = state_from_t<Row, Field>;
/// @brief `identify_with<Row>`.
template <typename Row> using identify_with = identify_with_t<Row>;
/// @brief `max_in_flight<N>`.
template <std::size_t N> using max_in_flight = max_in_flight_t<N>;
/// @brief `default_timeout_ms<Ms>`.
template <std::uint32_t Ms>
using default_timeout_ms = default_timeout_ms_t<Ms>;
/// @brief `default_retries<N>`.
template <std::uint32_t N> using default_retries = default_retries_t<N>;
/// @brief `late_reply_window_ms<Ms>`.
template <std::uint32_t Ms>
using late_reply_window_ms = late_reply_window_ms_t<Ms>;

namespace detail
{
template <typename T> struct is_options_t : std::false_type
{
};

template <typename... O> struct is_options_t<options_t<O...>> : std::true_type
{
};

// The header member dialogue_options, found by its name: a plain type or an
// alias template over the layer.
LUMEX_PROTOCOL_DETAIL_NAME_PROBE (dialogue_options);

template <typename H, typename P, typename = void>
struct options_template_t : std::false_type
{
};

template <typename H, typename P>
struct options_template_t<
    H, P, typename voider_t<typename H::template dialogue_options<P>>::type>
    : std::true_type
{
};

template <typename H, typename = void>
struct options_member_t : std::false_type
{
};

template <typename H>
struct options_member_t<H,
                        typename voider_t<typename H::dialogue_options>::type>
    : std::true_type
{
};

// 1 an alias template, 2 a type (both found by a valid use, a final header
// too), 3 named but not usable, 0 none. The name of a member of a final
// header cannot be probed: an unusable member there is ignored.
template <typename P, typename H = typename P::header_type>
struct options_form_t
    : std::integral_constant<
          int, options_template_t<H, P>::value
                   ? 1
                   : (options_member_t<H>::value
                          ? 2
                          : (names_dialogue_options_t<H>::value ? 3 : 0))>
{
};

template <typename P, int Form = options_form_t<P>::value>
struct options_select_t
{
  using type = options_t<>;
};

template <typename P> struct options_select_t<P, 1>
{
  using type = typename P::header_type::template dialogue_options<P>;
};

template <typename P> struct options_select_t<P, 2>
{
  using type = typename P::header_type::dialogue_options;
};

template <typename P> struct options_select_t<P, 3>
{
  static_assert (dependent_false_t<P>::value,
                 "LUMEX_PROTOCOL: the header names dialogue_options, but not "
                 "as an accessible type (using dialogue_options = "
                 "dialogue::options<...>;) or alias template (template "
                 "<typename P> using dialogue_options = ...;), or it names a "
                 "row the layer does not have");
  using type = options_t<>;
};
} // namespace detail

/**
 * @brief `options_of_t<P>::type`: the protocol options of layer `P`, read
 * from its header (`dialogue_options`, a type or an alias template over the
 * layer), `options_t<>` when the header names none.
 */
template <typename P> struct options_of_t
{
  /// The options.
  using type = typename detail::options_select_t<P>::type;
  static_assert (detail::is_options_t<type>::value,
                 "LUMEX_PROTOCOL: the dialogue_options of a header is a "
                 "dialogue::options<...>");
};

namespace detail
{
template <typename K, typename List> struct find_option_t
{
  using type = void;
};

template <typename K, typename Head, typename... Tail>
struct find_option_t<K, dsl::type_list<Head, Tail...>>
    : std::conditional<
          std::is_same<typename protocol_kind_t<Head>::type, K>::value,
          type_is_t<Head>, find_option_t<K, dsl::type_list<Tail...>>>::type
{
};

template <typename O, std::uint64_t Default> struct option_number_t
{
  static LUMEX_CONSTEXPR std::uint64_t
  get () LUMEX_NOEXCEPT
  {
    return O::number ();
  }
};

template <std::uint64_t Default> struct option_number_t<void, Default>
{
  static LUMEX_CONSTEXPR std::uint64_t
  get () LUMEX_NOEXCEPT
  {
    return Default;
  }
};

template <typename O> struct option_row_t
{
  using type = typename O::row_type;
};

template <> struct option_row_t<void>
{
  using type = void;
};

template <typename P, typename K>
struct protocol_option_t
    : find_option_t<K, typename options_of_t<P>::type::types>
{
};

// Whether row R is a row of P or of a layer P extends.
template <typename P, typename R,
          bool Root = std::is_void<typename P::base_protocol>::value>
struct row_in_chain_t
    : std::integral_constant<
          bool,
          dsl::list_contains<typename dsl::commands_of<P>::type, R>::value
              || row_in_chain_t<typename P::base_protocol, R>::value>
{
};

template <typename P, typename R>
struct row_in_chain_t<P, R, true>
    : dsl::list_contains<typename dsl::commands_of<P>::type, R>
{
};

// The fields of a reply: 0 none or a reply kind, 1 a layout, 2 one field.
template <typename R>
struct reply_fields_form_t
    : std::integral_constant<int,
                             std::is_same<R, dsl::no_reply>::value
                                 ? 0
                                 : (dsl::is_layout<R>::value
                                        ? 1
                                        : (field::is_field<R>::value ? 2 : 0))>
{
};

template <typename R, int Form = reply_fields_form_t<R>::value>
struct reply_fields_t
{
  using types = dsl::type_list<>;
};

template <typename R> struct reply_fields_t<R, 1>
{
  using types = typename R::fields_type::types;
};

template <typename R> struct reply_fields_t<R, 2>
{
  using types = dsl::type_list<R>;
};

template <typename R>
struct reply_field_count_t : dsl::list_size<typename reply_fields_t<R>::types>
{
};

// Whether field I of reply R holds an integer, a flag or an enumeration
// (false when there is no such field).
template <typename R, std::size_t I,
          bool InRange = (I < reply_field_count_t<R>::value)>
struct level_field_t : std::false_type
{
};

template <typename R, std::size_t I>
struct level_field_t<R, I, true>
    : std::integral_constant<
          bool,
          std::is_integral<typename field::value_of<typename dsl::list_at<
              I, typename reply_fields_t<R>::types>::type>::type>::value
              || std::is_enum<typename field::value_of<typename dsl::list_at<
                  I, typename reply_fields_t<R>::types>::type>::type>::value>
{
};

template <typename Row, typename K>
struct row_has_t : dsl::has_option_type<K, typename Row::options_type>
{
};

template <typename T> struct is_edge_event_t : std::false_type
{
};

template <std::size_t F, std::int64_t A, std::int64_t B>
struct is_edge_event_t<edge_event_t<F, A, B>> : std::true_type
{
};

template <typename Out, typename... T> struct edge_filter_t
{
  using type = Out;
};

template <typename... O, typename Head, typename... Tail>
struct edge_filter_t<dsl::type_list<O...>, Head, Tail...>
    : edge_filter_t<typename std::conditional<is_edge_event_t<Head>::value,
                                              dsl::type_list<O..., Head>,
                                              dsl::type_list<O...>>::type,
                    Tail...>
{
};

template <typename Set> struct edges_of_t;

template <typename... T> struct edges_of_t<dsl::option_set<T...>>
{
  using type = typename edge_filter_t<dsl::type_list<>, T...>::type;
};

template <typename Set> struct reply_row_of_t;

template <> struct reply_row_of_t<dsl::option_set<>>
{
  using type = void;
};

template <typename Head, typename... Tail>
struct reply_row_of_t<dsl::option_set<Head, Tail...>>
    : reply_row_of_t<dsl::option_set<Tail...>>
{
};

template <typename R, typename... Tail>
struct reply_row_of_t<dsl::option_set<reply_row_t<R>, Tail...>>
{
  using type = R;
};

// The correlation a row names (by_key when it names none: ask named first).
template <typename Row, bool Present = row_has_t<Row, correlation_t>::value>
struct named_how_t
{
  using named = std::false_type;

  static LUMEX_CONSTEXPR correlation
  get () LUMEX_NOEXCEPT
  {
    return correlation::by_key;
  }
};

template <typename Row> struct named_how_t<Row, true>
{
  using named = std::true_type;

  static LUMEX_CONSTEXPR correlation
  get () LUMEX_NOEXCEPT
  {
    return dsl::get_option<correlation_t> (Row::options ()).how;
  }
};

// Whether a row names correlation `how`.
template <typename Row>
LUMEX_CONSTEXPR bool
names_how (correlation how) LUMEX_NOEXCEPT
{
  return named_how_t<Row>::named::value && named_how_t<Row>::get () == how;
}

// Whether a part of a reply kind is a row (another message, without the
// key of the request).
template <typename Reply> struct any_part_is_row_t : std::false_type
{
};

template <typename A, typename N>
struct any_part_is_row_t<ack_nak_t<A, N>>
    : std::integral_constant<bool, part_form_t<A>::value == 1
                                       || part_form_t<N>::value == 1>
{
};

template <typename A, typename C>
struct any_part_is_row_t<two_phase_t<A, C>>
    : std::integral_constant<bool, part_form_t<A>::value == 1
                                       || part_form_t<C>::value == 1>
{
};

template <typename Row> struct how_of_t
{
  static LUMEX_CONSTEXPR correlation
  get () LUMEX_NOEXCEPT
  {
    return named_how_t<Row>::named::value ? named_how_t<Row>::get ()
           : row_has_t<Row, reply_row_kind_t>::value
               ? correlation::by_reply_key
           : row_has_t<Row, match_with_t>::value ? correlation::custom
           : any_part_is_row_t<typename Row::reply_type>::value
               ? correlation::by_order
               : correlation::by_key;
  }
};

template <typename Row, bool Present = row_has_t<Row, min_interval_t>::value>
struct min_interval_of_t
{
  static LUMEX_CONSTEXPR std::uint32_t
  get () LUMEX_NOEXCEPT
  {
    return 0;
  }
};

template <typename Row> struct min_interval_of_t<Row, true>
{
  static LUMEX_CONSTEXPR std::uint32_t
  get () LUMEX_NOEXCEPT
  {
    return dsl::get_option<min_interval_t> (Row::options ()).milliseconds;
  }
};

template <typename Row, bool Present = row_has_t<Row, match_with_t>::value>
struct matcher_of_t
{
  static LUMEX_CONSTEXPR reply_matcher_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename Row> struct matcher_of_t<Row, true>
{
  static LUMEX_CONSTEXPR reply_matcher_t
  get () LUMEX_NOEXCEPT
  {
    return dsl::get_option<match_with_t> (Row::options ()).matcher;
  }
};

template <typename Reply> struct kind_of_reply_t
{
  static LUMEX_CONSTEXPR reply_kind
  get () LUMEX_NOEXCEPT
  {
    return std::is_same<Reply, dsl::no_reply>::value ? reply_kind::none
           : field::is_field<Reply>::value           ? reply_kind::message
                                                     : reply_kind::other;
  }
};

template <typename A, typename N> struct kind_of_reply_t<ack_nak_t<A, N>>
{
  static LUMEX_CONSTEXPR reply_kind
  get () LUMEX_NOEXCEPT
  {
    return reply_kind::ack_nak;
  }
};

template <typename A, typename C> struct kind_of_reply_t<two_phase_t<A, C>>
{
  static LUMEX_CONSTEXPR reply_kind
  get () LUMEX_NOEXCEPT
  {
    return reply_kind::two_phase;
  }
};

template <typename T> struct kind_of_reply_t<echo_set_t<T>>
{
  static LUMEX_CONSTEXPR reply_kind
  get () LUMEX_NOEXCEPT
  {
    return reply_kind::echo_set;
  }
};

template <typename T> struct kind_of_reply_t<echo_zero_on_error_t<T>>
{
  static LUMEX_CONSTEXPR reply_kind
  get () LUMEX_NOEXCEPT
  {
    return reply_kind::echo_zero_on_error;
  }
};

template <typename T> struct kind_of_reply_t<state_instead_of_refusal_t<T>>
{
  static LUMEX_CONSTEXPR reply_kind
  get () LUMEX_NOEXCEPT
  {
    return reply_kind::state_instead_of_refusal;
  }
};

// Where row R named for layer P is: 0 a row of P (or no row), 1 a row of a
// layer P extends that P replaced (the engine finds rows through the index
// of P, so the name would mean the replacement), 2 a row of another
// protocol.
template <typename P, typename R>
struct row_state_t
    : std::integral_constant<
          int, dsl::list_contains<typename dsl::commands_of<P>::type, R>::value
                   ? 0
                   : (row_in_chain_t<P, R>::value ? 1 : 2)>
{
};

template <typename P>
struct row_state_t<P, void> : std::integral_constant<int, 0>
{
};

// The same for a part of a reply kind (a field part is always 0).
template <typename P, typename T, int Form = part_form_t<T>::value>
struct part_state_t : std::integral_constant<int, 0>
{
};

template <typename P, typename T>
struct part_state_t<P, T, 1> : row_state_t<P, T>
{
};

// Whether a part of the reply kind is in state State.
template <typename P, typename Reply, int State>
struct parts_in_state_t : std::false_type
{
};

template <typename P, typename A, typename N, int State>
struct parts_in_state_t<P, ack_nak_t<A, N>, State>
    : std::integral_constant<bool, part_state_t<P, A>::value == State
                                       || part_state_t<P, N>::value == State>
{
};

template <typename P, typename A, typename C, int State>
struct parts_in_state_t<P, two_phase_t<A, C>, State>
    : std::integral_constant<bool, part_state_t<P, A>::value == State
                                       || part_state_t<P, C>::value == State>
{
};

// The checks of one edge of a row.
template <typename Row, typename Edge> struct edge_check_t : std::true_type
{
  static_assert (
      row_has_t<Row, dsl::kind::stream>::value,
      "LUMEX_PROTOCOL: edge_event on a row without stream: an edge is seen "
      "in the messages the device sends by itself");
  static_assert (
      Edge::field () < reply_field_count_t<typename Row::reply_type>::value,
      "LUMEX_PROTOCOL: edge_event<Field, From, To>: Field is not a field of "
      "the reply of the row");
  static_assert (
      !(Edge::field () < reply_field_count_t<typename Row::reply_type>::value)
          || level_field_t<typename Row::reply_type, Edge::field ()>::value,
      "LUMEX_PROTOCOL: edge_event<Field, From, To>: the field is an integer, "
      "a flag or an enumeration");
};

template <typename Row, typename List> struct edges_check_t;

template <typename Row, typename... E>
struct edges_check_t<Row, dsl::type_list<E...>>
    : all_t<edge_check_t<Row, E>::value...>
{
};

// The checks of one row of protocol P.
template <typename P, typename Row> struct row_check_t : std::true_type
{
  using reply_row = typename reply_row_of_t<typename Row::options_type>::type;
  using has_reply_row = row_has_t<Row, reply_row_kind_t>;
  using has_matcher = row_has_t<Row, match_with_t>;

  using named = typename named_how_t<Row>::named;

  static_assert (
      !named::value || !has_reply_row::value
          || names_how<Row> (correlation::by_reply_key),
      "LUMEX_PROTOCOL: reply_row<R> needs correlation::by_reply_key "
      "(or no correlate)");
  static_assert (!names_how<Row> (correlation::by_reply_key)
                     || has_reply_row::value,
                 "LUMEX_PROTOCOL: correlation::by_reply_key needs "
                 "reply_row<R> (the row whose message answers)");
  static_assert (!names_how<Row> (correlation::custom) || has_matcher::value,
                 "LUMEX_PROTOCOL: correlation::custom needs match_with "
                 "(function)");
  static_assert (!named::value || !has_matcher::value
                     || names_how<Row> (correlation::custom),
                 "LUMEX_PROTOCOL: match_with needs correlation::custom (or "
                 "no correlate)");
  static_assert (!(has_reply_row::value && has_matcher::value),
                 "LUMEX_PROTOCOL: a row names reply_row<R> or match_with, not "
                 "both");
  static_assert (
      !has_reply_row::value
          || std::is_same<typename Row::reply_type, dsl::no_reply>::value,
      "LUMEX_PROTOCOL: a row with reply_row<R> leaves its reply "
      "group empty: the message of R is its reply");
  static_assert (row_state_t<P, reply_row>::value != 2,
                 "LUMEX_PROTOCOL: reply_row<R> names a row of another "
                 "protocol (R is a row of this protocol or of its base)");
  static_assert (row_state_t<P, reply_row>::value != 1,
                 "LUMEX_PROTOCOL: reply_row<R> names a row the layer "
                 "replaced: list the row that names it in the layer too, so "
                 "that it names the row of the layer");
  static_assert (!parts_in_state_t<P, typename Row::reply_type, 2>::value,
                 "LUMEX_PROTOCOL: a reply kind of the row names a row of "
                 "another protocol (an acknowledgement or a completion is a "
                 "row of this protocol or of its base)");
  static_assert (!parts_in_state_t<P, typename Row::reply_type, 1>::value,
                 "LUMEX_PROTOCOL: a reply kind of the row names a row the "
                 "layer replaced: list the row that names it in the layer "
                 "too, so that it names the row of the layer");
  static_assert (
      !named::value
          || static_cast<int> (named_how_t<Row>::get ())
                 <= static_cast<int> (correlation::custom),
      "LUMEX_PROTOCOL: correlate (how) takes a correlation: by_key, "
      "by_order, by_reply_key or custom");
  static_assert (!names_how<Row> (correlation::by_key)
                     || !any_part_is_row_t<typename Row::reply_type>::value,
                 "LUMEX_PROTOCOL: correlation::by_key on a row whose reply "
                 "kind names a row: that message does not carry the key of "
                 "the request (by_order is the default)");
  static_assert (
      how_of_t<Row>::get () != correlation::by_order
          || option_number_t<
                 typename protocol_option_t<P, max_in_flight_kind_t>::type,
                 1>::get ()
                 == 1,
      "LUMEX_PROTOCOL: a by_order row in a protocol with max_in_flight "
      "greater than 1: a reply without the key of its request cannot be "
      "told from the reply of another request");
  static_assert (
      edges_check_t<
          Row, typename edges_of_t<typename Row::options_type>::type>::value,
      "LUMEX_PROTOCOL: the edge events of the row are valid");
};

template <typename P, typename List> struct rows_check_t;

template <typename P, typename... Rows>
struct rows_check_t<P, dsl::type_list<Rows...>>
    : all_t<row_check_t<P, Rows>::value...>
{
};

// The checks of one protocol option for layer P.
template <typename P, typename O> struct option_check_t : std::true_type
{
};

template <typename P, typename Row, std::uint32_t Ms>
struct option_check_t<P, keepalive_t<Row, Ms>> : std::true_type
{
  static_assert (row_state_t<P, Row>::value != 2,
                 "LUMEX_PROTOCOL: keepalive<Row, PeriodMs> names a row of "
                 "another protocol");
  static_assert (
      row_state_t<P, Row>::value != 1,
      "LUMEX_PROTOCOL: keepalive<Row, PeriodMs> names a row the layer "
      "replaced: name it through the layer (typename P::ROW_t)");
};

template <typename P, typename Row>
struct option_check_t<P, identify_with_t<Row>> : std::true_type
{
  static_assert (row_state_t<P, Row>::value != 2,
                 "LUMEX_PROTOCOL: identify_with<Row> names a row of another "
                 "protocol");
  static_assert (row_state_t<P, Row>::value != 1,
                 "LUMEX_PROTOCOL: identify_with<Row> names a row the layer "
                 "replaced: name it through the layer (typename P::ROW_t)");
  static_assert (reply_form_t<Row>::value == 1,
                 "LUMEX_PROTOCOL: identify_with<Row>: the row has a reply of "
                 "the core (a field or a layout), the identity the client "
                 "reads");
};

template <typename P, typename Row, std::size_t Field>
struct option_check_t<P, state_from_t<Row, Field>> : std::true_type
{
  static_assert (row_state_t<P, Row>::value != 2,
                 "LUMEX_PROTOCOL: state_from<Row, Field> names a row of "
                 "another protocol");
  static_assert (
      row_state_t<P, Row>::value != 1,
      "LUMEX_PROTOCOL: state_from<Row, Field> names a row the layer "
      "replaced: name it through the layer (typename P::ROW_t)");
  static_assert (
      Field < reply_field_count_t<typename Row::reply_type>::value,
      "LUMEX_PROTOCOL: state_from<Row, Field>: Field is not a field "
      "of the reply of Row (a field or a layout of the core)");
  static_assert (
      !(Field < reply_field_count_t<typename Row::reply_type>::value)
          || level_field_t<typename Row::reply_type, Field>::value,
      "LUMEX_PROTOCOL: state_from<Row, Field>: the field is an integer or "
      "an enumeration (the state)");
};

// Whether the keepalive period is shorter than the watchdog (true when the
// protocol names only one of them).
template <typename K, typename W> struct keepalive_shorter_t : std::true_type
{
};

template <typename Row, std::uint32_t PeriodMs, std::uint32_t Ms>
struct keepalive_shorter_t<keepalive_t<Row, PeriodMs>, watchdog_ms_t<Ms>>
    : std::integral_constant<bool, (PeriodMs < Ms)>
{
};

template <typename P>
struct keepalive_fits_t
    : keepalive_shorter_t<
          typename protocol_option_t<P, keepalive_kind_t>::type,
          typename protocol_option_t<P, watchdog_kind_t>::type>
{
};

template <typename P, typename Options> struct options_check_t;

template <typename P, typename... O>
struct options_check_t<P, options_t<O...>>
    : all_t<option_check_t<P, O>::value...>
{
};

// The value a row gives the client: the message of reply_row<R>, else by
// the kind of its reply.
template <typename Row, typename ReplyRow, int Form>
struct reply_value_select_t : message_value_t<ReplyRow>
{
};

// The value of a reply kind: its value_type (a kind of the dialogue), void
// for a kind of another layer the client cannot return.
template <typename Reply, typename = void> struct kind_value_t
{
  using type = void;
};

template <typename Reply>
struct kind_value_t<Reply, typename voider_t<typename Reply::value_type>::type>
{
  using type = typename Reply::value_type;
};

template <typename Row, int Form> struct reply_value_select_t<Row, void, Form>
{
  using type = typename kind_value_t<typename Row::reply_type>::type;
};

template <typename Row> struct reply_value_select_t<Row, void, 0>
{
  using type = field::record<>;
};

template <typename Row> struct reply_value_select_t<Row, void, 1>
{
  using type = typename layout::reply_value_of<Row>::type;
};
} // namespace detail

/**
 * @brief `check_protocol_t<P>::value`: true when every dialogue word of
 * layer `P` is used as the dialogue needs it; otherwise it does not compile,
 * with the message of the failed check.
 * @details The checks: `reply_row<R>`, the rows a reply kind names, the
 * rows of the protocol options are rows of `P` (a row of a base that `P`
 * replaced does not pass: the layer lists the naming row again, and a
 * protocol option names the row through the layer); `correlate
 * (by_reply_key)` comes with `reply_row<R>` and `correlate (custom)` with
 * `match_with`, and neither with another correlation; `by_key` is not on a
 * row whose reply kind names a row; a `by_order` row needs `max_in_flight`
 * of 1; `correlate` takes a value of the enumeration; a row with
 * `reply_row<R>` has no reply group of its own; `edge_event` is on a
 * `stream` row and names a reply field that holds an integer, a flag or an
 * enumeration; `state_from` names such a field; `identify_with` names a row
 * whose reply is a field or a layout; a `keepalive` period is shorter than
 * `watchdog_ms`. `index::build_index<P>` asks for it through the vocabulary
 * (`lumex_check_protocol`), and so do the traits.
 */
template <typename P> struct check_protocol_t : std::true_type
{
  static_assert (::lumex::core::protocol::dsl::is_protocol<P>::value,
                 "LUMEX_PROTOCOL: check_protocol_t<P>: P is a "
                 "LUMEX_PROTOCOL_DEFINE or LUMEX_PROTOCOL_EXTEND type");
  static_assert (sizeof (typename options_of_t<P>::type) != 0,
                 "LUMEX_PROTOCOL: the protocol options of the dialogue are a "
                 "complete type");
  static_assert (
      detail::options_check_t<P, typename options_of_t<P>::type>::value,
      "LUMEX_PROTOCOL: the protocol options of the dialogue are valid");
  static_assert (
      detail::keepalive_fits_t<P>::value,
      "LUMEX_PROTOCOL: keepalive<Row, PeriodMs> with a period not shorter "
      "than watchdog_ms<Ms>: the watchdog would end the link between two "
      "keepalives");
  static_assert (
      detail::rows_check_t<
          P,
          typename ::lumex::core::protocol::dsl::commands_of<P>::type>::value,
      "LUMEX_PROTOCOL: the rows of the protocol are valid for the dialogue");
};

/**
 * @brief What the dialogue needs to know of row `Row` of layer `P`, at
 * compile time.
 */
template <typename P, typename Row> struct row_traits_t
{
  static_assert (detail::row_check_t<P, Row>::value,
                 "LUMEX_PROTOCOL: the row is valid for the dialogue");

  /// The row whose message answers (`reply_row<R>`), `void` without one.
  using reply_row =
      typename detail::reply_row_of_t<typename Row::options_type>::type;
  /// The edges of the row (`edge_event_t` types) in table order.
  using edge_events =
      typename detail::edges_of_t<typename Row::options_type>::type;
  /// What the client returns for the row.
  using value_type = typename detail::reply_value_select_t<
      Row, reply_row, detail::reply_form_t<Row>::value>::type;

  /// @brief The correlation: the one the row names; else `by_reply_key`
  /// with `reply_row<R>`, `custom` with `match_with`, `by_order` when a
  /// part of the reply kind is another message (an acknowledgement row), and
  /// `by_key` otherwise.
  static LUMEX_CONSTEXPR correlation
  how () LUMEX_NOEXCEPT
  {
    return detail::how_of_t<Row>::get ();
  }

  /// @brief What the reply of the row is.
  static LUMEX_CONSTEXPR reply_kind
  kind () LUMEX_NOEXCEPT
  {
    return detail::kind_of_reply_t<typename Row::reply_type>::get ();
  }

  /// @brief `min_interval (ms)` of the row, 0 without it.
  static LUMEX_CONSTEXPR std::uint32_t
  min_interval_ms () LUMEX_NOEXCEPT
  {
    return detail::min_interval_of_t<Row>::get ();
  }

  /// @brief The function of `match_with`, `nullptr` without it.
  static LUMEX_CONSTEXPR reply_matcher_t
  matcher () LUMEX_NOEXCEPT
  {
    return detail::matcher_of_t<Row>::get ();
  }
};

/**
 * @brief `reply_value_of_t<Row>::type`: what the client returns for `Row`:
 * the value of a field or a layout reply, `field::record<>` without a reply,
 * the value of the message of `reply_row<R>`, the `value_type` of a reply
 * kind of the dialogue, `void` for a reply kind of another layer.
 */
template <typename Row> struct reply_value_of_t
{
  /// The value.
  using type = typename detail::reply_value_select_t<
      Row, typename detail::reply_row_of_t<typename Row::options_type>::type,
      detail::reply_form_t<Row>::value>::type;
};

/**
 * @brief The protocol options of layer `P` as values, with their defaults
 * (one request in flight, 0 for an option that is not given).
 */
template <typename P> struct protocol_traits_t
{
  static_assert (check_protocol_t<P>::value,
                 "LUMEX_PROTOCOL: the protocol is valid for the dialogue");

  /// The options (`options_t<...>`).
  using options = typename options_of_t<P>::type;
  /// The row of `keepalive`, `void` without it.
  using keepalive_row =
      typename detail::option_row_t<typename detail::protocol_option_t<
          P, detail::keepalive_kind_t>::type>::type;
  /// The row of `state_from`, `void` without it.
  using state_row =
      typename detail::option_row_t<typename detail::protocol_option_t<
          P, detail::state_from_kind_t>::type>::type;
  /// The row of `identify_with`, `void` without it.
  using identify_row =
      typename detail::option_row_t<typename detail::protocol_option_t<
          P, detail::identify_with_kind_t>::type>::type;

  /// @brief `max_in_flight<N>`, 1 without it.
  static LUMEX_CONSTEXPR std::size_t
  max_in_flight () LUMEX_NOEXCEPT
  {
    return static_cast<std::size_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::max_in_flight_kind_t>::type,
                                1>::get ());
  }

  /// @brief The period of `keepalive`, 0 without it.
  static LUMEX_CONSTEXPR std::uint32_t
  keepalive_period_ms () LUMEX_NOEXCEPT
  {
    return static_cast<std::uint32_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::keepalive_kind_t>::type,
                                0>::get ());
  }

  /// @brief `watchdog_ms<Ms>`, 0 without it (no watchdog).
  static LUMEX_CONSTEXPR std::uint32_t
  watchdog_ms () LUMEX_NOEXCEPT
  {
    return static_cast<std::uint32_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::watchdog_kind_t>::type,
                                0>::get ());
  }

  /// @brief `min_interval_ms<Ms>`, 0 without it (no rate limit).
  static LUMEX_CONSTEXPR std::uint32_t
  min_interval_ms () LUMEX_NOEXCEPT
  {
    return static_cast<std::uint32_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::min_interval_kind_t>::type,
                                0>::get ());
  }

  /// @brief The field of `state_from`, 0 without it.
  static LUMEX_CONSTEXPR std::size_t
  state_field () LUMEX_NOEXCEPT
  {
    return static_cast<std::size_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::state_from_kind_t>::type,
                                0>::get ());
  }

  /// @brief `default_timeout_ms<Ms>`, 0 without it (the engine's default).
  static LUMEX_CONSTEXPR std::uint32_t
  default_timeout_ms () LUMEX_NOEXCEPT
  {
    return static_cast<std::uint32_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::default_timeout_kind_t>::type,
                                0>::get ());
  }

  /// @brief `default_retries<N>`, 0 without it.
  static LUMEX_CONSTEXPR std::uint32_t
  default_retries () LUMEX_NOEXCEPT
  {
    return static_cast<std::uint32_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::default_retries_kind_t>::type,
                                0>::get ());
  }

  /// @brief `late_reply_window_ms<Ms>`, 0 without it.
  static LUMEX_CONSTEXPR std::uint32_t
  late_reply_window_ms () LUMEX_NOEXCEPT
  {
    return static_cast<std::uint32_t> (
        detail::option_number_t<typename detail::protocol_option_t<
                                    P, detail::late_reply_window_kind_t>::type,
                                0>::get ());
  }
};

/**
 * @brief The vocabulary of a table with a dialogue: the words of
 * `registry::table_vocabulary` (and so of `dsl::vocabulary`), the reply
 * kinds, the row options, and the check `index::build_index<P>` asks for.
 * @details A header names it as `using lumex_vocabulary =
 * dialogue::vocabulary_t;` (or a class derived from it, with words of its
 * own). Reply kinds: `ack_nak<Ack, Nak>`, `two_phase<Ack, Completion>`,
 * `echo_set<T>`, `echo_zero_on_error<T>`, `state_instead_of_refusal<T>`.
 * Row options: `correlate (correlation::...)`, `reply_row<R> ()`,
 * `min_interval (ms)`, `edge_event<Field, From, To> ()`, `match_with
 * (function)`.
 */
struct vocabulary_t : ::lumex::core::protocol::registry::table_vocabulary
{
  /// @brief Reply kind `ack_nak<Ack, Nak>`.
  template <typename Ack, typename Nak> using ack_nak = ack_nak_t<Ack, Nak>;

  /// @brief Reply kind `two_phase<Ack, Completion>`.
  template <typename Ack, typename Completion>
  using two_phase = two_phase_t<Ack, Completion>;

  /// @brief Reply kind `echo_set<T>`.
  template <typename T> using echo_set = echo_set_t<T>;

  /// @brief Reply kind `echo_zero_on_error<T>`.
  template <typename T> using echo_zero_on_error = echo_zero_on_error_t<T>;

  /// @brief Reply kind `state_instead_of_refusal<T>`.
  template <typename T>
  using state_instead_of_refusal = state_instead_of_refusal_t<T>;

  /// @brief The correlations of `correlate (correlation::by_order)`.
  using correlation
      = ::lumex::applied::transport::protocol::dialogue::correlation;

  /// @brief Row option: how the reply of the row is matched.
  static LUMEX_CONSTEXPR correlation_t
  correlate (correlation how) LUMEX_NOEXCEPT
  {
    return correlation_t (how);
  }

  /// @brief Row option `reply_row<R> ()`: the message of `R` answers.
  template <typename Row> using reply_row = reply_row_t<Row>;

  /// @brief Row option: at least `milliseconds` between two requests of the
  /// row.
  static LUMEX_CONSTEXPR min_interval_t
  min_interval (std::uint32_t milliseconds) LUMEX_NOEXCEPT
  {
    return min_interval_t (milliseconds);
  }

  /// @brief Row option `edge_event<Field, From, To> ()` of a stream row.
  template <std::size_t Field, std::int64_t From, std::int64_t To>
  using edge_event = edge_event_t<Field, From, To>;

  /// @brief Row option: `matcher` decides which message answers the row.
  static LUMEX_CONSTEXPR match_with_t
  match_with (reply_matcher_t matcher) LUMEX_NOEXCEPT
  {
    return match_with_t (matcher);
  }

  /// @brief The check the index makes of a protocol with this vocabulary.
  template <typename P> using lumex_check_protocol = check_protocol_t<P>;
};
} // namespace dialogue
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DSL_DIALOGUE_VOCABULARY_HPP
