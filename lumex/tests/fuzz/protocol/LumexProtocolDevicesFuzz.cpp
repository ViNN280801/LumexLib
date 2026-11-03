// libFuzzer target of the thirteen de-identified device tables
// (lumex/tests/core/protocol/devices, devices A..M, every layer of every
// table but the aligned family of device B, which is left out on purpose):
// the first byte of an input chooses a layer, the second the
// direction of the message, the rest is the message. dispatch_message must
// never read past the input (AddressSanitizer); when it accepts the message
// it calls the visitor of exactly one row, the run-time index finds that same
// row, the value the visitor got encodes to a message, and dispatching that
// message again reaches the same row and encodes to the same bytes (decode
// and encode are stable after one round). Anything else stops the run. The
// layers that read session values get a session that knows every value as 0.
// LLVMFuzzerInitialize writes the seed corpus into the corpus directory named
// on the command line: per row the sample request and the sample reply of
// the table (field::sample_value), no vector file is committed. Built with
// LUMEX_BUILD_FUZZERS (Clang only); CTest runs it for a short bounded time
// under the label fuzz.
//
// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"
#include "lumex/tests/core/protocol/devices/device_b/LumexProtocolDeviceB.hpp"
#include "lumex/tests/core/protocol/devices/device_c/LumexProtocolDeviceC.hpp"
#include "lumex/tests/core/protocol/devices/device_d/LumexProtocolDeviceD.hpp"
#include "lumex/tests/core/protocol/devices/device_e/LumexProtocolDeviceETable.hpp"
#include "lumex/tests/core/protocol/devices/device_f/LumexProtocolDeviceFTable.hpp"
#include "lumex/tests/core/protocol/devices/device_g/LumexProtocolDeviceGTable.hpp"
#include "lumex/tests/core/protocol/devices/device_h/LumexProtocolDeviceH.hpp"
#include "lumex/tests/core/protocol/devices/device_i/LumexProtocolDeviceITable.hpp"
#include "lumex/tests/core/protocol/devices/device_j/LumexProtocolDeviceJTable.hpp"
#include "lumex/tests/core/protocol/devices/device_k/LumexProtocolDeviceKTable.hpp"
#include "lumex/tests/core/protocol/devices/device_l/LumexProtocolDeviceL.hpp"
#include "lumex/tests/core/protocol/devices/device_m/LumexProtocolDeviceM.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace dev_a = lumex_tests::device_a;
namespace dev_b = lumex_tests::device_b;
namespace dev_c = lumex_tests::device_c;
namespace dev_d = lumex_tests::device_d;
namespace dev_e = lumex_tests::tables_e;
namespace dev_f = lumex_tests::tables_f;
namespace dev_g = lumex_tests::tables_g;
namespace dev_h = lumex_tests::device_h;
namespace dev_i = lumex_tests::tables_i;
namespace dev_j = lumex_tests::tables_j;
namespace dev_k = lumex_tests::tables_k;
namespace dev_l = lumex_tests::device_l;
namespace dev_m = lumex_tests::device_m;

void
fail (char const *expression, int line)
{
  std::fprintf (stderr, "check failed at line %d: %s\n", line, expression);
  std::abort ();
}

