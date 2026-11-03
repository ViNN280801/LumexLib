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
 * @file LumexReplyReader.hpp
 * @brief `dialogue::reply_reader_of<P, Row> ()`: the reader the dialogue
 * engine applies to a frame that answers an exchange of a row with a reply
 * kind, so that the non-template engine knows what the frame does.
 * @details The reader classifies the frame with `decode_reply_kind<P, Row>`
 * (the classification `describe` writes) and decides:
 * - `ack_nak`: the ACK completes the exchange, the NAK is
 *   `reply_effect::nak` (sent again while the policy allows);
 * - `two_phase`: the acknowledgement is `reply_effect::more` (the engine
 *   waits for the completion), the completion completes the exchange;
 * - `echo_set`: an echo equal to the value sent completes it, another one
 *   fails it with `protocol_errc::not_confirmed`;
 * - `echo_zero_on_error`: a zero echo fails it with `protocol_errc::refused`
 *   when the value sent is not zero, any other echo completes it. Writing 0
 *   and getting 0 back succeeds: a refusal of a write of 0 cannot be told
 *   apart from the echo (a row without request fields has no value sent: a
 *   zero fails it);
 * - `state_instead_of_refusal`: an echo equal to the value sent completes it,
 *   another value (the state of the device) fails it with
 *   `protocol_errc::refused`.
 * A frame that is no part of the kind fails the exchange with the error of
 * the decoder (`unexpected_reply`, `truncated`, ...). The value sent is the
 * last field of the request; an echo whose value is the whole request record
 * (a layout of the same fields) is compared with the record. A row whose
 * reply is no reply kind of the dialogue has no reader (`nullptr`): the
 * engine takes its frame as the reply as it is.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_REPLY_READER_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_REPLY_READER_HPP

#include <cstddef>
#include <system_error>
#include <type_traits>
#include <utility>

#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolMessage.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
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
namespace detail
{
inline reply_reading_t
reading_of (reply_effect effect,
            std::error_code const &error = std::error_code ())
{
  reply_reading_t out;
  out.effect = effect;
  out.error = error;
  return out;
}

inline reply_reading_t
failed_with (::lumex::core::protocol::error::protocol_errc code)
{
  return reading_of (reply_effect::fail,
                     ::lumex::core::protocol::error::make_error_code (code));
}

// The type of the last request field (int for a record without fields: the
// check of the missing field comes first).
template <typename Record, bool Fields = (Record::size::value > 0)>
struct last_field_t
{
  using type = int;
};

template <typename Record> struct last_field_t<Record, true>
{
  using type = typename std::decay<
      decltype (::lumex::core::protocol::field::get<Record::size::value - 1> (
          std::declval<Record const &> ()))>::type;
};

// Whether `A const &` can be compared with `B const &` with `==`.
template <typename A, typename B> struct echo_comparable_t
{
  template <typename X, typename Y>
  static auto test (int)
      -> decltype (std::declval<X const &> () == std::declval<Y const &> (),
                   std::true_type ());
  template <typename X, typename Y> static std::false_type test (...);

  static LUMEX_CONSTEXPR bool const value = decltype (test<A, B> (0))::value;
};

// The value sent: the whole request record when the echo is that record,
// else its last field.
template <typename Echo, typename Record,
          bool Whole = std::is_same<Echo, Record>::value>
struct echo_compare_t
{
  static_assert (Record::size::value > 0,
                 "LUMEX_PROTOCOL: an echo kind (echo_set, "
                 "echo_zero_on_error, state_instead_of_refusal) on a row "
                 "without request fields: there is no value sent to compare "
                 "the echo with");
  static_assert (
      echo_comparable_t<Echo, typename last_field_t<Record>::type>::value,
      "LUMEX_PROTOCOL: the echo of an echo kind can be compared with the last "
      "field of the request (or is the whole request record): the type of "
      "the echo and the type of that field have no operator==");

  static bool
  equal (Echo const &echo, Record const &request)
  {
    return ::lumex::core::protocol::field::get<Record::size::value - 1> (
               request)
           == echo;
  }
};

template <typename Echo, typename Record>
struct echo_compare_t<Echo, Record, true>
{
  static bool
  equal (Echo const &echo, Record const &request)
  {
    return echo == request;
  }
};

// Whether the echo of a decoded reply equals the value of the request.
template <typename P, typename Row, typename Echo>
std::error_code
echo_equals (Echo const &echo, layout::bytes_view_t request,
             field::decode_context_t const *context, bool &equal)
{
  using record_type = typename layout::request_of<Row>::type;
  layout::decode_result_t<record_type> const sent
      = layout::decode_message<P, Row> (request, context);
  if (!sent)
    return sent.error ();
  equal = echo_compare_t<Echo, record_type>::equal (echo, *sent);
  return std::error_code ();
}

// Whether the value sent equals the (zero) echo of echo_zero_on_error; a
// row without request fields has no value sent: never.
template <typename P, typename Row, typename Echo,
          bool Fields = (layout::request_of<Row>::type::size::value > 0)>
struct zero_sent_t
{
  static std::error_code
  apply (Echo const &echo, layout::bytes_view_t request,
         field::decode_context_t const *context, bool &equal)
  {
    return echo_equals<P, Row> (echo, request, context, equal);
  }
};

template <typename P, typename Row, typename Echo>
struct zero_sent_t<P, Row, Echo, false>
{
  static std::error_code
  apply (Echo const &, layout::bytes_view_t, field::decode_context_t const *,
         bool &equal)
  {
    equal = false;
    return std::error_code ();
  }
};

template <typename P, typename Row, typename Reply = typename Row::reply_type>
struct reader_t
{
  static reply_reader_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename Row, typename A, typename N>
struct reader_t<P, Row, ack_nak_t<A, N>>
{
  static reply_reading_t
  read (layout::bytes_view_t message, field::decode_context_t const *context,
        layout::bytes_view_t)
  {
    layout::decode_result_t<decoded_reply_t<ack_nak_t<A, N>>> const decoded
        = decode_reply_kind<P, Row> (message, context);
    if (!decoded)
      return reading_of (reply_effect::fail, decoded.error ());
    return reading_of (decoded->part == reply_part::ack
                           ? reply_effect::complete
                           : reply_effect::nak);
  }

