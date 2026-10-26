// Helpers of the framing tests: a sink that records frames and errors, byte
// vector literals, and feeding a stream to a decoder split at every position.
#ifndef LUMEX_TESTS_CORE_FRAMING_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_FRAMING_TEST_SUPPORT_HPP

#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/core/framing/codec/LumexFramingCodec.hpp"

namespace lumex_framing_test
{
namespace codec = lumex::core::framing::codec;
using codec::byte_t;
using codec::bytes_t;
using codec::bytes_view_t;

/// What a decoder delivered: frames and errors in the order they came.
struct received_t
{
  std::vector<bytes_t> frames;
  std::vector<std::error_code> errors;
  /// One entry per sink call: 'F' for a frame, 'E' for an error.
  std::string order;
};

/// The sink of the error form; records into a received_t.
struct recording_sink
{
  received_t *received;

  void
  operator() (bytes_view_t frame, std::error_code error) const
  {
    if (error)
      {
        received->errors.push_back (error);
        received->order += 'E';
      }
    else
      {
        received->frames.push_back (bytes_t (frame.begin (), frame.end ()));
        received->order += 'F';
      }
  }
};

/// The bytes of a string, without the terminating zero.
inline bytes_t
bytes_of (std::string const &text)
{
  return bytes_t (text.begin (), text.end ());
}

/// Feeds the whole stream to the decoder in one call.
template <typename Codec>
received_t
decode_all (Codec &decoder, bytes_t const &stream)
{
  received_t received;
  recording_sink sink{ &received };
  decoder.decode (bytes_view_t (stream.data (), stream.size ()), sink);
  return received;
}

/// Feeds the stream one byte per call.
template <typename Codec>
received_t
decode_bytewise (Codec &decoder, bytes_t const &stream)
{
  received_t received;
  recording_sink sink{ &received };
  for (std::size_t i = 0; i < stream.size (); ++i)
    decoder.decode (bytes_view_t (stream.data () + i, 1), sink);
  return received;
}

/// Feeds the stream in two calls split at `at`.
template <typename Codec>
received_t
decode_split (Codec &decoder, bytes_t const &stream, std::size_t at)
{
  received_t received;
  recording_sink sink{ &received };
  decoder.decode (bytes_view_t (stream.data (), at), sink);
  decoder.decode (bytes_view_t (stream.data () + at, stream.size () - at),
                  sink);
  return received;
}

/// Encodes a payload, which must succeed, and returns the frame.
template <typename Codec>
bytes_t
encode_ok (Codec const &encoder, bytes_t const &payload)
{
  bytes_t out;
  auto const result
      = encoder.encode (bytes_view_t (payload.data (), payload.size ()), out);
  if (!result.has_value ())
    out.clear ();
  return out;
}

/// The concatenation of two byte vectors.
inline bytes_t
concat (bytes_t first, bytes_t const &second)
{
  first.insert (first.end (), second.begin (), second.end ());
  return first;
}
} // namespace lumex_framing_test

#endif // !LUMEX_TESTS_CORE_FRAMING_TEST_SUPPORT_HPP
