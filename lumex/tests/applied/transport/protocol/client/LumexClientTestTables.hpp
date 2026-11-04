// The synthetic table of the tests of the generated client
// (lumex/applied/transport/protocol/client): an invented "mixer" with a base
// and two layers of it the device tells apart by its identity, one row of
// every reply kind, a dangerous row, rows whose specification name differs
// from the identifier, a reply that depends on the session and rows limited
// to a firmware. No real device, its names, codes or values are in it.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_TEST_TABLES_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_TEST_TABLES_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>

#include "lumex/applied/transport/protocol/client/LumexClientTypes.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"

namespace lumex_tests
{
namespace client_tables
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace field = lumex::core::protocol::field;

struct codec_t
{
};

/// The states of the mixer.
enum class mixer_state : std::uint8_t
{
  idle = 0,
  busy = 1
};

struct mixer_t;
struct mixer_v1_t;
struct mixer_v2_t;

/// Takes a reply of PICK only when its value is 7.
inline bool
match_seven (lumex::core::protocol::index::row_t const *,
             lumex::core::protocol::layout::bytes_view_t frame,
             lumex::core::protocol::layout::bytes_view_t)
{
  return frame.size () == 2 && frame.data ()[0] == 0x31
         && frame.data ()[1] == 7;
}

/// A one-byte key first; INFO tells the layer.
struct mixer_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::INFO_t>,
                          dialogue::layer_set<mixer_v1_t, mixer_v2_t>,
                          dialogue::default_timeout_ms<200>>;
};

/// The session value that chooses the layout of the reply of SHAPE.
LUMEX_PROTOCOL_CONTEXT_KEY (mixer_form);

LUMEX_PROTOCOL_LAYOUT (small_t, (u8_), ("a"));
LUMEX_PROTOCOL_LAYOUT (wide_t, (u16_le_), ("a"));
LUMEX_PROTOCOL_LAYOUT (
    shape_t,
    (union_by_context_<mixer_form, case_<0, small_t>, case_<1, wide_t>>),
    ("form"));
LUMEX_PROTOCOL_LAYOUT (info_t, (text_<8>, u16_le_, u8_, u8_),
                       ("model", "firmware", "layer", "form"));
LUMEX_PROTOCOL_LAYOUT (reject_t, (u8_, u16_le_), ("code", "detail"));
LUMEX_PROTOCOL_LAYOUT (change_t, (u8_, u16_le_), ("parameter", "value"));
LUMEX_PROTOCOL_LAYOUT (level_t, (u8_, u8_), ("state", "level"));

// clang-format off
#define MIXER_ROWS(X, X_AS) \
  X (ACK, 0x06, "Accepted", (), (), (), ()) \
  X (REJECT, 0x15, "Refused", (), (), (reject_t), ()) \
  X (CHANGE, 0x30, "A parameter changed", (), (), (change_t), ()) \
  X (INFO, 0x10, "Identity", (), (), (info_t), (idempotent)) \
  X (LEVEL, 0x12, "Level", (), (), (level_t), \
     (stream (100), edge_event<0, 0, 1> ())) \
  X (PICK, 0x31, "Answered by what the matcher takes", (), (), (u8_), \
     (match_with (&match_seven))) \
  X (PING, 0x20, "Liveness", (), (), (u8_), (timeout (300), retries (2))) \
  X (RATE, 0x21, "Set the rate", (u8_), ("rate"), (u8_), ()) \
  X (PAIR, 0x22, "Two arguments", (u8_, u16_le_), ("a", "b"), (u16_le_), ()) \
  X (SHAPE, 0x23, "Depends on the session", (), (), (shape_t), ()) \
  X (MOVE, 0x24, "Move", (u8_), ("target"), (ack_nak<u8_, reject_t>), ()) \
  X (PRIME, 0x25, "Prime", (u16_le_), ("volume"), \
     (two_phase<ACK_t, u16_le_>), (timeout (400))) \
  X (FLOW, 0x26, "Set the flow", (u16_le_), ("flow"), (echo_set<u16_le_>), ()) \
  X (LIMIT, 0x27, "Set the limit", (u8_), ("limit"), \
     (echo_zero_on_error<u8_>), ()) \
  X (MODE, 0x28, "Set the mode", (u8_), ("mode"), \
     (state_instead_of_refusal<u8_>), ()) \
  X (WRITE, 0x29, "Write", (u8_, u16_le_), ("parameter", "value"), (), \
     (reply_row<CHANGE_t> ())) \
  X (RUN, 0x2A, "Run, no reply", (u8_), ("speed"), (), ()) \
  X (ERASE, 0x2B, "Erase", (u8_), ("block"), (u8_), (dangerous)) \
  X_AS (WIPE, "DELETE ALL", 0x2C, "Wipe", (), (), (u8_), (dangerous)) \
  X_AS (PEEK, "PEEK-ONE", 0x2D, "Peek", (u8_), ("at"), (u8_), ()) \
  X (OLD, 0x2E, "An old row", (), (), (u8_), (firmware (1, 9))) \
  X (NEW, 0x2F, "A new row", (), (), (u8_), (firmware (20, 99)))