#define REQUIRE(condition)                                                    \
  do                                                                          \
    {                                                                         \
      if (!(condition))                                                       \
        fail (#condition, __LINE__);                                          \
    }                                                                         \
  while (false)

// A session that knows every value as 0.
bool
zero_lookup (void const *, char const *, std::int64_t &value)
{
  value = 0;
  return true;
}

field::decode_context_t
session ()
{
  field::decode_context_t const result = { nullptr, &zero_lookup };
  return result;
}

// What a dispatch visitor saw: the row, the number of calls and the message
// encoded again from the value it got.
template <typename P> struct seen_t
{
  std::string row;
  std::size_t calls = 0;
  bool encoded = true;
  // The decoders are lenient, the encoders refuse a value outside the limits
  // of a registry entry or an entry without the access the command needs.
  bool refused = false;
  layout::bytes_t again;
  field::decode_context_t const *context = nullptr;

  template <typename Row>
  void
  request (typename layout::request_of<Row>::type const &record)
  {
    ++calls;
    row = Row::identifier ();
    note (layout::encode_message_record<P, Row> (again, record, context));
  }

  template <typename Row, typename Value>
  void
  reply (Value const &value)
  {
    ++calls;
    row = Row::identifier ();
    encode_again<Row> (value, layout::has_core_reply<Row> ());
  }

private:
  void
  note (layout::encode_result_t const &result)
  {
    encoded = result.has_value ();
    if (!encoded)
      refused
          = result.error ()
                == lumex::core::protocol::error::make_error_code (
                    lumex::core::protocol::error::protocol_errc::out_of_limits)
            || result.error ()
                   == lumex::core::protocol::error::make_error_code (
                       lumex::core::protocol::error::protocol_errc::
                           access_denied);
  }

  template <typename Row, typename Value>
  void
  encode_again (Value const &value, std::true_type)
  {
    note (layout::encode_reply_message<P, Row> (again, value, context));
  }

  // A reply kind of the dialogue: dispatch hands over the message itself.
  template <typename Row>
  void
  encode_again (layout::bytes_view_t const &message, std::false_type)
  {
    again.assign (message.begin (), message.end ());
  }
};

// `sample`: the message is a sample of the table (the sample request or the
// sample reply of a row), which must come back from dispatch byte for byte.
template <typename P>
void
run_layer (layout::bytes_view_t message, index::frame_direction direction,
           bool sample)
{
  field::decode_context_t const context = session ();
  seen_t<P> first;
  first.context = &context;
  std::error_code const error
      = layout::dispatch_message<P> (message, direction, first, &context);
  if (error)
    {
      REQUIRE (first.calls == 0);
      return;
    }
  REQUIRE (first.calls == 1);
  // The index finds the row dispatch called.
  index::index_result_t const &protocol = index::protocol_index<P> ();
  REQUIRE (protocol.has_value ());
  index::frame_match_t const found = protocol->match (message);
  REQUIRE (found.row != nullptr);
  REQUIRE (found.row->entry->identifier == first.row);
  // Decode and encode are stable after one round.
  if (first.refused)
    {
      REQUIRE (!sample);
      return;
    }
  REQUIRE (first.encoded);
  if (sample)
    REQUIRE (first.again
             == layout::bytes_t (message.begin (), message.end ()));
  seen_t<P> second;
  second.context = &context;
  REQUIRE (!layout::dispatch_message<P> (
      layout::bytes_view_t (first.again.data (), first.again.size ()),
      direction, second, &context));
  REQUIRE (second.calls == 1);
  REQUIRE (second.encoded);
  REQUIRE (second.row == first.row);
  REQUIRE (second.again == first.again);
}

using bytes_t = layout::bytes_t;

// The sample request and the sample reply of every row of a layer.
template <typename P> struct seeds_of
{
  template <typename Row>
  static void
  add_row (std::vector<std::pair<bytes_t, bool>> &out)
  {
    field::decode_context_t const context = session ();
    bytes_t request;
    REQUIRE ((layout::encode_message_record<P, Row> (
                  request, field::sample_value<typename Row::fields_type> (),
                  &context)
                  .has_value ()));
    out.push_back (std::make_pair (request, true));
    bytes_t reply;
    REQUIRE (add_reply<Row> (reply, &context, layout::has_core_reply<Row> ()));
    out.push_back (std::make_pair (reply, false));
  }

  template <typename Row>
  static bool
  add_reply (bytes_t &out, field::decode_context_t const *context,
             std::true_type)
  {
    return layout::sample_reply_message<P, Row> (out, context).has_value ();
  }

  template <typename Row>
  static bool
  add_reply (bytes_t &out, field::decode_context_t const *context,
             std::false_type)
  {
    // A reply kind of the dialogue: the header and the key alone.
    return layout::encode_header<P, Row> (out, context).has_value ();
  }

  template <typename List> struct each_row;

  template <typename... Rows> struct each_row<dsl::type_list<Rows...>>
  {
    static void
    add (std::vector<std::pair<bytes_t, bool>> &out)
    {
      int const results[] = { 0, (add_row<Rows> (out), 0)... };
      (void)results;
    }
  };

  static void
  collect (std::vector<std::pair<bytes_t, bool>> &out)
  {
    each_row<typename dsl::commands_of<P>::type>::add (out);
  }
};

struct layer_t
{
  void (*run) (layout::bytes_view_t, index::frame_direction, bool);
  void (*seeds) (std::vector<std::pair<bytes_t, bool>> &);
};

#define LUMEX_FUZZ_LAYER(P) { &run_layer<P>, &seeds_of<P>::collect }

layer_t const layers[] = {
  LUMEX_FUZZ_LAYER (dev_a::family_a),
  LUMEX_FUZZ_LAYER (dev_a::a_stream),
  LUMEX_FUZZ_LAYER (dev_a::a_old),
  LUMEX_FUZZ_LAYER (dev_a::a_system),
  LUMEX_FUZZ_LAYER (dev_b::device_b2),
  LUMEX_FUZZ_LAYER (dev_b::device_b2_m2),
  LUMEX_FUZZ_LAYER (dev_b::b2_system),
  LUMEX_FUZZ_LAYER (dev_b::device_b1),
  LUMEX_FUZZ_LAYER (dev_b::device_b1_rx),
  LUMEX_FUZZ_LAYER (dev_c::c_i2c),
  LUMEX_FUZZ_LAYER (dev_c::device_c),
  LUMEX_FUZZ_LAYER (dev_c::device_c_rx),
  LUMEX_FUZZ_LAYER (dev_d::device_d),
  LUMEX_FUZZ_LAYER (dev_d::d_system),
  LUMEX_FUZZ_LAYER (dev_e::family_e),
  LUMEX_FUZZ_LAYER (dev_e::device_e),
  LUMEX_FUZZ_LAYER (dev_f::family_f),
  LUMEX_FUZZ_LAYER (dev_f::device_f),
  LUMEX_FUZZ_LAYER (dev_g::family_g),
  LUMEX_FUZZ_LAYER (dev_g::device_g),
  LUMEX_FUZZ_LAYER (dev_g::old_g),
  LUMEX_FUZZ_LAYER (dev_h::h_can),
  LUMEX_FUZZ_LAYER (dev_h::device_h),
  LUMEX_FUZZ_LAYER (dev_i::device_i),
  LUMEX_FUZZ_LAYER (dev_i::alt_i),
  LUMEX_FUZZ_LAYER (dev_j::j_text),
  LUMEX_FUZZ_LAYER (dev_j::j_hid),
  LUMEX_FUZZ_LAYER (dev_k::device_k),
  LUMEX_FUZZ_LAYER (dev_l::l_requests),
  LUMEX_FUZZ_LAYER (dev_l::l_replies),
  LUMEX_FUZZ_LAYER (dev_m::device_m1),
  LUMEX_FUZZ_LAYER (dev_m::device_m1_m2),
  LUMEX_FUZZ_LAYER (dev_m::device_m1_m3),
  LUMEX_FUZZ_LAYER (dev_m::device_m2),
};

std::size_t const layer_count = sizeof layers / sizeof layers[0];

// The samples of every layer as inputs ([layer][direction][message]).
std::set<bytes_t> const &
sample_inputs ()
{
  static std::set<bytes_t> const result = []
    {
      std::set<bytes_t> inputs;
      for (std::size_t n = 0; n < layer_count; ++n)
        {
          std::vector<std::pair<bytes_t, bool>> samples;
          layers[n].seeds (samples);
          for (std::size_t s = 0; s < samples.size (); ++s)
            {
              bytes_t input;
              input.push_back (static_cast<std::uint8_t> (n));
              input.push_back (samples[s].second ? 0 : 1);
              input.insert (input.end (), samples[s].first.begin (),
                            samples[s].first.end ());
              inputs.insert (input);
            }
        }
      return inputs;
    }();
  return result;
}
} // namespace

