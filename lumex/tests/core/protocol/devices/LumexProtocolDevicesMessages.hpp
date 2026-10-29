// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// The checks of whole messages over every row of a device table layer
// (lumex/tests/core/protocol/devices): the sample request and reply of each
// row encode as a message (the header and the key, then the fields), the
// run-time index reads the key the header wrote and finds the same row with
// its fields right after the header, and dispatch hands the message to the
// same row with a value that encodes to the same bytes.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_MESSAGES_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_MESSAGES_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"

namespace lumex_tests
{
namespace devices
{
namespace field = lumex::core::protocol::field;
namespace dsl = lumex::core::protocol::dsl;

/// What a dispatch visitor saw: the row, the call and the message encoded
/// again from the value it got.
template <typename P> struct dispatch_record
{
  std::string row;
  std::string call;
  bytes_t again;
  std::size_t calls = 0;
  field::decode_context_t const *context = nullptr;

  template <typename Row>
  void
  request (typename layout::request_of<Row>::type const &record)
  {
    ++calls;
    row = Row::identifier ();
    call = "request";
    EXPECT_TRUE (
        (layout::encode_message_record<P, Row> (again, record, context)
             .has_value ()));
  }

  template <typename Row, typename Value>
  void
  reply (Value const &value)
  {
    ++calls;
    row = Row::identifier ();
    call = "reply";
    EXPECT_TRUE ((layout::encode_reply_message<P, Row> (again, value, context)
                      .has_value ()));
  }
};

/// The message is found by the index at its row with the fields at
/// `offset`, and dispatch in `direction` calls the visitor of that row once
/// with a value that encodes to the same message.
template <typename P, typename Row>
void
expect_message_of_row (bytes_t const &message, std::size_t offset,
                       index::frame_direction direction,
                       field::decode_context_t const *context)
{
  char const *const side
      = direction == index::frame_direction::tx ? "request" : "reply";
  EXPECT_TRUE (finds<Row> (index_of<P> (), message, offset))
      << P::protocol_name () << " " << side << " " << dump (message);
  dispatch_record<P> seen;
  seen.context = context;
  std::error_code const error
      = layout::dispatch<P> (view_of (message), direction, seen, context);
  EXPECT_FALSE (error) << P::protocol_name () << "." << Row::identifier ()
                       << " " << side << ": " << error.message () << ": "
                       << dump (message);
  EXPECT_EQ (seen.calls, error ? 0u : 1u);
  EXPECT_EQ (seen.row, error ? std::string () : Row::identifier ())
      << P::protocol_name () << " " << side;
  EXPECT_EQ (seen.again, error ? bytes_t () : message)
      << P::protocol_name () << "." << Row::identifier () << " " << side;
}

namespace detail
{
// The reply of a row: 0 none (an empty record), 1 a field or a layout, 2 a
// reply kind of the dialogue (not a message of the core).
template <typename Row>
struct reply_kind
    : std::integral_constant<
          int,
          std::is_same<typename Row::reply_type, dsl::no_reply>::value
              ? 0
              : (field::is_field<typename Row::reply_type>::value ? 1 : 2)>
{
};

template <typename P, typename Row>
void
expect_reply (field::decode_context_t const *context,
              std::integral_constant<int, 0>)
{
  bytes_t header;
  ASSERT_TRUE ((layout::encode_header<P, Row> (header, context).has_value ()));
  bytes_t message;
  ASSERT_TRUE ((layout::encode_reply_message<P, Row> (
                    message, field::record<> (), context)
                    .has_value ()))
      << P::protocol_name () << "." << Row::identifier ();
  expect_message_of_row<P, Row> (message, header.size (),
                                 index::frame_direction::rx, context);
}

template <typename P, typename Row>
void
expect_reply (field::decode_context_t const *context,
              std::integral_constant<int, 1>)
{
  bytes_t header;
  ASSERT_TRUE ((layout::encode_header<P, Row> (header, context).has_value ()));
  bytes_t message;
  layout::encode_result_t const encoded
      = layout::encode_reply_message<P, Row> (
          message, field::sample_value<typename Row::reply_type> (), context);
  ASSERT_TRUE (encoded.has_value ())
      << P::protocol_name () << "." << Row::identifier ()
      << ": the sample reply does not encode: " << encoded.error ().message ();
  expect_message_of_row<P, Row> (message, header.size (),
                                 index::frame_direction::rx, context);
}

template <typename P, typename Row>
void
expect_reply (field::decode_context_t const *, std::integral_constant<int, 2>)
{
}

template <typename P, typename Row>
void
expect_row (field::decode_context_t const *context)
{
  bytes_t header;
  ASSERT_TRUE ((layout::encode_header<P, Row> (header, context).has_value ()))
      << P::protocol_name () << "." << Row::identifier ();
  bytes_t message;
  layout::encode_result_t const encoded
      = layout::encode_message_record<P, Row> (
          message, field::sample_value<typename Row::fields_type> (), context);
  ASSERT_TRUE (encoded.has_value ())
      << P::protocol_name () << "." << Row::identifier ()
      << ": the sample request does not encode: "
      << encoded.error ().message ();
  ASSERT_GE (message.size (), header.size ());
  EXPECT_EQ (bytes_t (message.begin (),
                      message.begin ()
                          + static_cast<std::ptrdiff_t> (header.size ())),
             header);
  expect_message_of_row<P, Row> (message, header.size (),
                                 index::frame_direction::tx, context);
  expect_reply<P, Row> (context, reply_kind<Row> ());
}

template <typename P, typename List> struct each_row;

template <typename P, typename... Rows>
struct each_row<P, dsl::type_list<Rows...>>
{
  static std::size_t
  check (field::decode_context_t const *context)
  {
    int const results[] = { 0, (expect_row<P, Rows> (context), 0)... };
    (void)results;
    return sizeof...(Rows);
  }
};
} // namespace detail

/**
 * Every row of layer `P`: its sample request and reply as messages, found
 * by the index and dispatched to the same row; returns the number of rows.
 * `context` gives the session values the decoders of the layer need.
 */
template <typename P>
std::size_t
expect_every_row_message (field::decode_context_t const *context = nullptr)
{
  return detail::each_row<P, typename dsl::commands_of<P>::type>::check (
      context);
}

/// A session that knows every value as 0 (the first case of the session
/// unions of the tables).
struct zero_session
{
  static bool
  lookup (void const *, char const *, std::int64_t &value)
  {
    value = 0;
    return true;
  }

  static field::decode_context_t
  context ()
  {
    field::decode_context_t const result = { nullptr, &zero_session::lookup };
    return result;
  }
};
} // namespace devices
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_MESSAGES_HPP
