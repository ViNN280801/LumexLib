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
 * @file LumexProtocolCodec.hpp
 * @brief Encoding and decoding: `layout::encode` / `layout::decode` of any
 * field in a given layout, and per protocol row `encode_request`,
 * `decode_request`, `encode_reply`, `decode_reply`, `decode_nested`; the
 * layout of a protocol `layout_of<P>`; the alignment check `is_aligned<P>`.
 * @details The layout of a protocol is chosen by its header type (the
 * message format of the layer): `P::header_type::layout_type` when the
 * header names one, `binary_layout<>` otherwise. A layer made with
 * `LUMEX_PROTOCOL_EXTEND_FRAMED` names another header, so the same rows
 * (the same command types) are encoded in another layout without being
 * written again. The fields of a row are logical; only the layout decides
 * bytes or digits.
 *
 * Every function returns `lumex::expected` with a `std::error_code` of
 * `protocol_errc`. A decoder reads only its input (a shorter message is
 * `truncated`, a longer one `trailing_bytes`), allocates nothing and keeps
 * the raw values (`fixed_point::raw`) for the log line; an encoder appends
 * to the buffer and leaves it unchanged on failure.
 */
#ifndef LUMEX_CORE_PROTOCOL_LAYOUT_CODEC_HPP
#define LUMEX_CORE_PROTOCOL_LAYOUT_CODEC_HPP

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>

#include "lumex/core/expected/Expected"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/protocol/layout/LumexProtocolBinaryLayout.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace layout
{
/// @brief The result of an encode: nothing, or why it failed.
using encode_result_t
    = ::lumex::core::expected::result::expected<void, std::error_code>;

/// @brief The result of a decode: the value, or why it failed.
template <typename T>
using decode_result_t
    = ::lumex::core::expected::result::expected<T, std::error_code>;

namespace detail
{
template <typename H, typename = void> struct header_layout
{
  using type = binary_layout<>;
};

template <typename H>
struct header_layout<H, typename ::lumex::core::protocol::field::detail::
                            voider<typename H::layout_type>::type>
{
  using type = typename H::layout_type;
};

inline ::lumex::core::expected::error::unexpected<std::error_code>
failed (std::error_code const &code)
{
  return ::lumex::core::expected::error::unexpected<std::error_code> (code);
}

// The siblings of a field decoded on its own (not in a group).
using no_siblings = ::lumex::core::protocol::field::record<>;

template <typename Layout, typename Impl>
void
check_aligned () LUMEX_NOEXCEPT
{
  static_assert (Layout::template is_aligned<Impl>::value,
                 "LUMEX_PROTOCOL: binary_layout<natural_alignment<...> >: a "
                 "2-byte field is not at an even offset, or a 4- or 8-byte "
                 "field not at a multiple of 4 (offsets are checked up to "
                 "the first field of variable size)");
}

template <typename Record, typename Sequence> struct record_assign;

template <typename Record, std::size_t... I>
struct record_assign<Record,
                     ::lumex::core::utility::sequence::index_sequence<I...>>
{
  template <typename... Args>
  static bool
  apply (Record &record, Args const &...args)
  {
    bool ok = true;
    bool const results[]
        = { true,
            (ok = ok
                  && ::lumex::core::protocol::field::impl_of<
                      typename Record::template field_at<I>::type>::type::
                      assign (::lumex::core::protocol::field::get<I> (record),
                              args))... };
    (void)results;
    return ok;
  }
};
} // namespace detail

/// @brief `layout_of<P>::type`: the layout of protocol `P` (see the file
/// description).
template <typename P>
struct layout_of : detail::header_layout<typename P::header_type>
{
};

/// @brief `request_of<Row>::type`: the record of the request fields of a
/// row.
template <typename Row> struct request_of
{
  /// The record.
  using type = typename ::lumex::core::protocol::field::record_of<
      typename Row::fields_type>::type;
};

/// @brief `reply_value_of<Row>::type`: the value of the reply of a row (a
/// record for a layout).
template <typename Row> struct reply_value_of
{
  static_assert (!std::is_same<typename Row::reply_type,
                               ::lumex::core::protocol::dsl::no_reply>::value,
                 "LUMEX_PROTOCOL: the row has no reply");
  static_assert (
      ::lumex::core::protocol::field::is_field<
          typename Row::reply_type>::value,
      "LUMEX_PROTOCOL: the reply of the row is a reply kind of the dialogue "
      "(applied/transport/protocol), not a field: the dialogue decodes it");
  /// The value.
  using type = typename ::lumex::core::protocol::field::value_of<
      typename Row::reply_type>::type;
};

/**
 * @brief Decodes `message` as field `Field` (a field, a layout, a field
 * list) in layout `Layout`; the whole message must be the field.
 * @param context The session values (`union_by_context_`), or `nullptr`.
 */
template <typename Layout, typename Field>
decode_result_t<typename ::lumex::core::protocol::field::value_of<Field>::type>
decode (bytes_view_t message, decode_context_t const *context = nullptr)
{
  using impl = typename ::lumex::core::protocol::field::impl_of<Field>::type;
  detail::check_aligned<Layout, impl> ();
  typename Layout::reader reader (message, context);
  typename impl::value_type value = typename impl::value_type ();
  if (!impl::template decode<true> (reader, value, detail::no_siblings ())
      || !reader.finish ())
    return detail::failed (reader.error ());
  return value;
}

/**
 * @brief Appends `value` as field `Field` in layout `Layout` to `out`;
 * `out` is unchanged on failure.
 */
template <typename Layout, typename Field>
encode_result_t
encode (bytes_t &out,
        typename ::lumex::core::protocol::field::value_of<Field>::type const
            &value,
        decode_context_t const *context = nullptr)
{
  using impl = typename ::lumex::core::protocol::field::impl_of<Field>::type;
  detail::check_aligned<Layout, impl> ();
  std::size_t const start = out.size ();
  typename Layout::writer writer (out, context);
  if (!impl::template encode<true> (writer, value, detail::no_siblings ()))
    {
      out.resize (start);
      return detail::failed (writer.error ());
    }
  return encode_result_t ();
}

/**
 * @brief The request record of `Row` built from one argument per field
 * (each converted by its field: an engineering value for a scaled field, a
 * string for a text); `invalid_value` when one does not fit.
 */
template <typename Row, typename... Args>
decode_result_t<typename request_of<Row>::type>
make_request (Args const &...args)
{
  using record_type = typename request_of<Row>::type;
  static_assert (sizeof...(Args) == record_type::size::value,
                 "LUMEX_PROTOCOL: give one argument per field of the row");
  record_type record = record_type ();
  if (!detail::record_assign<
          record_type, ::lumex::core::utility::sequence::make_index_sequence<
                           sizeof...(Args)>>::apply (record, args...))
    return detail::failed (::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::invalid_value));
  return record;
}

