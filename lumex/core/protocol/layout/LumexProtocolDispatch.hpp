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
 * @file LumexProtocolDispatch.hpp
 * @brief `dispatch<P> (message, direction, visitor)`: a received message to
 * the typed row its key names.
 * @details The key reader of layer `P` reads the key of the message and the
 * rows of the table are compared with it as the run-time index compares
 * them (`LumexProtocolMessage.hpp`). For the row found, the visitor gets
 * the decoded value:
 * @code
 * struct visitor
 * {
 *   template <typename Row>
 *   void request (typename layout::request_of<Row>::type const &record);
 *   template <typename Row, typename Value>
 *   void reply (Value const &value);
 * };
 * @endcode
 * `request<Row>` is called for a message going to the device
 * (`frame_direction::tx`), `reply<Row>` for one coming from it (`rx`: a
 * reply or an event) with `reply_message_of<Row>::type` (an empty
 * `field::record<>` for a row without a reply) or, for a reply kind of the
 * dialogue layer, the message itself as `bytes_view_t`. The visitor is
 * called at most once, and only with a value that decoded: a message
 * without a readable key or with a key no row has is `unknown_frame`, a
 * decode error is returned as it is.
 */
#ifndef LUMEX_CORE_PROTOCOL_LAYOUT_DISPATCH_HPP
#define LUMEX_CORE_PROTOCOL_LAYOUT_DISPATCH_HPP

#include <cstddef>
#include <system_error>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolKey.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/index/LumexProtocolKeyReader.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/protocol/layout/LumexProtocolMessage.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
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
// Decodes the fields of a message of one row and calls the visitor.
template <typename Visitor>
using visit_thunk_t
    = std::error_code (*) (bytes_view_t message, bytes_view_t fields,
                           Visitor &visitor, decode_context_t const *context);

// Whether a key read from a message is the key of one row.
using key_test_t
    = bool (*) (::lumex::core::protocol::dsl::key_value_t const &key);

template <typename P, typename Row, typename Visitor>
std::error_code
visit_request (bytes_view_t, bytes_view_t fields, Visitor &visitor,
               decode_context_t const *context)
{
  decode_result_t<typename request_of<Row>::type> const record
      = decode_request<P, Row> (fields, context);
  if (!record)
    return record.error ();
  visitor.template request<Row> (*record);
  return std::error_code ();
}

template <typename P, typename Row, typename Visitor,
          bool Core
          = std::is_same<typename Row::reply_type,
                         ::lumex::core::protocol::dsl::no_reply>::value
            || ::lumex::core::protocol::field::is_field<
                typename Row::reply_type>::value>
struct visit_reply
{
  static std::error_code
  apply (bytes_view_t, bytes_view_t fields, Visitor &visitor,
         decode_context_t const *context)
  {
    decode_result_t<typename reply_message_of<Row>::type> const value
        = reply_message_select<Row>::template decode<P> (fields, context);
    if (!value)
      return value.error ();
    visitor.template reply<Row> (*value);
    return std::error_code ();
  }
};

// A reply kind of the dialogue layer: the visitor decodes the message.
template <typename P, typename Row, typename Visitor>
struct visit_reply<P, Row, Visitor, false>
{
  static std::error_code
  apply (bytes_view_t message, bytes_view_t, Visitor &visitor,
         decode_context_t const *)
  {
    visitor.template reply<Row> (message);
    return std::error_code ();
  }
};

template <typename Visitor> struct dispatch_entry
{
  key_test_t matches;
  visit_thunk_t<Visitor> request;
  visit_thunk_t<Visitor> reply;
};

template <typename P, typename Visitor, typename List> struct dispatch_table;

template <typename P, typename Visitor, typename... Rows>
struct dispatch_table<P, Visitor,
                      ::lumex::core::protocol::dsl::type_list<Rows...>>
{
  // The entry of the row whose key is `key`, or nullptr; the rows are
  // compared in table order (their keys are unique on the wire).
  static dispatch_entry<Visitor> const *
  find (::lumex::core::protocol::dsl::key_value_t const &key) LUMEX_NOEXCEPT
  {
    // One entry more than rows: an empty protocol still has an array.
    static dispatch_entry<Visitor> const entries[sizeof...(Rows) + 1]
        = { { &key_is<P, Rows>, &visit_request<P, Rows, Visitor>,
              &visit_reply<P, Rows, Visitor>::apply }...,
            { nullptr, nullptr, nullptr } };
    for (std::size_t i = 0; i < sizeof...(Rows); ++i)
      if (entries[i].matches (key))
        return &entries[i];
    return nullptr;
  }
};
} // namespace detail

/**
 * @brief Decodes `message` of layer `P` as the row its key names and calls
 * `visitor.template request<Row> (record)` (`tx`) or `visitor.template
 * reply<Row> (value)` (`rx`); see the file description.
 * @param context The session values the decoders read, or `nullptr`.
 * @return Nothing when the visitor was called; `unknown_frame` when no row
 * has the key of the message (or it has none), the decode error otherwise.
 */
template <typename P, typename Visitor>
std::error_code
dispatch (bytes_view_t message,
          ::lumex::core::protocol::index::frame_direction direction,
          Visitor &&visitor, decode_context_t const *context = nullptr)
{
  using visitor_type = typename std::remove_reference<Visitor>::type;
  ::lumex::core::protocol::index::key_read_t read
      = ::lumex::core::protocol::index::key_read_t ();
  if (!::lumex::core::protocol::index::key_reader_of<P>::type::read (message,
                                                                     read))
    return ::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::unknown_frame);
  detail::dispatch_entry<visitor_type> const *const found
      = detail::dispatch_table<P, visitor_type,
                               typename ::lumex::core::protocol::dsl::
                                   commands_of<P>::type>::find (read.key);
  if (found == nullptr)
    return ::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::unknown_frame);
  bytes_view_t const fields = message.subspan (
      read.payload_offset < message.size () ? read.payload_offset
                                            : message.size ());
  return (direction == ::lumex::core::protocol::index::frame_direction::tx
              ? found->request
              : found->reply) (message, fields, visitor, context);
}
} // namespace layout
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_LAYOUT_DISPATCH_HPP
