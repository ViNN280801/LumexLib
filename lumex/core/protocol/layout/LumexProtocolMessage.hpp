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
 * @file LumexProtocolMessage.hpp
 * @brief Whole messages of a protocol row: the header and the key, then the
 * fields. `encode_header`, `encode_message`, `encode_message_record`,
 * `decode_message`, `encode_reply_message`, `decode_reply_message` and
 * `reply_message_of`.
 * @details The encoders of `LumexProtocolCodec.hpp` write and read the
 * fields of a row (and the key prefix of a tokenized layout); the key and
 * the header that precede the fields in the binary and fixed-width layouts
 * are what this file adds. A message is what the frame codec carries (its
 * payload): the bytes before the fields, then the fields. The header type
 * of the layer decides the bytes before the fields:
 * @code
 * struct my_header
 * {
 *   using key_reader = index::key_at<1, 1>;
 *   // Optional: the address from the session, then the key.
 *   static layout::encode_result_t write_header (
 *       layout::bytes_t &out, dsl::key_value_t const &key,
 *       field::decode_context_t const *context);
 * };
 * @endcode
 * A header that names `write_header` writes everything before the fields,
 * the key included, so that the key reader of the layer finds the key it
 * wrote. Otherwise the writer twin of the key reader writes them
 * (`index::key_at<Offset, Width>::write`: `Offset` zero bytes, then the
 * key; `key_at_digits`: `Offset` characters `'0'`, then the digits;
 * `tokenized_key`: nothing, the tokenized layout writes the key itself).
 * A layer whose reader has no writer twin (its own reader type,
 * `no_key_reader`) names `write_header`; the encoders do not compile
 * otherwise. The key given to the header is the key of the row as the wire
 * carries it (an integer cut to `index::key_width_of<P>`).
 *
 * The decoders read the key with the key reader of the layer and check that
 * it is the key of the row, then decode the fields from where the reader
 * says they start: a message without a readable key is `unknown_frame`, a
 * request with another key `unknown_frame`, a reply with another key
 * `unexpected_reply`. A row without a reply has an empty reply message (the
 * header and the key): its value is `field::record<>`. A reply kind of the
 * dialogue layer is no message of the core (`reply_value_of` refuses it).
 * Every encoder leaves the buffer unchanged on failure.
 */
#ifndef LUMEX_CORE_PROTOCOL_LAYOUT_MESSAGE_HPP
#define LUMEX_CORE_PROTOCOL_LAYOUT_MESSAGE_HPP

#include <cstddef>
#include <cstring>
#include <system_error>
#include <type_traits>
#include <utility>