/**
 * @brief Appends the request of `Row` (its fields, after the key in a
 * tokenized layout) in the layout of protocol `P` to `out`.
 */
template <typename P, typename Row>
encode_result_t
encode_request_record (bytes_t &out,
                       typename request_of<Row>::type const &record,
                       decode_context_t const *context = nullptr)
{
  using layout_type = typename layout_of<P>::type;
  using impl = typename ::lumex::core::protocol::field::impl_of<
      typename Row::fields_type>::type;
  detail::check_aligned<layout_type, impl> ();
  std::size_t const start = out.size ();
  typename layout_type::writer writer (out, context);
  if (!layout_type::template write_prefix<Row> (writer)
      || !impl::template encode<true> (writer, record, detail::no_siblings ()))
    {
      out.resize (start);
      return detail::failed (writer.error ());
    }
  return encode_result_t ();
}

/**
 * @brief Appends the request of `Row` made of `args` (one per field, see
 * `make_request`) in the layout of protocol `P` to `out`.
 */
template <typename P, typename Row, typename... Args>
encode_result_t
encode_request (bytes_t &out, Args const &...args)
{
  decode_result_t<typename request_of<Row>::type> const record
      = make_request<Row> (args...);
  if (!record)
    return detail::failed (record.error ());
  return encode_request_record<P, Row> (out, *record);
}

/// @brief Decodes the request of `Row` from `message` in the layout of
/// protocol `P` (a tokenized message starts with the key of `Row`).
template <typename P, typename Row>
decode_result_t<typename request_of<Row>::type>
decode_request (bytes_view_t message,
                decode_context_t const *context = nullptr)
{
  using layout_type = typename layout_of<P>::type;
  using impl = typename ::lumex::core::protocol::field::impl_of<
      typename Row::fields_type>::type;
  detail::check_aligned<layout_type, impl> ();
  typename layout_type::reader reader (message, context);
  typename request_of<Row>::type record = typename request_of<Row>::type ();
  if (!layout_type::template read_prefix<Row> (reader)
      || !impl::template decode<true> (reader, record, detail::no_siblings ())
      || !reader.finish ())
    return detail::failed (reader.error ());
  return record;
}

