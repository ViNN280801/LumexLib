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
 * @file LumexDialogueFeatures.hpp
 * @brief From the table to the run-time description the non-template engine
 * reads: `dialogue::features_of<P> ()`, `dialogue::options_for<P> ()`, and
 * `dialogue::keepalive_from<P> (source)`, the generator of the keepalive
 * requests from a function that gives their arguments.
 * @details `features_of<P> ()` instantiates a reader of every row with a reply
 * kind of layer `P`, and a decoder of every row with edges and of the row of
 * `state_from`: the cost of the rows that use the features, once per layer.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_FEATURES_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_FEATURES_HPP

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexReplyReader.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/field/LumexProtocolContext.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolMessage.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
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
// A value as a level: the check of the table lets only integers, flags and
// enumerations come here at run time.
template <typename T,
          bool Level = std::is_integral<T>::value || std::is_enum<T>::value>
struct level_of_t
{
  static std::int64_t
  get (T const &value) LUMEX_NOEXCEPT
  {
    return static_cast<std::int64_t> (value);
  }
};

template <typename T> struct level_of_t<T, false>
{
  static std::int64_t
  get (T const &) LUMEX_NOEXCEPT
  {
    return 0;
  }
};

// Field `wanted` of a record.
template <typename Record, std::size_t I = 0,
          bool More = (I < Record::size::value)>
struct record_level_t
{
  static std::int64_t
  get (Record const &record, std::size_t wanted) LUMEX_NOEXCEPT
  {
    using value_type =
        typename std::decay<decltype (::lumex::core::protocol::field::get<I> (
            record))>::type;
    return wanted == I ? level_of_t<value_type>::get (
                             ::lumex::core::protocol::field::get<I> (record))
                       : record_level_t<Record, I + 1>::get (record, wanted);
  }
};

template <typename Record, std::size_t I>
struct record_level_t<Record, I, false>
{
  static std::int64_t
  get (Record const &, std::size_t) LUMEX_NOEXCEPT
  {
    return 0;
  }
};

// The level of a decoded reply: a layout is a record, a field its value.
template <typename R, typename Value,
          bool Layout = (reply_fields_form_t<R>::value == 1)>
struct reply_level_t
{
  static std::int64_t
  get (Value const &value, std::size_t) LUMEX_NOEXCEPT
  {
    return level_of_t<Value>::get (value);
  }
};

template <typename R, typename Value> struct reply_level_t<R, Value, true>
{
  static std::int64_t
  get (Value const &value, std::size_t wanted) LUMEX_NOEXCEPT
  {
    return record_level_t<Value>::get (value, wanted);
  }
};

template <typename P, typename Row> struct levels_of_t
{
  static bool
  read (layout::bytes_view_t message, field::decode_context_t const *context,
        std::size_t const *fields, std::size_t count, std::int64_t *values)
  {
    using value_type = typename layout::reply_message_of<Row>::type;
    layout::decode_result_t<value_type> const decoded
        = layout::decode_reply_message<P, Row> (message, context);
    if (!decoded)
      return false;
    for (std::size_t i = 0; i < count; ++i)
      values[i] = reply_level_t<typename Row::reply_type, value_type>::get (
          *decoded, fields[i]);
    return true;
  }
};

// The level reader of a row with edges or of the row of the state; null for
// the other rows, whose decoder is then not instantiated.
template <typename P, typename Row, bool Needed> struct level_reader_of_t
{
  static level_reader_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename Row> struct level_reader_of_t<P, Row, true>
{
  static level_reader_t
  get () LUMEX_NOEXCEPT
  {
    return &levels_of_t<P, Row>::read;
  }
};

// The edges of a row as a static array.
template <typename List> struct edges_table_t;

template <> struct edges_table_t<dsl::type_list<>>
{
  static edge_spec_t const *
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }

  static LUMEX_CONSTEXPR std::size_t
  count () LUMEX_NOEXCEPT
  {
    return 0;
  }
};

template <typename E>
edge_spec_t
edge_spec_of () LUMEX_NOEXCEPT
{
  edge_spec_t out;
  out.field = E::field ();
  out.from = E::from ();
  out.to = E::to ();
  return out;
}

template <typename... E> struct edges_table_t<dsl::type_list<E...>>
{
  static edge_spec_t const *
  get ()
  {
    static edge_spec_t const items[sizeof...(E)] = { edge_spec_of<E> ()... };
    return items;
  }

  static LUMEX_CONSTEXPR std::size_t
  count () LUMEX_NOEXCEPT
  {
    return sizeof...(E);
  }
};

template <typename Row> struct identifier_or_null_t
{
  static char const *
  get () LUMEX_NOEXCEPT
  {
    return Row::identifier ();
  }
};