#define MIXER_V1_ROWS(X, X_AS) \
  X (ONLY_V1, 0x40, "Only the first layer", (), (), (u8_), ())
#define MIXER_V2_ROWS(X, X_AS) \
  X (ONLY_V2, 0x41, "Only the second layer", (u8_), ("a"), (u16_le_), ()) \
  X (PING, 0x42, "Liveness, another key", (), (), (u16_le_), ())
// clang-format on

LUMEX_PROTOCOL_DEFINE (mixer_t, codec_t, mixer_header_t, MIXER_ROWS);
LUMEX_PROTOCOL_EXTEND (mixer_v1_t, mixer_t, MIXER_V1_ROWS);
LUMEX_PROTOCOL_EXTEND (mixer_v2_t, mixer_t, MIXER_V2_ROWS);

/// A header with an edge, a stream enable and other protocol options, for the
/// features of a selected layer: the base has none of them.
struct plain_t;
struct featured_t;

struct plain_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>,
                          dialogue::layer_set<plain_t, featured_t>>;
};

struct featured_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options = dialogue::options<
      dialogue::identify_with<typename P::WHO_t>,
      dialogue::layer_set<plain_t, featured_t>,
      dialogue::keepalive<typename P::BEAT_t, 100>,
      dialogue::watchdog_ms<1000>, dialogue::min_interval_ms<20>,
      dialogue::state_from<typename P::STATUS_t, 0>,
      dialogue::default_timeout_ms<150>, dialogue::default_retries<1>,
      dialogue::max_in_flight<2>>;
};

/// A layer with a window for late replies of a by_order row.
struct windowed_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::late_reply_window_ms<100>>;
};

/// Layers of the plain table that differ in the values a layer gives the
/// dialogue: how many exchanges are in flight, the default timeout, the
/// default retries (quick, slow) and the late reply window (windowed, wide).
struct quick_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options = dialogue::options<dialogue::max_in_flight<2>,
                                             dialogue::default_timeout_ms<100>,
                                             dialogue::default_retries<1>>;
};

struct slow_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options = dialogue::options<dialogue::max_in_flight<4>,
                                             dialogue::default_timeout_ms<300>,
                                             dialogue::default_retries<2>>;
};

struct patient_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::late_reply_window_ms<300>>;
};

LUMEX_PROTOCOL_LAYOUT (who_t, (u8_), ("layer"));

// clang-format off
#define PLAIN_ROWS(X, X_AS) \
  X (WHO, 0x01, "Who", (), (), (who_t), ()) \
  X (BEAT, 0x02, "Beat", (), (), (), ()) \
  X (STATUS, 0x03, "Status", (), (), (u8_), (stream (100))) \
  X (ASK, 0x04, "Ask", (), (), (u8_), ())
#define NO_NEW_ROWS(X, X_AS)
#define WINDOWED_ROWS(X, X_AS) \
  X (ORDERED, 0x05, "Answered in order", (), (), (u8_), \
     (correlate (correlation::by_order)))
// clang-format on