  static reply_reader_t
  get () LUMEX_NOEXCEPT
  {
    return &read;
  }
};

template <typename P, typename Row, typename A, typename C>
struct reader_t<P, Row, two_phase_t<A, C>>
{
  static reply_reading_t
  read (layout::bytes_view_t message, field::decode_context_t const *context,
        layout::bytes_view_t)
  {
    layout::decode_result_t<decoded_reply_t<two_phase_t<A, C>>> const decoded
        = decode_reply_kind<P, Row> (message, context);
    if (!decoded)
      return reading_of (reply_effect::fail, decoded.error ());
    return reading_of (decoded->part == reply_part::ack
                           ? reply_effect::more
                           : reply_effect::complete);
  }

  static reply_reader_t
  get () LUMEX_NOEXCEPT
  {
    return &read;
  }
};

template <typename P, typename Row, typename T>
struct reader_t<P, Row, echo_set_t<T>>
{
  static reply_reading_t
  read (layout::bytes_view_t message, field::decode_context_t const *context,
        layout::bytes_view_t request)
  {
    layout::decode_result_t<decoded_reply_t<echo_set_t<T>>> const decoded
        = decode_reply_kind<P, Row> (message, context);
    if (!decoded)
      return reading_of (reply_effect::fail, decoded.error ());
    bool equal = false;
    std::error_code const error
        = echo_equals<P, Row> (decoded->value, request, context, equal);
    if (error)
      return reading_of (reply_effect::fail, error);
    return equal ? reading_of (reply_effect::complete)
                 : failed_with (::lumex::core::protocol::error::protocol_errc::
                                    not_confirmed);
  }

  static reply_reader_t
  get () LUMEX_NOEXCEPT
  {
    return &read;
  }
};

template <typename P, typename Row, typename T>
struct reader_t<P, Row, echo_zero_on_error_t<T>>
{
  static reply_reading_t
  read (layout::bytes_view_t message, field::decode_context_t const *context,
        layout::bytes_view_t request)
  {
    layout::decode_result_t<decoded_reply_t<echo_zero_on_error_t<T>>> const
        decoded
        = decode_reply_kind<P, Row> (message, context);
    if (!decoded)
      return reading_of (reply_effect::fail, decoded.error ());
    if (decoded->part != reply_part::refused)
      return reading_of (reply_effect::complete);
    // A zero echo: the refusal, unless the value sent was zero.
    bool zero_sent = false;
    std::error_code const error
        = zero_sent_t<P, Row, decltype (decoded->value)>::apply (
            decoded->value, request, context, zero_sent);
    if (error)
      return reading_of (reply_effect::fail, error);
    return zero_sent
               ? reading_of (reply_effect::complete)
               : failed_with (
                     ::lumex::core::protocol::error::protocol_errc::refused);
  }

  static reply_reader_t
  get () LUMEX_NOEXCEPT
  {
    return &read;
  }
};

template <typename P, typename Row, typename T>
struct reader_t<P, Row, state_instead_of_refusal_t<T>>
{
  static reply_reading_t
  read (layout::bytes_view_t message, field::decode_context_t const *context,
        layout::bytes_view_t request)
  {
    layout::decode_result_t<
        decoded_reply_t<state_instead_of_refusal_t<T>>> const decoded
        = decode_reply_kind<P, Row> (message, context);
    if (!decoded)
      return reading_of (reply_effect::fail, decoded.error ());
    bool equal = false;
    std::error_code const error
        = echo_equals<P, Row> (decoded->value, request, context, equal);
    if (error)
      return reading_of (reply_effect::fail, error);
    return equal ? reading_of (reply_effect::complete)
                 : failed_with (
                       ::lumex::core::protocol::error::protocol_errc::refused);
  }

  static reply_reader_t
  get () LUMEX_NOEXCEPT
  {
    return &read;
  }
};
} // namespace detail

/**
 * @brief The reader of the reply kind of `Row` in layer `P` for
 * `exchange_t::reader` (see the file description); `nullptr` for a row whose
 * reply is a field, a layout or none.
 */
template <typename P, typename Row>
reply_reader_t
reply_reader_of () LUMEX_NOEXCEPT
{
  return detail::reader_t<P, Row>::get ();
}
} // namespace dialogue
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_REPLY_READER_HPP