/// @brief Appends the reply of `Row` in the layout of protocol `P` to `out`.
template <typename P, typename Row>
encode_result_t
encode_reply (bytes_t &out, typename reply_value_of<Row>::type const &value,
              decode_context_t const *context = nullptr)
{
  using layout_type = typename layout_of<P>::type;
  using impl = typename ::lumex::core::protocol::field::impl_of<
      typename Row::reply_type>::type;
  detail::check_aligned<layout_type, impl> ();
  std::size_t const start = out.size ();
  typename layout_type::writer writer (out, context);
  if (!layout_type::template write_prefix<Row> (writer)
      || !impl::template encode<true> (writer, value, detail::no_siblings ()))
    {
      out.resize (start);
      return detail::failed (writer.error ());
    }
  return encode_result_t ();
}

/// @brief Decodes the reply of `Row` from `message` in the layout of
/// protocol `P`.
template <typename P, typename Row>
decode_result_t<typename reply_value_of<Row>::type>
decode_reply (bytes_view_t message, decode_context_t const *context = nullptr)
{
  using layout_type = typename layout_of<P>::type;
  using impl = typename ::lumex::core::protocol::field::impl_of<
      typename Row::reply_type>::type;
  detail::check_aligned<layout_type, impl> ();
  typename layout_type::reader reader (message, context);
  typename impl::value_type value = typename impl::value_type ();
  if (!layout_type::template read_prefix<Row> (reader)
      || !impl::template decode<true> (reader, value, detail::no_siblings ())
      || !reader.finish ())
    return detail::failed (reader.error ());
  return value;
}

/**
 * @brief Decodes the payload of a `nested_<P, KeyField>` value as the request
 * of row `Row` of the inner protocol `P`; `unknown_frame` when the key of
 * the value is not the key of `Row`.
 */
template <typename P, typename Row, typename Key>
decode_result_t<typename request_of<Row>::type>
decode_nested (::lumex::core::protocol::field::nested_value<Key> const &value,
               decode_context_t const *context = nullptr)
{
  if (::lumex::core::protocol::field::detail::to_discriminant (value.key)
      != static_cast<std::int64_t> (Row::key ()))
    return detail::failed (::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::unknown_frame));
  using layout_type = typename layout_of<P>::type;
  using impl = typename ::lumex::core::protocol::field::impl_of<
      typename Row::fields_type>::type;
  typename layout_type::reader reader (value.payload, context);
  typename request_of<Row>::type record = typename request_of<Row>::type ();
  if (!impl::template decode<true> (reader, record, detail::no_siblings ())
      || !reader.finish ())
    return detail::failed (reader.error ());
  return record;
}

namespace detail
{
template <typename Layout, typename Row,
          bool HasReply = ::lumex::core::protocol::field::is_field<
              typename Row::reply_type>::value>
struct row_aligned
    : std::integral_constant<
          bool, Layout::template is_aligned<
                    typename ::lumex::core::protocol::field::impl_of<
                        typename Row::fields_type>::type>::value
                    && Layout::template is_aligned<
                        typename ::lumex::core::protocol::field::impl_of<
                            typename Row::reply_type>::type>::value>
{
};

template <typename Layout, typename Row>
struct row_aligned<Layout, Row, false>
    : Layout::template is_aligned<typename ::lumex::core::protocol::field::
                                      impl_of<typename Row::fields_type>::type>
{
};

template <typename Layout, typename List> struct rows_aligned;

template <typename Layout, typename... Rows>
struct rows_aligned<Layout, ::lumex::core::protocol::dsl::type_list<Rows...>>
    : ::lumex::core::protocol::field::detail::values_all<
          row_aligned<Layout, Rows>::value...>
{
};
} // namespace detail

/**
 * @brief Whether every request and reply of protocol `P` satisfies the
 * alignment policy of its layout: `static_assert
 * (layout::is_aligned<P>::value,
 * "...")` checks a whole table at once.
 */
template <typename P>
struct is_aligned
    : detail::rows_aligned<
          typename layout_of<P>::type,
          typename ::lumex::core::protocol::dsl::commands_of<P>::type>
{
};
} // namespace layout
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_LAYOUT_CODEC_HPP
