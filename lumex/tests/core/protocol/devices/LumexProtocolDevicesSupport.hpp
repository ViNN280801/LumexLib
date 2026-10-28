// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Helpers of the device table tests (lumex/tests/core/protocol/devices):
// byte vectors, the round trip of a request or a reply (decode, encode
// again, the same bytes), the row the run-time index finds for a frame, and
// the start of the fields in a frame.
//
// The comments of the form /*@r..*/ right after a literal in the device
// files mark a value that was altered; the number is opaque and carries no
// value.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_SUPPORT_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace lumex_tests
{
namespace devices
{
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;

using bytes_t = layout::bytes_t;

/// The bytes of a list of small integers.
inline bytes_t
bytes (std::initializer_list<int> values)
{
  bytes_t out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

/// The characters of a text as bytes (no terminating NUL).
inline bytes_t
text_bytes (std::string const &text)
{
  return bytes_t (text.begin (), text.end ());
}

/// The bytes of a hexadecimal text ("A0E0..."), two digits per byte.
inline bytes_t
hex_bytes (std::string const &text)
{
  bytes_t out;
  for (std::size_t i = 0; i + 1 < text.size (); i += 2)
    out.push_back (static_cast<std::uint8_t> (
        std::stoul (text.substr (i, 2), nullptr, 16)));
  return out;
}

/// A view of a buffer.
inline layout::bytes_view_t
view_of (bytes_t const &data)
{
  return layout::bytes_view_t (data.data (), data.size ());
}

/// `head` followed by `tail`.
inline bytes_t
join (bytes_t head, bytes_t const &tail)
{
  head.insert (head.end (), tail.begin (), tail.end ());
  return head;
}

/// The bytes of `frame` from `from` on.
inline bytes_t
tail_of (bytes_t const &frame, std::size_t from)
{
  return from < frame.size ()
             ? bytes_t (frame.begin () + static_cast<std::ptrdiff_t> (from),
                        frame.end ())
             : bytes_t ();
}

/// The bytes as "01 0A FF" for the failure messages.
inline std::string
dump (bytes_t const &data)
{
  std::string out;
  for (std::size_t i = 0; i < data.size (); ++i)
    {
      char cell[4];
      std::snprintf (cell, sizeof (cell), "%02X", data[i]);
      if (i != 0)
        out += ' ';
      out += cell;
    }
  return out;
}

/// The request of `Row` in `message` decodes, and its record encodes to the
/// same bytes.
template <typename P, typename Row>
::testing::AssertionResult
request_round_trip (bytes_t const &message)
{
  layout::decode_result_t<typename layout::request_of<Row>::type> const decoded
      = layout::decode_request<P, Row> (view_of (message));
  if (!decoded)
    return ::testing::AssertionFailure ()
           << Row::identifier () << ": the request does not decode ("
           << decoded.error ().message () << "): " << dump (message);
  bytes_t again;
  layout::encode_result_t const encoded
      = layout::encode_request_record<P, Row> (again, *decoded);
  if (!encoded || again != message)
    return ::testing::AssertionFailure ()
           << Row::identifier () << ": encoded again " << dump (again)
           << ", expected " << dump (message);
  return ::testing::AssertionSuccess ();
}

/// The reply of `Row` in `message` decodes, and its value encodes to the
/// same bytes.
template <typename P, typename Row>
::testing::AssertionResult
reply_round_trip (bytes_t const &message)
{
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<P, Row> (view_of (message));
  if (!decoded)
    return ::testing::AssertionFailure ()
           << Row::identifier () << ": the reply does not decode ("
           << decoded.error ().message () << "): " << dump (message);
  bytes_t again;
  layout::encode_result_t const encoded
      = layout::encode_reply<P, Row> (again, *decoded);
  if (!encoded || again != message)
    return ::testing::AssertionFailure ()
           << Row::identifier () << ": encoded again " << dump (again)
           << ", expected " << dump (message);
  return ::testing::AssertionSuccess ();
}

/// The index of `P` finds row `Row` for `frame` and the fields start at
/// `offset`.
template <typename Row>
::testing::AssertionResult
finds (index::protocol_index_t const &protocol, bytes_t const &frame,
       std::size_t offset)
{
  index::frame_match_t const found = protocol.match (view_of (frame));
  if (found.row == nullptr)
    return ::testing::AssertionFailure ()
           << "no row for " << dump (frame) << ", expected "
           << Row::identifier ();
  std::string const name = found.row->entry->identifier;
  if (name != Row::identifier () || found.payload_offset != offset)
    return ::testing::AssertionFailure ()
           << "row " << name << " at " << found.payload_offset << ", expected "
           << Row::identifier () << " at " << offset;
  return ::testing::AssertionSuccess ();
}

/// The index of `P` (one instance per program); fails the test when the
/// index refuses the table.
template <typename P>
index::protocol_index_t const &
index_of ()
{
  index::index_result_t const &result = index::protocol_index<P> ();
  if (!result)
    ADD_FAILURE () << "the index refuses the table: "
                   << result.error ().message;
  return *result;
}

/// A number as decimal text (the parts of the expected log lines).
template <typename T>
std::string
dec (T value)
{
  return std::to_string (value);
}
} // namespace devices
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_SUPPORT_HPP