extern "C" int
LLVMFuzzerInitialize (int *argc, char ***argv)
{
  for (int i = 1; i < *argc; ++i)
    {
      std::string const directory ((*argv)[i]);
      if (directory.empty () || directory[0] == '-')
        continue;
      std::size_t number = 0;
      for (std::size_t n = 0; n < layer_count; ++n)
        {
          std::vector<std::pair<bytes_t, bool>> samples;
          layers[n].seeds (samples);
          for (std::size_t s = 0; s < samples.size (); ++s)
            {
              bytes_t seed;
              seed.push_back (static_cast<std::uint8_t> (n));
              seed.push_back (samples[s].second ? 0 : 1);
              seed.insert (seed.end (), samples[s].first.begin (),
                           samples[s].first.end ());
              std::ofstream file (directory + "/seed_"
                                      + std::to_string (number++) + ".bin",
                                  std::ios::binary);
              if (!file)
                return 0;
              file.write (reinterpret_cast<char const *> (seed.data ()),
                          static_cast<std::streamsize> (seed.size ()));
            }
        }
      break;
    }
  return 0;
}

extern "C" int
LLVMFuzzerTestOneInput (std::uint8_t const *data, std::size_t size)
{
  if (size < 2)
    return 0;
  // An exact copy: a read past the message is a read past this buffer.
  bytes_t const message (data + 2, data + size);
  // The input as the seed writer would have made it.
  bytes_t input (data, data + size);
  input[0] = static_cast<std::uint8_t> (data[0] % layer_count);
  input[1] = static_cast<std::uint8_t> (data[1] & 1U);
  layers[data[0] % layer_count].run (
      layout::bytes_view_t (message.data (), message.size ()),
      (data[1] & 1U) != 0 ? index::frame_direction::rx
                          : index::frame_direction::tx,
      sample_inputs ().count (input) != 0);
  return 0;
}