LUMEX_PROTOCOL_DEFINE (plain_t, codec_t, plain_header_t, PLAIN_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (featured_t, plain_t, codec_t, featured_header_t,
                              NO_NEW_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (windowed_t, plain_t, codec_t, windowed_header_t,
                              WINDOWED_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (quick_t, plain_t, codec_t, quick_header_t,
                              NO_NEW_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (slow_t, plain_t, codec_t, slow_header_t,
                              NO_NEW_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (patient_t, plain_t, codec_t, patient_header_t,
                              WINDOWED_ROWS);

/// identify_with without layer_set.
struct solo_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>>;
};

// clang-format off
#define SOLO_ROWS(X, X_AS) \
  X (WHO, 0x01, "Who", (), (), (who_t), ()) \
  X (ASK, 0x04, "Ask", (), (), (u8_), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (solo_t, codec_t, solo_header_t, SOLO_ROWS);

/// A table without identify_with and layer_set.
struct lone_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// clang-format off
#define LONE_ROWS(X, X_AS) \
  X (ASK, 0x01, "Ask", (), (), (u8_), ())
// Two rows of one name: the specification name of the second is the first's
// identifier, which the compiler cannot see and the index refuses.
#define TWINS_ROWS(X, X_AS) \
  X (FIRST, 0x01, "First", (), (), (u8_), ()) \
  X_AS (SECOND, "FIRST", 0x02, "Second", (), (), (u8_), ())
// A row whose specification name is that of a member of the client, with
// another identifier: no clash.
#define SPEC_NAMED_ROWS(X, X_AS) \
  X (ASK, 0x01, "Ask", (), (), (u8_), ()) \
  X_AS (SHUT, "close", 0x02, "Shut", (), (), (u8_), ()) \
  X_AS (ATTACH, "on", 0x03, "Attach", (u8_), ("what"), (u8_), ())
// A row for each name client<P> declares (public and private, functions,
// templates, types, data, the class name): no client can be made of this
// table, the tests read the check on it.
#define MEMBERS_ROWS(X, X_AS) \
  X (lumex_invoke, 0x01, "A member", (), (), (u8_), ()) \
  X (as, 0x02, "A member", (), (), (u8_), ()) \
  X (on, 0x03, "A member", (), (), (u8_), ()) \
  X (unsubscribe, 0x04, "A member", (), (), (u8_), ()) \
  X (on_unknown, 0x05, "A member", (), (), (u8_), ()) \
  X (async_identify, 0x06, "A member", (), (), (u8_), ()) \
  X (identify, 0x07, "A member", (), (), (u8_), ()) \
  X (async_enable_streams, 0x08, "A member", (), (), (u8_), ()) \
  X (session, 0x09, "A member", (), (), (u8_), ()) \
  X (set_session, 0x0A, "A member", (), (), (u8_), ()) \
  X (statistics, 0x0B, "A member", (), (), (u8_), ()) \
  X (cancel, 0x0C, "A member", (), (), (u8_), ()) \
  X (close, 0x0D, "A member", (), (), (u8_), ()) \
  X (get_executor, 0x0E, "A member", (), (), (u8_), ()) \
  X (get_strand, 0x0F, "A member", (), (), (u8_), ()) \
  X (engine, 0x10, "A member", (), (), (u8_), ()) \
  X (executor_type, 0x11, "A member", (), (), (u8_), ()) \
  X (strand_type, 0x12, "A member", (), (), (u8_), ()) \
  X (protocol_type, 0x13, "A member", (), (), (u8_), ()) \
  X (client, 0x14, "A member", (), (), (u8_), ()) \
  X (client_members_t, 0x15, "A member", (), (), (u8_), ()) \
  X (traits, 0x16, "A member", (), (), (u8_), ()) \
  X (identify_signature, 0x17, "A member", (), (), (u8_), ()) \
  X (state_, 0x18, "A member", (), (), (u8_), ()) \
  X (engine_, 0x19, "A member", (), (), (u8_), ()) \
  X (core, 0x1A, "A member", (), (), (u8_), ()) \
  X (identify_sync, 0x1B, "A member", (), (), (u8_), ())
// The same for client_layer<Owner, L>.
#define LAYER_MEMBERS_ROWS(X, X_AS) \
  X (lumex_invoke, 0x01, "A member", (), (), (u8_), ()) \
  X (on, 0x02, "A member", (), (), (u8_), ()) \
  X (client_layer, 0x03, "A member", (), (), (u8_), ()) \
  X (core_, 0x04, "A member", (), (), (u8_), ())
// clang-format on

LUMEX_PROTOCOL_DEFINE (lone_t, codec_t, lone_header_t, LONE_ROWS);
LUMEX_PROTOCOL_DEFINE (twins_t, codec_t, lone_header_t, TWINS_ROWS);
LUMEX_PROTOCOL_DEFINE (spec_named_t, codec_t, lone_header_t, SPEC_NAMED_ROWS);
LUMEX_PROTOCOL_DEFINE (members_t, codec_t, lone_header_t, MEMBERS_ROWS);
LUMEX_PROTOCOL_DEFINE (layer_members_t, codec_t, lone_header_t,
                       LAYER_MEMBERS_ROWS);
} // namespace client_tables
} // namespace lumex_tests

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
/// The identity of the mixer: layer 1 or 2, 0 and the rest are no layer.
template <> struct identity_of_t<lumex_tests::client_tables::mixer_t>
{
  static std::error_code
  read (reply_of<lumex_tests::client_tables::mixer_t::INFO_t> const &reply,
        identity_t &identity)
  {
    namespace ct = lumex_tests::client_tables;
    identity.model = std::string (ct::field::get<0> (reply).chars,
                                  ct::field::get<0> (reply).size ());
    identity.firmware = ct::field::get<1> (reply);
    int const layer = ct::field::get<2> (reply);
    identity.layer
        = layer == 1 ? "mixer_v1_t" : (layer == 2 ? "mixer_v2_t" : "");
    identity.values.push_back (std::make_pair (
        std::string ("mixer_form"), std::int64_t (ct::field::get<3> (reply))));
    return std::error_code ();
  }
};

template <> struct identity_of_t<lumex_tests::client_tables::solo_t>
{
  static std::error_code
  read (reply_of<lumex_tests::client_tables::solo_t::WHO_t> const &,
        identity_t &identity)
  {
    identity.layer = "whatever the device says";
    return std::error_code ();
  }
};

template <> struct identity_of_t<lumex_tests::client_tables::plain_t>
{
  static std::error_code
  read (reply_of<lumex_tests::client_tables::plain_t::WHO_t> const &reply,
        identity_t &identity)
  {
    namespace ct = lumex_tests::client_tables;
    identity.layer = ct::field::get<0> (reply) == 1 ? "featured_t" : "plain_t";
    return std::error_code ();
  }
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_TEST_TABLES_HPP
