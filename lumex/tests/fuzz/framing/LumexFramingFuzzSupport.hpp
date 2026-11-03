// The common body of the libFuzzer targets of lumex/core/framing: every
// target names a codec (or a pipeline) and the variants of its options, this
// header turns that into the checks. An input is [variant][chunk plan]
// [stream]. The stream is decoded
//   - in one chunk, and in chunks cut by the plan, and after a reset(): the
//     three runs must report the same frames and the same errors in the same
//     order (a decoder keeps a partial frame between calls);
//   - through a sink that takes only frames: it must hear exactly the frames
//     of the sink that also takes errors;
// every delivered frame is within the limit of the options, every error is a
// framing_errc, and an error comes with an empty frame. A delivered frame is
// encoded again and decoded by a fresh decoder: it must come back as the one
// frame it was. For the codecs whose frame is canonical the encoded bytes
// must also stand in the stream as they are. The stream, taken as a payload,
// either fails with payload_too_large and leaves the output unchanged, or
// comes back from a fresh decoder as the one frame it was. Anything else
// stops the run. Built with LUMEX_BUILD_FUZZERS (Clang only), under the label
// fuzz; LLVMFuzzerInitialize writes the seed corpus (frames made by the codec
// from synthetic payloads) into the corpus directory named on the command
// line. No vector file is committed.

#ifndef LUMEX_TESTS_FUZZ_FRAMING_SUPPORT_HPP
#define LUMEX_TESTS_FUZZ_FRAMING_SUPPORT_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/framing/codec/LumexFramingErrc.hpp"