#include "lumex/core/protocol/dsl/LumexProtocolKey.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/index/LumexProtocolKeyReader.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/protocol/layout/LumexProtocolTextLayouts.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-invalidation")
// Clang 23 reports a reference parameter to a container as "invalidated" by
// its own resize (a false positive: only references into the container are
// invalidated, not the container).
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace layout
{
namespace detail
{
template <typename T> struct message_voider
{
  using type = void;
};

// Whether header type H names write_header.
template <typename H, typename = void>
struct has_header_writer : std::false_type
{
};

template <typename H>
struct has_header_writer<
    H, typename message_voider<decltype (H::write_header (
           std::declval<bytes_t &> (),
           std::declval<::lumex::core::protocol::dsl::key_value_t const &> (),
           std::declval<decode_context_t const *> ()))>::type> : std::true_type
{
};

// Whether key reader R has its writer twin.
template <typename R, typename = void> struct has_key_writer : std::false_type
{
};

template <typename R>
struct has_key_writer<
    R, typename message_voider<decltype (R::write (
           std::declval<bytes_t &> (),
           std::declval<
               ::lumex::core::protocol::dsl::key_value_t const &> ()))>::type>
    : std::true_type
{
};

template <typename R> struct is_tokenized_reader : std::false_type
{
};

template <char KeySeparator, bool Trim>
struct is_tokenized_reader<
    ::lumex::core::protocol::index::tokenized_key<KeySeparator, Trim>>
    : std::true_type
{
};

template <typename P>
struct is_tokenized_layer : layout_of<P>::type::writer::is_tokenized
{
};

// The key of Row as the wire of layer P carries it.
template <typename P, typename Row>
::lumex::core::protocol::dsl::key_value_t
wire_key () LUMEX_NOEXCEPT
{
  return ::lumex::core::protocol::dsl::normalize_key (
      ::lumex::core::protocol::dsl::key_value (Row::key ()),
      ::lumex::core::protocol::index::key_width_of<P>::value);
}

inline ::lumex::core::expected::error::unexpected<std::error_code>
message_failed (::lumex::core::protocol::error::protocol_errc code)
{
  return ::lumex::core::expected::error::unexpected<std::error_code> (
      ::lumex::core::protocol::error::make_error_code (code));
}

// The header of layer P: its own write_header, or the writer twin of its key
// reader.
template <typename P,
          bool Own = has_header_writer<typename P::header_type>::value>
struct header_of
{
  static encode_result_t
  write (bytes_t &out, ::lumex::core::protocol::dsl::key_value_t const &key,
         decode_context_t const *context)
  {
    return P::header_type::write_header (out, key, context);
  }
};

template <typename P> struct header_of<P, false>
{
  using reader =
      typename ::lumex::core::protocol::index::key_reader_of<P>::type;
  static_assert (has_key_writer<reader>::value,
                 "LUMEX_PROTOCOL: the key reader of the layer has no writer "
                 "twin (static bool write (bytes_t &, dsl::key_value_t const "
                 "&)): the header type names static encode_result_t "
                 "write_header (bytes_t &, dsl::key_value_t const &, "
                 "decode_context_t const *)");
  static_assert (is_tokenized_reader<reader>::value
                     == is_tokenized_layer<P>::value,
                 "LUMEX_PROTOCOL: a tokenized layer reads its key with "
                 "index::tokenized_key, another layer with another reader; "
                 "a layer that does otherwise names write_header in its "
                 "header type");

  static encode_result_t
  write (bytes_t &out, ::lumex::core::protocol::dsl::key_value_t const &key,
         decode_context_t const *)
  {
    if (!reader::write (out, key))
      return message_failed (
          ::lumex::core::protocol::error::protocol_errc::invalid_value);
    return encode_result_t ();
  }
};

// Whether the key text `text` is the key of Row in a tokenized layer, or a
// text key of Row in any layer.
template <typename K, bool Tokenized, typename = void> struct text_key_match
{
  static bool
  equal (K const &, char const *, std::size_t) LUMEX_NOEXCEPT
  {
    return false;
  }
};

template <typename K, bool Tokenized>
struct text_key_match<
    K, Tokenized,
    typename std::enable_if<
        std::is_same<K, ::lumex::core::protocol::dsl::text_key>::value
        || (Tokenized
            && (std::is_integral<K>::value || std::is_enum<K>::value))>::type>
{
  static bool
  equal (K const &key, char const *text, std::size_t size) LUMEX_NOEXCEPT
  {
    key_text const own (key);
    return own.size == size
           && (size == 0 || std::memcmp (own.data, text, size) == 0);
  }
};

/**
 * Whether `read` (a key the reader of layer P found in a message) is the key
 * of Row, as the index compares them: a text by its characters (the key
 * text of a tokenized layer), an integer or composite key by its bits cut
 * to the width of the wire.
 */
template <typename P, typename Row>
bool
key_is (::lumex::core::protocol::dsl::key_value_t const &read) LUMEX_NOEXCEPT
{
  namespace dsl = ::lumex::core::protocol::dsl;
  if (read.kind == dsl::key_kind::text)
    return text_key_match<
        typename Row::key_type,
        is_tokenized_layer<P>::value>::equal (Row::key (), read.text,
                                              read.text_size);
  if (read.kind != dsl::key_kind::integer
      && read.kind != dsl::key_kind::composite)
    return false;
  dsl::key_value_t const own = wire_key<P, Row> ();
  return (own.kind == dsl::key_kind::integer
          || own.kind == dsl::key_kind::composite)
         && dsl::normalize_key (
                read, ::lumex::core::protocol::index::key_width_of<P>::value)
                    .integer
                == own.integer;
}

// The fields of a message after its key: where the reader of P found them.
// Returns false when the message has no readable key or another key.
template <typename P, typename Row>
bool
fields_of (bytes_view_t message, bytes_view_t &fields,
           bool &other_key) LUMEX_NOEXCEPT
{
  ::lumex::core::protocol::index::key_read_t read
      = ::lumex::core::protocol::index::key_read_t ();
  other_key = false;
  if (!::lumex::core::protocol::index::key_reader_of<P>::type::read (message,
                                                                     read))
    return false;
  if (!key_is<P, Row> (read.key))
    {
      other_key = true;
      return false;
    }
  fields = message.subspan (read.payload_offset < message.size ()
                                ? read.payload_offset
                                : message.size ());
  return true;
}

// A row without a reply: its reply message is the key and no field.
template <typename Row> struct no_reply_row
{
  using fields_type = ::lumex::core::protocol::dsl::field_list<>;
  using key_type = typename Row::key_type;

  static LUMEX_CONSTEXPR key_type
  key () LUMEX_NOEXCEPT
  {
    return Row::key ();
  }
};

template <typename Row,
          bool None
          = std::is_same<typename Row::reply_type,
                         ::lumex::core::protocol::dsl::no_reply>::value>
struct reply_message_select
{
  using type = typename reply_value_of<Row>::type;

  template <typename P>
  static encode_result_t
  encode (bytes_t &out, type const &value, decode_context_t const *context)
  {
    return encode_reply<P, Row> (out, value, context);
  }

  template <typename P>
  static decode_result_t<type>
  decode (bytes_view_t fields, decode_context_t const *context)
  {
    return decode_reply<P, Row> (fields, context);
  }
};

template <typename Row> struct reply_message_select<Row, true>
{
  using type = ::lumex::core::protocol::field::record<>;

  template <typename P>
  static encode_result_t
  encode (bytes_t &out, type const &value, decode_context_t const *context)
  {
    return encode_request_record<P, no_reply_row<Row>> (out, value, context);
  }

  template <typename P>
  static decode_result_t<type>
  decode (bytes_view_t fields, decode_context_t const *context)
  {
    return decode_request<P, no_reply_row<Row>> (fields, context);
  }
};
} // namespace detail

/**
 * @brief `reply_message_of<Row>::type`: the value of the reply message of
 * a row: the value of its reply, `field::record<>` for a row without a
 * reply (the message is the header and the key).
 */
template <typename Row> struct reply_message_of
{
  /// The value.
  using type = typename detail::reply_message_select<Row>::type;
};

/**
 * @brief Appends what precedes the fields of a message of `Row` in layer
 * `P`: the header and the key (see the file description).
 * @param context The session values the header may read, or `nullptr`.
 */
template <typename P, typename Row>
encode_result_t
encode_header (bytes_t &out, decode_context_t const *context = nullptr)
{
  std::size_t const start = out.size ();
  encode_result_t const written = detail::header_of<P>::write (
      out, detail::wire_key<P, Row> (), context);
  if (written)
    return encode_result_t ();
  out.resize (start);
  return detail::failed (written.error ());
}

/**
 * @brief Appends the request message of `Row` in layer `P`: the header and
 * the key, then the fields of `record`.
 */
template <typename P, typename Row>
encode_result_t
encode_message_record (bytes_t &out,
                       typename request_of<Row>::type const &record,
                       decode_context_t const *context = nullptr)
{
  std::size_t const start = out.size ();
  encode_result_t const header = encode_header<P, Row> (out, context);
  if (!header)
    return detail::failed (header.error ());
  encode_result_t const fields
      = encode_request_record<P, Row> (out, record, context);
  if (fields)
    return encode_result_t ();
  out.resize (start);
  return detail::failed (fields.error ());
}

/**
 * @brief Appends the request message of `Row` in layer `P` made of `args`
 * (one per field, see `make_request`); the header gets no session values
 * (`encode_message_record` takes them).
 */
template <typename P, typename Row, typename... Args>
encode_result_t
encode_message (bytes_t &out, Args const &...args)
{
  decode_result_t<typename request_of<Row>::type> const record
      = make_request<Row> (args...);
  if (!record)
    return detail::failed (record.error ());
  return encode_message_record<P, Row> (out, *record);
}

/**
 * @brief Decodes the request message of `Row` in layer `P`;
 * `unknown_frame` when it has no readable key or another key.
 */
template <typename P, typename Row>
decode_result_t<typename request_of<Row>::type>
decode_message (bytes_view_t message,
                decode_context_t const *context = nullptr)
{
  bytes_view_t fields;
  bool other_key = false;
  if (!detail::fields_of<P, Row> (message, fields, other_key))
    return detail::message_failed (
        ::lumex::core::protocol::error::protocol_errc::unknown_frame);
  return decode_request<P, Row> (fields, context);
}

/**
 * @brief Appends the reply message of `Row` in layer `P`: the header and the
 * key, then the reply (nothing for a row without a reply: give
 * `field::record<> ()`).
 */
template <typename P, typename Row>
encode_result_t
encode_reply_message (bytes_t &out,
                      typename reply_message_of<Row>::type const &value,
                      decode_context_t const *context = nullptr)
{
  std::size_t const start = out.size ();
  encode_result_t const header = encode_header<P, Row> (out, context);
  if (!header)
    return detail::failed (header.error ());
  encode_result_t const fields
      = detail::reply_message_select<Row>::template encode<P> (out, value,
                                                               context);
  if (fields)
    return encode_result_t ();
  out.resize (start);
  return detail::failed (fields.error ());
}

/**
 * @brief Decodes the reply message of `Row` in layer `P`;
 * `unexpected_reply` when the message has another key, `unknown_frame`
 * when it has no readable key.
 */
template <typename P, typename Row>
decode_result_t<typename reply_message_of<Row>::type>
decode_reply_message (bytes_view_t message,
                      decode_context_t const *context = nullptr)
{
  bytes_view_t fields;
  bool other_key = false;
  if (!detail::fields_of<P, Row> (message, fields, other_key))
    return detail::message_failed (
        other_key
            ? ::lumex::core::protocol::error::protocol_errc::unexpected_reply
            : ::lumex::core::protocol::error::protocol_errc::unknown_frame);
  return detail::reply_message_select<Row>::template decode<P> (fields,
                                                                context);
}
} // namespace layout
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_LAYOUT_MESSAGE_HPP