template <> struct identifier_or_null_t<void>
{
  static char const *
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

// The message of a row without request fields; null for the others.
template <typename P, typename Row,
          bool Empty
          = dsl::list_size<typename Row::fields_type::types>::value == 0>
struct empty_request_of_t
{
  static empty_request_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename Row> struct empty_request_of_t<P, Row, true>
{
  static std::error_code
  encode (layout::bytes_t &message, field::decode_context_t const &context)
  {
    layout::encode_result_t const encoded
        = layout::encode_message<P, Row> (message, context);
    return encoded ? std::error_code () : encoded.error ();
  }

  static empty_request_t
  get () LUMEX_NOEXCEPT
  {
    return &encode;
  }
};

template <typename P, typename Row>
row_features_t
row_features_of ()
{
  using traits = row_traits_t<P, Row>;
  using edges = edges_table_t<typename traits::edge_events>;
  row_features_t out;
  out.identifier = Row::identifier ();
  out.how = traits::how ();
  out.reply_row = identifier_or_null_t<typename traits::reply_row>::get ();
  out.matcher = traits::matcher ();
  out.reader = reply_reader_of<P, Row> ();
  out.min_interval_ms = traits::min_interval_ms ();
  out.edges = edges::get ();
  out.edge_count = edges::count ();
  using reads_levels = std::integral_constant<
      bool,
      (edges::count () != 0
       || std::is_same<Row, typename protocol_traits_t<P>::state_row>::value)>;
  out.read_levels = level_reader_of_t<P, Row, reads_levels::value>::get ();
  out.enables = identifier_or_null_t<typename traits::enabled_row>::get ();
  out.empty_request = empty_request_of_t<P, Row>::get ();
  return out;
}

template <typename P, typename List> struct rows_table_t;

template <typename P, typename... Rows>
struct rows_table_t<P, dsl::type_list<Rows...>>
{
  static row_features_t const *
  get ()
  {
    // One more: a table without rows has a place too.
    static row_features_t const items[sizeof...(Rows) + 1]
        = { row_features_of<P, Rows> ()..., row_features_t () };
    return items;
  }

  static LUMEX_CONSTEXPR std::size_t
  count () LUMEX_NOEXCEPT
  {
    return sizeof...(Rows);
  }
};

template <typename P>
protocol_features_t
protocol_features_of ()
{
  using traits = protocol_traits_t<P>;
  using rows = rows_table_t<P, typename dsl::commands_of<P>::type>;
  protocol_features_t out;
  out.rows = rows::get ();
  out.row_count = rows::count ();
  out.keepalive_row
      = identifier_or_null_t<typename traits::keepalive_row>::get ();
  out.keepalive_period_ms = traits::keepalive_period_ms ();
  out.watchdog_ms = traits::watchdog_ms ();
  out.min_interval_ms = traits::min_interval_ms ();
  out.state_row = identifier_or_null_t<typename traits::state_row>::get ();
  out.state_field = traits::state_field ();
  return out;
}

// The generator of keepalive requests over a function giving a tuple of the
// arguments of Row.
template <typename P, typename Row, typename Source> struct keepalive_source_t
{
  Source source;

  template <typename Tuple, std::size_t... I>
  static std::error_code
  encode (layout::bytes_t &message, field::decode_context_t const &context,
          Tuple const &arguments,
          ::lumex::core::utility::sequence::index_sequence<I...>)
  {
    layout::encode_result_t const encoded = layout::encode_message<P, Row> (
        message, context, std::get<I> (arguments)...);
    return encoded ? std::error_code () : encoded.error ();
  }

  std::error_code
  operator() (layout::bytes_t &message, field::decode_context_t const &context)
  {
    using tuple_type = decltype (source ());
    tuple_type const arguments = source ();
    return encode (message, context, arguments,
                   ::lumex::core::utility::sequence::make_index_sequence<
                       std::tuple_size<tuple_type>::value> ());
  }
};
} // namespace detail

/**
 * @brief What the table of layer `P` says of the features of the engine, for
 * `dialogue_options_t::features`; static data, made once.
 */
template <typename P>
protocol_features_t const *
features_of ()
{
  static protocol_features_t const made = detail::protocol_features_of<P> ();
  return &made;
}

/**
 * @brief The options of an engine as the protocol options of layer `P` say:
 * `max_in_flight`, `default_timeout_ms`, `default_retries`,
 * `late_reply_window_ms` and the features; the rest as by default.
 */
template <typename P>
dialogue_options_t
options_for ()
{
  using traits = protocol_traits_t<P>;
  dialogue_options_t out;
  out.max_in_flight = traits::max_in_flight ();
  if (traits::default_timeout_ms () != 0)
    out.defaults.timeout_ms = traits::default_timeout_ms ();
  out.defaults.retries = traits::default_retries ();
  out.late_reply_window_ms = traits::late_reply_window_ms ();
  out.features = features_of<P> ();
  return out;
}

/**
 * @brief The generator of the keepalive requests of layer `P`
 * (`dialogue_engine::set_keepalive`): `source` gives the arguments of the row
 * of `keepalive<Row, PeriodMs>` as a `std::tuple` each time one is sent.
 */
template <typename P, typename Source>
keepalive_generator_t
keepalive_from (Source source)
{
  using row_type = typename protocol_traits_t<P>::keepalive_row;
  static_assert (!std::is_void<row_type>::value,
                 "LUMEX_PROTOCOL: keepalive_from<P>: the protocol names no "
                 "keepalive<Row, PeriodMs>");
  detail::keepalive_source_t<P, row_type, Source> made
      = { std::move (source) };
  return keepalive_generator_t (std::move (made));
}
} // namespace dialogue
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_FEATURES_HPP