#define LUMEX_FUZZ_REQUIRE(condition)                                         \
  do                                                                          \
    {                                                                         \
      if (!(condition))                                                       \
        lumex_framing_fuzz::fail (#condition, __LINE__);                      \
    }                                                                         \
  while (false)

namespace lumex_framing_fuzz
{
namespace codec = lumex::core::framing::codec;

using bytes_t = std::vector<std::uint8_t>;

inline void
fail (char const *expression, int line)
{
  std::fprintf (stderr, "check failed at line %d: %s\n", line, expression);
  std::abort ();
}

struct event_t
{
  bytes_t frame;
  std::error_code error;
};

using events_t = std::vector<event_t>;

inline bool
operator== (event_t const &lhs, event_t const &rhs)
{
  return lhs.frame == rhs.frame && lhs.error == rhs.error;
}

// A sink that takes frames and errors.
struct recorder_t
{
  events_t *events;
  void
  operator() (codec::bytes_view_t frame, std::error_code error) const
  {
    event_t event;
    event.frame.assign (frame.begin (), frame.end ());
    event.error = error;
    events->push_back (event);
  }
};

// A sink that takes frames only.
struct frames_only_t
{
  std::vector<bytes_t> *frames;
  void
  operator() (codec::bytes_view_t frame) const
  {
    frames->push_back (bytes_t (frame.begin (), frame.end ()));
  }
};

template <typename Codec>
events_t
decode_all (Codec &decoder, bytes_t const &stream, std::size_t plan)
{
  events_t events;
  recorder_t sink = { &events };
  if (plan == 0)
    {
      LUMEX_FUZZ_REQUIRE (
          decoder.decode (codec::bytes_view_t (stream.data (), stream.size ()),
                          sink)
          == stream.size ());
      return events;
    }
  std::size_t at = 0;
  std::size_t step = 0;
  while (at < stream.size ())
    {
      std::size_t const size
          = std::min (stream.size () - at, (plan * (step + 1)) % 11 + 1);
      LUMEX_FUZZ_REQUIRE (
          decoder.decode (codec::bytes_view_t (stream.data () + at, size),
                          sink)
          == size);
      at += size;
      ++step;
    }
  return events;
}

inline bool
contains (bytes_t const &haystack, bytes_t const &needle)
{
  return std::search (haystack.begin (), haystack.end (), needle.begin (),
                      needle.end ())
         != haystack.end ();
}

// Traits of a target:
//   using codec_t = ...;
//   static unsigned variants ();
//   static codec_t make (unsigned variant);
//   static std::size_t frame_limit (unsigned variant);  // largest frame
//   static bool payload_ok (unsigned variant, bytes_t const &);
//     // encode then decode returns the payload
//   static bool grows ();  // encode may refuse a frame that was decoded
//   static void check_frame (unsigned variant, bytes_t const &frame);
//   static void extra (unsigned variant, bytes_t const &stream);
//     (both from default_traits_t)
//   static bool canonical ();  // the encoded frame stands in the stream
//   static std::vector<bytes_t> payloads ();  // seeds
// Three codecs behind one interface, for a target whose variants differ in a
// template argument (the check value of a header, the stages of a pipeline).
template <typename A, typename B, typename C> class select_t
{
public:
  select_t (unsigned which, A const &a, B const &b, C const &c)
      : which_ (which % 3U), a_ (a), b_ (b), c_ (c)
  {
  }

  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    switch (which_)
      {
      case 0:
        return a_.encode (payload, out);
      case 1:
        return b_.encode (payload, out);
      default:
        return c_.encode (payload, out);
      }
  }

  template <typename Sink>
  std::size_t
  decode (codec::bytes_view_t chunk, Sink &&sink)
  {
    switch (which_)
      {
      case 0:
        return a_.decode (chunk, sink);
      case 1:
        return b_.decode (chunk, sink);
      default:
        return c_.decode (chunk, sink);
      }
  }

  void
  reset () noexcept
  {
    a_.reset ();
    b_.reset ();
    c_.reset ();
  }

private:
  unsigned which_;
  A a_;
  B b_;
  C c_;
};

// What a target need not say: no check of a frame of its own, no extra check.
struct default_traits_t
{
  static void
  check_frame (unsigned, bytes_t const &)
  {
  }
  static void
  extra (unsigned, bytes_t const &)
  {
  }
};

template <typename Traits>
void
check_input (std::uint8_t const *data, std::size_t size)
{
  using codec_t = typename Traits::codec_t;
  if (size < 2)
    return;
  unsigned const variant = data[0] % Traits::variants ();
  std::size_t const plan = data[1];
  bytes_t const stream (data + 2, data + size);
  std::size_t const limit = Traits::frame_limit (variant);

  codec_t whole = Traits::make (variant);
  events_t const expected = decode_all (whole, stream, 0);
  {
    codec_t chunked = Traits::make (variant);
    LUMEX_FUZZ_REQUIRE (decode_all (chunked, stream, plan) == expected);
    whole.reset ();
    LUMEX_FUZZ_REQUIRE (decode_all (whole, stream, 0) == expected);
  }
  {
    codec_t frames_codec = Traits::make (variant);
    std::vector<bytes_t> frames;
    frames_only_t sink = { &frames };
    frames_codec.decode (codec::bytes_view_t (stream.data (), stream.size ()),
                         sink);
    std::vector<bytes_t> wanted;
    for (std::size_t i = 0; i < expected.size (); ++i)
      if (!expected[i].error)
        wanted.push_back (expected[i].frame);
    LUMEX_FUZZ_REQUIRE (frames == wanted);
  }

  std::error_code const sample
      = make_error_code (lumex::core::framing::codec::framing_errc::truncated);
  for (std::size_t i = 0; i < expected.size (); ++i)
    {
      event_t const &event = expected[i];
      if (event.error)
        {
          LUMEX_FUZZ_REQUIRE (event.error.category () == sample.category ());
          LUMEX_FUZZ_REQUIRE (event.frame.empty ());
          continue;
        }
      LUMEX_FUZZ_REQUIRE (event.frame.size () <= limit);
      Traits::check_frame (variant, event.frame);
      codec_t encoder = Traits::make (variant);
      bytes_t wire;
      codec::encode_result_t const encoded = encoder.encode (
          codec::bytes_view_t (event.frame.data (), event.frame.size ()),
          wire);
      if (!encoded.has_value () && Traits::grows ()
          && encoded.error ()
                 == make_error_code (lumex::core::framing::codec::
                                         framing_errc::payload_too_large))
        // A stage added its check value to a frame that came without it.
        continue;
      LUMEX_FUZZ_REQUIRE (encoded.has_value ());
      codec_t fresh = Traits::make (variant);
      events_t const again = decode_all (fresh, wire, 0);
      LUMEX_FUZZ_REQUIRE (again.size () == 1 && !again[0].error
                          && again[0].frame == event.frame);
      if (Traits::canonical ())
        LUMEX_FUZZ_REQUIRE (contains (stream, wire));
    }

  Traits::extra (variant, stream);

  // The stream as a payload.
  if (Traits::payload_ok (variant, stream))
    {
      codec_t encoder = Traits::make (variant);
      bytes_t wire (3, 0xEE);
      codec::encode_result_t const result = encoder.encode (
          codec::bytes_view_t (stream.data (), stream.size ()), wire);
      if (!result.has_value ())
        {
          LUMEX_FUZZ_REQUIRE (wire == bytes_t (3, 0xEE));
          return;
        }
      LUMEX_FUZZ_REQUIRE (wire.size () >= 3
                          && std::equal (wire.begin (), wire.begin () + 3,
                                         bytes_t (3, 0xEE).begin ()));
      wire.erase (wire.begin (), wire.begin () + 3);
      codec_t fresh = Traits::make (variant);
      events_t const again = decode_all (fresh, wire, 0);
      LUMEX_FUZZ_REQUIRE (again.size () == 1 && !again[0].error
                          && again[0].frame == stream);
    }
}

// The frame of `payload` as the codec of `variant` writes it; when that codec
// refuses it (the payload is above its limit), the codec of another variant
// that takes it, so that the corpus holds frames just above the small limits.
template <typename Traits>
bool
seed_frame (unsigned variant, bytes_t const &payload, bytes_t &wire)
{
  for (unsigned n = 0; n < Traits::variants (); ++n)
    {
      typename Traits::codec_t const encoder
          = Traits::make ((variant + n) % Traits::variants ());
      wire.clear ();
      if (encoder
              .encode (codec::bytes_view_t (payload.data (), payload.size ()),
                       wire)
              .has_value ())
        return true;
    }
  return false;
}

template <typename Traits>
void
write_seeds (std::string const &directory)
{
  std::vector<bytes_t> const payloads = Traits::payloads ();
  std::size_t number = 0;
  for (unsigned variant = 0; variant < Traits::variants (); ++variant)
    for (std::size_t plan = 0; plan < 2; ++plan)
      {
        // The frames of all payloads in one stream, and each frame alone.
        bytes_t all;
        all.push_back (static_cast<std::uint8_t> (variant));
        all.push_back (static_cast<std::uint8_t> (plan * 3));
        std::vector<bytes_t> files;
        for (std::size_t i = 0; i < payloads.size (); ++i)
          {
            bytes_t wire;
            if (!seed_frame<Traits> (variant, payloads[i], wire))
              continue;
            all.insert (all.end (), wire.begin (), wire.end ());
            bytes_t single (all.begin (), all.begin () + 2);
            single.insert (single.end (), wire.begin (), wire.end ());
            files.push_back (single);
            if (single.size () > 3)
              {
                // The same frame cut short.
                single.resize (single.size () - 1);
                files.push_back (single);
              }
          }
        files.push_back (all);
        for (std::size_t n = 0; n < files.size (); ++n)
          {
            std::ofstream file (directory + "/seed_"
                                    + std::to_string (number++) + ".bin",
                                std::ios::binary);
            if (!file)
              return;
            file.write (reinterpret_cast<char const *> (files[n].data ()),
                        static_cast<std::streamsize> (files[n].size ()));
          }
      }
}

// The payloads every byte-oriented codec is seeded with.
inline std::vector<bytes_t>
common_payloads ()
{
  std::vector<bytes_t> result;
  result.push_back (bytes_t ());
  result.push_back (bytes_t (1, 0x41));
  {
    std::uint8_t const special[] = { 0x00, 0x01, 0xC0, 0xDB, 0xDC, 0xDD,
                                     0x0D, 0x0A, 0x7E, 0xFF, 0x00, 0x00 };
    result.push_back (bytes_t (special, special + sizeof special));
  }
  // Just under, at and above the small limit of the targets (16).
  result.push_back (bytes_t (15, 0x11));
  result.push_back (bytes_t (16, 0x12));
  result.push_back (bytes_t (17, 0x13));
  {
    bytes_t longer;
    for (std::size_t i = 0; i < 300; ++i)
      longer.push_back (static_cast<std::uint8_t> (i % 255 + 1));
    result.push_back (longer);
  }
  return result;
}

inline void
seed_directory (int *argc, char ***argv, void (*write) (std::string const &))
{
  for (int i = 1; i < *argc; ++i)
    {
      std::string const argument ((*argv)[i]);
      if (argument.empty () || argument[0] == '-')
        continue;
      write (argument);
      return;
    }
}
} // namespace lumex_framing_fuzz

// Defines the two entry points of a target from its traits.
#define LUMEX_FRAMING_FUZZ_TARGET(TRAITS)                                     \
  extern "C" int LLVMFuzzerInitialize (int *argc, char ***argv)               \
  {                                                                           \
    lumex_framing_fuzz::seed_directory (                                      \
        argc, argv, &lumex_framing_fuzz::write_seeds<TRAITS>);                \
    return 0;                                                                 \
  }                                                                           \
  extern "C" int LLVMFuzzerTestOneInput (std::uint8_t const *data,            \
                                         std::size_t size)                    \
  {                                                                           \
    lumex_framing_fuzz::check_input<TRAITS> (data, size);                     \
    return 0;                                                                 \
  }

#endif // !LUMEX_TESTS_FUZZ_FRAMING_SUPPORT_HPP
