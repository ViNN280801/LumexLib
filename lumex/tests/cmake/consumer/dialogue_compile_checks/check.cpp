// Compile checks of the dialogue vocabulary
// (lumex/applied/transport/protocol/dsl), built by
// cmake.dialogue_compile_checks. Exactly one of LUMEX_DIALOGUE_GOOD_CASE or
// LUMEX_DIALOGUE_BAD_CASE=<n> is defined; the fixture compiles with warnings
// as errors. Every bad case is a table the dialogue cannot use and must be
// refused with the message of its own check (see CMakeLists.txt for the
// list), most of them when its index is built. The tables are synthetic: an
// invented device, no real protocol.

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include "lumex/applied/transport/protocol/dialogue/LumexDialogueFeatures.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexReplyReader.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"

#if defined(LUMEX_DIALOGUE_GOOD_CASE)
#define LUMEX_DIALOGUE_CASE 0
#else
#define LUMEX_DIALOGUE_CASE LUMEX_DIALOGUE_BAD_CASE
#endif

#if LUMEX_DIALOGUE_CASE == 0                                                  \
    || (LUMEX_DIALOGUE_CASE >= 52 && LUMEX_DIALOGUE_CASE <= 71)               \
    || (LUMEX_DIALOGUE_CASE >= 79 && LUMEX_DIALOGUE_CASE <= 81)
#include <future>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/protocol/client/LumexProtocolClient.hpp"
#include "lumex/applied/transport/protocol/discovery/LumexDiscover.hpp"
#endif

namespace check_tables
{
namespace dialogue = ::lumex::applied::transport::protocol::dialogue;
namespace index = ::lumex::core::protocol::index;
namespace layout = ::lumex::core::protocol::layout;

struct codec_t
{
};

inline bool
match_any (index::row_t const *, layout::bytes_view_t, layout::bytes_view_t)
{
  return true;
}

LUMEX_PROTOCOL_LAYOUT (state_t, (u8_, u8_), ("valve", "mode"));
LUMEX_PROTOCOL_LAYOUT (info_t, (u16_le_), ("firmware"));
LUMEX_PROTOCOL_LAYOUT (label_t, (text_<4>, u8_), ("name", "value"));

// Another protocol, whose rows the bad cases name.
struct other_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
};
#define OTHER_ROWS(X, X_AS) X (FOREIGN, 0x40, "Foreign", (), (), (state_t), ())
LUMEX_PROTOCOL_DEFINE (other_t, codec_t, other_header_t, OTHER_ROWS);

// The protocol options of the header, per case.
#if LUMEX_DIALOGUE_CASE == 7
// 7: state_from with a field index out of range.
#define CHECK_STATE dialogue::state_from<typename P::STATE_t, 5>
#elif LUMEX_DIALOGUE_CASE == 8
// 8: state_from naming a row of another protocol.
#define CHECK_STATE dialogue::state_from<other_t::FOREIGN_t, 0>
#else
#define CHECK_STATE dialogue::state_from<typename P::STATE_t, 1>
#endif
#if LUMEX_DIALOGUE_CASE == 16
// 16: a keepalive with a period of 0.
#define CHECK_KEEPALIVE dialogue::keepalive<typename P::TICK_t, 0>
#elif LUMEX_DIALOGUE_CASE == 17
// 17: a keepalive naming a row of another protocol.
#define CHECK_KEEPALIVE dialogue::keepalive<other_t::FOREIGN_t, 1000>
#else
#define CHECK_KEEPALIVE dialogue::keepalive<typename P::TICK_t, 1000>
#endif
#if LUMEX_DIALOGUE_CASE == 18
// 18: identify_with a row without a reply.
#define CHECK_IDENTIFY dialogue::identify_with<typename P::TICK_t>
#elif LUMEX_DIALOGUE_CASE == 28
// 28: identify_with a row of another protocol.
#define CHECK_IDENTIFY dialogue::identify_with<other_t::FOREIGN_t>
#else
#define CHECK_IDENTIFY dialogue::identify_with<typename P::INFO_t>
#endif
#if LUMEX_DIALOGUE_CASE == 12
// 12: a type that is no protocol option.
#define CHECK_MORE , int
#elif LUMEX_DIALOGUE_CASE == 13
// 13: a protocol option given twice.
#define CHECK_MORE , dialogue::watchdog_ms<20>
#elif LUMEX_DIALOGUE_CASE == 14
// 14: max_in_flight<0>.
#define CHECK_MORE , dialogue::max_in_flight<0>
#elif LUMEX_DIALOGUE_CASE == 35
// 35: by_order rows (GO, by default) with two requests in flight.
#define CHECK_MORE , dialogue::max_in_flight<2>
#else
#define CHECK_MORE , dialogue::max_in_flight<1>
#endif
#if LUMEX_DIALOGUE_CASE == 15
// 15: watchdog_ms<0>.
#define CHECK_WATCHDOG dialogue::watchdog_ms<0>
#elif LUMEX_DIALOGUE_CASE == 37
// 37: a watchdog no longer than the keepalive period.
#define CHECK_WATCHDOG dialogue::watchdog_ms<1000>
#else
#define CHECK_WATCHDOG dialogue::watchdog_ms<5000>
#endif

struct check_t;
struct layer_t;

#if LUMEX_DIALOGUE_CASE == 48
// 48: a layer_set without layers.
#define CHECK_LAYERS dialogue::layer_set<>
#elif LUMEX_DIALOGUE_CASE == 49
// 49: a layer_set of a type that is no layer.
#define CHECK_LAYERS dialogue::layer_set<check_t, int>
#elif LUMEX_DIALOGUE_CASE == 50
// 50: a layer_set naming a layer of another protocol.
#define CHECK_LAYERS dialogue::layer_set<check_t, other_t>
#elif LUMEX_DIALOGUE_CASE == 51
// 51: a layer_set naming a layer twice.
#define CHECK_LAYERS dialogue::layer_set<check_t, check_t>
#else
#define CHECK_LAYERS dialogue::layer_set<check_t, layer_t>
#endif

struct header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
#if LUMEX_DIALOGUE_CASE == 19
  // 19: dialogue_options that is no dialogue::options<...>.
  using dialogue_options = int;
#elif LUMEX_DIALOGUE_CASE == 20
  // 20: dialogue_options the dialogue cannot read (private).
private:
  using dialogue_options = dialogue::options<>;
#else
  template <typename P>
  using dialogue_options
      = dialogue::options<CHECK_KEEPALIVE, CHECK_WATCHDOG, CHECK_STATE,
                          CHECK_IDENTIFY, CHECK_LAYERS CHECK_MORE>;
#endif
};

// The row a bad case adds to the valid rows, per case.
#if LUMEX_DIALOGUE_CASE == 1
// 1: an ack_nak whose Ack is no field, layout or row.
#define CHECK_ROW(X) X (BAD, 0x50, "Bad", (), (), (ack_nak<int, NAK_t>), ())
#elif LUMEX_DIALOGUE_CASE == 2
// 2: an ack_nak whose Ack and Nak are the same.
#define CHECK_ROW(X) X (BAD, 0x50, "Bad", (), (), (ack_nak<NAK_t, NAK_t>), ())
#elif LUMEX_DIALOGUE_CASE == 3
// 3: reply_row naming a row of another protocol.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (u8_), ("value"), (),                                  \
     (reply_row<other_t::FOREIGN_t> ()))
#elif LUMEX_DIALOGUE_CASE == 4
// 4: reply_row of a type that is no row.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (u8_), ("value"), (), (reply_row<int> ()))
#elif LUMEX_DIALOGUE_CASE == 5
// 5: edge_event on a row without stream.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (state_t), (edge_event<0, 0, 1> ()))
#elif LUMEX_DIALOGUE_CASE == 6
// 6: edge_event with a field index out of range.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (state_t),                                     \
     (stream (10), edge_event<5, 0, 1> ()))
#elif LUMEX_DIALOGUE_CASE == 9
// 9: by_reply_key without reply_row.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (u8_), ("value"), (),                                  \
     (correlate (correlation::by_reply_key)))
#elif LUMEX_DIALOGUE_CASE == 10
// 10: reply_row with another correlation.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (u8_), ("value"), (),                                  \
     (reply_row<CHANGE_t> (), correlate (correlation::by_order)))
#elif LUMEX_DIALOGUE_CASE == 11
// 11: a custom correlation without match_with.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (u8_), (correlate (correlation::custom)))
#elif LUMEX_DIALOGUE_CASE == 21
// 21: echo_zero_on_error of a layout.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (echo_zero_on_error<state_t>), ())
#elif LUMEX_DIALOGUE_CASE == 22
// 22: an edge from a value to the same value.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (state_t),                                     \
     (stream (10), edge_event<0, 1, 1> ()))
#elif LUMEX_DIALOGUE_CASE == 23
// 23: an edge of a text field.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (label_t),                                     \
     (stream (10), edge_event<0, 0, 1> ()))
#elif LUMEX_DIALOGUE_CASE == 24
// 24: reply_row on a row with a reply group of its own.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (u8_), ("value"), (u8_), (reply_row<CHANGE_t> ()))
#elif LUMEX_DIALOGUE_CASE == 25
// 25: an acknowledgement row whose reply is itself a reply kind.
#define CHECK_ROW(X) X (BAD, 0x50, "Bad", (), (), (ack_nak<GO_t, NAK_t>), ())
#elif LUMEX_DIALOGUE_CASE == 26
// 26: match_with with another correlation.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (u8_),                                         \
     (match_with (&match_any), correlate (correlation::by_key)))
#elif LUMEX_DIALOGUE_CASE == 27
// 27: an acknowledgement row of another protocol.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (ack_nak<other_t::FOREIGN_t, NAK_t>), ())
#elif LUMEX_DIALOGUE_CASE == 29
// 29: a two_phase whose Ack is no field, layout or row.
#define CHECK_ROW(X) X (BAD, 0x50, "Bad", (), (), (two_phase<int, u8_>), ())
#elif LUMEX_DIALOGUE_CASE == 34
// 34: by_key on a row whose reply kind names a row.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (ack_nak<ACK_t, NAK_t>),                       \
     (correlate (correlation::by_key)))
#elif LUMEX_DIALOGUE_CASE == 36
// 36: a correlation outside the enumeration.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (u8_),                                         \
     (correlate (static_cast<correlation> (9))))
#elif LUMEX_DIALOGUE_CASE == 30
// 30: an echo_set of a type that is no field.
#define CHECK_ROW(X) X (BAD, 0x50, "Bad", (), (), (echo_set<int>), ())
#elif LUMEX_DIALOGUE_CASE == 38
// 38: an echo_set whose type cannot be compared with the last request field.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (u8_), ("value"), (echo_set<label_t>), ())
#elif LUMEX_DIALOGUE_CASE == 39
// 39: an echo_zero_on_error whose type cannot be compared with the last
// request field.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (label_t), ("value"), (echo_zero_on_error<u8_>), ())
#elif LUMEX_DIALOGUE_CASE == 40
// 40: enables_stream naming a row of another protocol.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (u8_), (enables_stream<other_t::FOREIGN_t> ()))
#elif LUMEX_DIALOGUE_CASE == 41
// 41: enables_stream of a type that is no row.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (u8_), (enables_stream<int> ()))
#elif LUMEX_DIALOGUE_CASE == 42
// 42: enables_stream naming a row without stream.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (u8_), (enables_stream<INFO_t> ()))
#elif LUMEX_DIALOGUE_CASE == 43
// 43: enables_stream on a row with request fields.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (u8_), ("value"), (u8_), (enables_stream<STATE_t> ()))
#elif LUMEX_DIALOGUE_CASE == 44
// 44: enables_stream on a dangerous row.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (u8_), (dangerous, enables_stream<STATE_t> ()))
#elif LUMEX_DIALOGUE_CASE == 72
// 72: a two_phase whose completion is a row with stream.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (two_phase<ACK_t, STATE_t>), ())
#elif LUMEX_DIALOGUE_CASE == 73
// 73: a two_phase whose acknowledgement is a row with stream.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (two_phase<STATE_t, CHANGE_t>), ())
#elif LUMEX_DIALOGUE_CASE == 74
// 74: an ack_nak whose positive part is a row with stream.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (ack_nak<STATE_t, NAK_t>), ())
#elif LUMEX_DIALOGUE_CASE == 75
// 75: an ack_nak whose negative part is a row with stream.
#define CHECK_ROW(X)                                                          \
  X (BAD, 0x50, "Bad", (), (), (ack_nak<ACK_t, STATE_t>), ())
#else
#define CHECK_ROW(X)
#endif

#if LUMEX_DIALOGUE_CASE == 45
// 45: a keepalive naming a dangerous row.
#define CHECK_TICK_FLAGS (dangerous)
#else
#define CHECK_TICK_FLAGS ()
#endif

// clang-format off
#define CHECK_ROWS(X, X_AS) \
  X (ACK, 0x06, "Ack", (), (), (), ()) \
  X (NAK, 0x15, "Nak", (), (), (u8_), ()) \
  X (CHANGE, 0x30, "Change", (), (), (state_t), ()) \
  X (INFO, 0x10, "Info", (), (), (info_t), ()) \
  X (TICK, 0x11, "Tick", (u8_), ("seconds"), (), CHECK_TICK_FLAGS) \
  X (STATE, 0x12, "State", (), (), (state_t), \
     (stream (100), edge_event<0, 0, 1> ())) \
  X (TURN_ON, 0x13, "Turn on", (), (), (u8_), (enables_stream<STATE_t> ())) \
  X (GO, 0x01, "Go", (), (), (ack_nak<ACK_t, NAK_t>), (min_interval (10))) \
  X (SET, 0x02, "Set", (u8_), ("value"), (), (reply_row<CHANGE_t> ())) \
  X (ASK, 0x03, "Ask", (), (), (u8_), (match_with (&match_any))) \
  X (ZERO, 0x04, "Zero", (u8_), ("value"), (echo_zero_on_error<u8_>), ()) \
  X (WATCH, 0x05, "Watch", (), (), (), (reply_row<STATE_t> ())) \
  X (BOTH, 0x07, "Both", (), (), (two_phase<ACK_t, INFO_t>), ()) \
  CHECK_ROW (X)
// clang-format on
LUMEX_PROTOCOL_DEFINE (check_t, codec_t, header_t, CHECK_ROWS);

// A layer, per case: a valid one replaces CHANGE and lists SET (which names
// it) again, so SET of the layer names the CHANGE of the layer.
#if LUMEX_DIALOGUE_CASE == 31
// 31: a layer replaces CHANGE, which SET names, without listing SET.
#define LAYER_ROWS(X, X_AS) X (CHANGE, 0x30, "Change", (), (), (info_t), ())
#elif LUMEX_DIALOGUE_CASE == 32
// 32: a layer replaces ACK, which GO names as a part, without listing GO.
#define LAYER_ROWS(X, X_AS) X (ACK, 0x06, "Ack", (), (), (u8_), ())
#elif LUMEX_DIALOGUE_CASE == 46
// 46: a layer replaces STATE, which TURN_ON names, without listing TURN_ON.
#define LAYER_ROWS(X, X_AS)                                                   \
  X (STATE, 0x12, "State", (), (), (info_t), (stream (100)))
#elif LUMEX_DIALOGUE_CASE == 76
// 76: a row of the layer names STATE, a row with stream that the layer
// inherits.
#define LAYER_ROWS(X, X_AS)                                                   \
  X (LBAD, 0x51, "Lbad", (), (), (ack_nak<ACK_t, STATE_t>), ())
#elif LUMEX_DIALOGUE_CASE == 77
// 77: a layer replaces ACK with a row that has stream and lists GO, which
// names it, again.
#define LAYER_ROWS(X, X_AS)                                                   \
  X (ACK, 0x06, "Ack", (), (), (), (stream (100)))                            \
  X (GO, 0x01, "Go", (), (), (ack_nak<ACK_t, NAK_t>), (min_interval (10)))
#elif LUMEX_DIALOGUE_CASE == 78
// 78: a layer replaces ACK with a row that has stream and does not list GO.
#define LAYER_ROWS(X, X_AS) X (ACK, 0x06, "Ack", (), (), (), (stream (100)))
#else
#define LAYER_ROWS(X, X_AS)                                                   \
  X (CHANGE, 0x30, "Change", (), (), (info_t), ())                            \
  X (SET, 0x02, "Set", (u8_), ("value"), (), (reply_row<CHANGE_t> ()))
#endif
#if LUMEX_DIALOGUE_CASE == 33
// 33: a layer whose own header names, as a protocol option, a row the layer
// replaces.
struct layer_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
  using dialogue_options
      = dialogue::options<dialogue::keepalive<check_t::TICK_t, 100>>;
};
#define TICK_ROWS(X, X_AS)                                                    \
  X (TICK, 0x11, "Tick", (u16_le_), ("seconds"), (), ())
LUMEX_PROTOCOL_EXTEND_FRAMED (layer_t, check_t, codec_t, layer_header_t,
                              TICK_ROWS);
#else
LUMEX_PROTOCOL_EXTEND (layer_t, check_t, LAYER_ROWS);
#endif

// The index asks the vocabulary for its checks: building it is enough.
inline void
use_index ()
{
  (void)index::build_index<check_t> ();
  (void)index::build_index<layer_t> ();
}

// The reader of a reply kind compares the echo with the value sent: a pair
// that cannot be compared is refused when the reader is made.
inline void
use_reader ()
{
#if LUMEX_DIALOGUE_CASE == 38 || LUMEX_DIALOGUE_CASE == 39
  (void)dialogue::reply_reader_of<check_t, check_t::BAD_t> ();
#else
  (void)dialogue::reply_reader_of<check_t, check_t::ZERO_t> ();
#endif
}

#if LUMEX_DIALOGUE_CASE == 47
// 47: keepalive_from on a protocol without a keepalive.
inline void
use_generator ()
{
  (void)dialogue::keepalive_from<other_t> ([] () { return std::tuple<> (); });
}
#endif

#if LUMEX_DIALOGUE_CASE == 0
static_assert (dialogue::check_protocol_t<check_t>::value,
               "the valid table passes");
static_assert (dialogue::protocol_traits_t<check_t>::max_in_flight () == 1,
               "the options are read");
static_assert (dialogue::row_traits_t<check_t, check_t::SET_t>::how ()
                   == dialogue::correlation::by_reply_key,
               "the traits are read");
static_assert (
    std::is_same<dialogue::row_traits_t<layer_t, layer_t::SET_t>::reply_row,
                 layer_t::CHANGE_t>::value,
    "the layer that lists SET again names its own CHANGE");
static_assert (dialogue::row_traits_t<check_t, check_t::WATCH_t>::how ()
                   == dialogue::correlation::by_reply_key,
               "reply_row names a row with stream");
static_assert (dialogue::row_traits_t<check_t, check_t::BOTH_t>::how ()
                   == dialogue::correlation::by_order,
               "a kind names a row without stream");
#endif
} // namespace check_tables

#if LUMEX_DIALOGUE_CASE == 0                                                  \
    || (LUMEX_DIALOGUE_CASE >= 52 && LUMEX_DIALOGUE_CASE <= 71)               \
    || (LUMEX_DIALOGUE_CASE >= 79 && LUMEX_DIALOGUE_CASE <= 81)
// The generated client: what a call cannot be.
namespace check_client
{
namespace dialogue = ::lumex::applied::transport::protocol::dialogue;
namespace pr = ::lumex::applied::transport::protocol;

struct client_codec_t
{
};

// A header with identify_with, one without, one naming a row with fields.
struct named_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::INFO_t>>;
};

struct fields_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::SET_t>>;
};

struct plain_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
};

LUMEX_PROTOCOL_LAYOUT (who_t, (u16_le_), ("firmware"));
LUMEX_PROTOCOL_LAYOUT (bad_t, (u8_, u8_), ("a", "b"));

// clang-format off
#define CLIENT_ROWS(X, X_AS) \
  X (INFO, 0x01, "Info", (), (), (who_t), ()) \
  X (SET, 0x02, "Set", (u8_), ("value"), (u8_), ()) \
  X (ERASE, 0x03, "Erase", (u8_), ("block"), (u8_), (dangerous)) \
  X (MOVE, 0x04, "Move", (u8_), ("to"), (ack_nak<u8_, bad_t>), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (client_t, client_codec_t, named_header_t, CLIENT_ROWS);
LUMEX_PROTOCOL_DEFINE (no_identify_t, client_codec_t, plain_header_t,
                       CLIENT_ROWS);
LUMEX_PROTOCOL_DEFINE (no_reader_t, client_codec_t, named_header_t,
                       CLIENT_ROWS);
LUMEX_PROTOCOL_DEFINE (with_fields_t, client_codec_t, fields_header_t,
                       CLIENT_ROWS);

// Rows named like a member of the client: a plain function, an overloaded one,
// a template, a row of the base layer, the class name, a member type; and a
// row of another identifier whose specification name is that of a member.
// clang-format off
#define STATISTICS_ROWS(X, X_AS) \
  X (statistics, 0x01, "Statistics", (), (), (u8_), ())
#define IDENTIFY_ROWS(X, X_AS) \
  X (identify, 0x01, "Identify", (), (), (u8_), ())
#define ON_ROWS(X, X_AS) \
  X (on, 0x01, "On", (), (), (u8_), ())
#define SESSION_ROWS(X, X_AS) \
  X (session, 0x01, "Session", (), (), (u8_), ())
#define CLIENT_NAME_ROWS(X, X_AS) \
  X (client, 0x01, "Client", (), (), (u8_), ())
#define EXECUTOR_ROWS(X, X_AS) \
  X (executor_type, 0x01, "Executor", (), (), (u8_), ())
#define LAYER_NAME_ROWS(X, X_AS) \
  X (client_layer, 0x01, "Client layer", (), (), (u8_), ())
#define NO_ROWS(X, X_AS)
#define OTHER_ID_ROWS(X, X_AS) \
  X (ASK, 0x01, "Ask", (), (), (u8_), ()) \
  X_AS (SHUT, "close", 0x02, "Shut", (), (), (u8_), ()) \
  X_AS (ATTACH, "on", 0x03, "Attach", (u8_), ("what"), (u8_), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (hides_plain_t, client_codec_t, plain_header_t,
                       STATISTICS_ROWS);
LUMEX_PROTOCOL_DEFINE (hides_overloaded_t, client_codec_t, plain_header_t,
                       IDENTIFY_ROWS);
LUMEX_PROTOCOL_DEFINE (hides_template_t, client_codec_t, plain_header_t,
                       ON_ROWS);
LUMEX_PROTOCOL_DEFINE (hides_in_base_t, client_codec_t, plain_header_t,
                       SESSION_ROWS);
LUMEX_PROTOCOL_EXTEND (hides_in_layer_t, hides_in_base_t, NO_ROWS);
LUMEX_PROTOCOL_DEFINE (hides_class_name_t, client_codec_t, plain_header_t,
                       CLIENT_NAME_ROWS);
LUMEX_PROTOCOL_DEFINE (hides_type_t, client_codec_t, plain_header_t,
                       EXECUTOR_ROWS);
LUMEX_PROTOCOL_DEFINE (layer_name_t, client_codec_t, plain_header_t,
                       LAYER_NAME_ROWS);
// clang-format off
#define ON_RECONNECT_ROWS(X, X_AS) \
  X (on_reconnect, 0x01, "A member", (), (), (u8_), ())
#define IDENTITY_ROWS(X, X_AS) \
  X (identity, 0x01, "A member", (), (), (u8_), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (hides_on_reconnect_t, client_codec_t, plain_header_t,
                       ON_RECONNECT_ROWS);
LUMEX_PROTOCOL_DEFINE (hides_identity_t, client_codec_t, plain_header_t,
                       IDENTITY_ROWS);
LUMEX_PROTOCOL_DEFINE (other_identifier_t, client_codec_t, plain_header_t,
                       OTHER_ID_ROWS);

// A client whose layer set holds a layer with a row named like a member of
// client_layer.
struct layer_with_on_t;

struct layered_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static constexpr std::size_t key_width = 1;
  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::layer_set<P, layer_with_on_t>>;
};
// clang-format off
#define LAYERED_ROWS(X, X_AS) \
  X (ASK, 0x01, "Ask", (), (), (u8_), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (layered_t, client_codec_t, layered_header_t,
                       LAYERED_ROWS);
LUMEX_PROTOCOL_EXTEND (layer_with_on_t, layered_t, ON_ROWS);
} // namespace check_client

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
template <> struct identity_of_t<::check_client::client_t>
{
  static std::error_code
  read (reply_of<::check_client::client_t::INFO_t> const &, identity_t &)
  {
    return std::error_code ();
  }
};

template <> struct identity_of_t<::check_client::with_fields_t>
{
  static std::error_code
  read (reply_of<::check_client::with_fields_t::SET_t> const &, identity_t &)
  {
    return std::error_code ();
  }
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

namespace check_client
{
using value_handler_t = void (*) (std::error_code, std::uint8_t);

// A wire of a stream, with an out-of-band sequence.
struct check_wire_t
{
  static pr::frame_link
  link (pr::discovery_candidate_t const &,
        ::lumex::applied::transport::stream::byte_stream)
  {
    return pr::frame_link ();
  }

  static std::vector<std::uint8_t>
  out_of_band (pr::discovery_candidate_t const &)
  {
    return std::vector<std::uint8_t> ();
  }
};

template <typename P>
inline void
discover_with (::lumex::applied::transport::executor_type const &executor)
{
  pr::async_discover<P, check_wire_t> (
      executor, std::vector<pr::discovery_candidate_t> (),
      pr::discovery_options_t (),
      [] (std::error_code, std::vector<pr::discovery_result_t>) {});
}

inline void
use_client (pr::client<client_t> &client, value_handler_t handler)
{
  std::uint8_t const one = 1;
#if LUMEX_DIALOGUE_CASE == 0
  // Every form of a row, and of a dangerous row.
  (void)client.SET (one);
  (void)client.SET (one, ::lumex::applied::transport::deadline_t (5));
  client.SET (one, handler);
  (void)client.ERASE (pr::confirm, one);
  (void)client.ERASE (pr::confirm, one,
                      ::lumex::applied::transport::deadline_t (5));
  client.ERASE (pr::confirm, one, handler);
  (void)client.identify ();
  (void)client.identity ();
  client.on_reconnect (
      [] (std::error_code const &, pr::identity_t const &) {});
  client.on_reconnect (nullptr);
  (void)client.as<client_t> ();
  (void)client.on<client_t::INFO_t> (
      [] (pr::reply_of<client_t::INFO_t> const &) {});
  // A row of another identifier may carry the specification name of a member.
  static_assert (sizeof (pr::client<other_identifier_t>) > 0, "it compiles");
  static_assert (
      sizeof (pr::client_layer<other_identifier_t, other_identifier_t>) > 0,
      "it compiles as a layer too");
#elif LUMEX_DIALOGUE_CASE == 52
  // 52: a dangerous row without confirm.
  (void)client.ERASE (one);
#elif LUMEX_DIALOGUE_CASE == 53
  // 53: confirm on a row that is not dangerous.
  (void)client.SET (pr::confirm, one);
#elif LUMEX_DIALOGUE_CASE == 54
  // 54: too few arguments.
  (void)client.SET ();
#elif LUMEX_DIALOGUE_CASE == 55
  // 55: too many arguments.
  client.SET (one, one, handler);
#elif LUMEX_DIALOGUE_CASE == 56
  // 56: one more argument, which is no deadline and no token.
  (void)client.SET (one, 2);
#elif LUMEX_DIALOGUE_CASE == 57
  // 57: as<L> of a layer that is no layer of this client.
  (void)client.as<no_identify_t> ();
#elif LUMEX_DIALOGUE_CASE == 58
  // 58: identify without identify_with.
  pr::client<no_identify_t> *const other = nullptr;
  (void)other->identify ();
#elif LUMEX_DIALOGUE_CASE == 59
  // 59: identify without identity_of_t.
  pr::client<no_reader_t> *const other = nullptr;
  (void)other->identify ();
#elif LUMEX_DIALOGUE_CASE == 60
  // 60: identify by a row with request fields.
  pr::client<with_fields_t> *const other = nullptr;
  (void)other->identify ();
#elif LUMEX_DIALOGUE_CASE == 61
  // 61: on<Row> of a row whose reply is a reply kind.
  (void)client.on<client_t::MOVE_t> ([] (std::uint8_t const &) {});
#elif LUMEX_DIALOGUE_CASE == 62
  // 62: a row of another protocol.
  (void)client.lumex_invoke<no_identify_t::SET_t> (one);
#elif LUMEX_DIALOGUE_CASE == 63
  // 63: a dangerous row without confirm, in the async form.
  client.ERASE (one, handler);
#elif LUMEX_DIALOGUE_CASE == 64
  // 64: a row named like a member function of the client.
  (void)sizeof (pr::client<hides_plain_t>);
#elif LUMEX_DIALOGUE_CASE == 65
  // 65: a row named like an overloaded member.
  (void)sizeof (pr::client<hides_overloaded_t>);
#elif LUMEX_DIALOGUE_CASE == 66
  // 66: a row named like a member template.
  (void)sizeof (pr::client<hides_template_t>);
#elif LUMEX_DIALOGUE_CASE == 67
  // 67: a row of the base layer named like a member.
  (void)sizeof (pr::client<hides_in_layer_t>);
#elif LUMEX_DIALOGUE_CASE == 68
  // 68: a row named like the class.
  (void)sizeof (pr::client<hides_class_name_t>);
#elif LUMEX_DIALOGUE_CASE == 69
  // 69: a row named like a member type.
  (void)sizeof (pr::client<hides_type_t>);
#elif LUMEX_DIALOGUE_CASE == 70
  // 70: a row of a layer named like a member of client_layer.
  (void)sizeof (pr::client_layer<layered_t, layer_with_on_t>);
#elif LUMEX_DIALOGUE_CASE == 71
  // 71: a row of a layer named like the class client_layer.
  (void)sizeof (pr::client_layer<layer_name_t, layer_name_t>);
#elif LUMEX_DIALOGUE_CASE == 79
  // 79: a discovery over a table that names no identify_with.
  discover_with<no_identify_t> (client.get_executor ());
#elif LUMEX_DIALOGUE_CASE == 80
  // 80: a row named like on_reconnect.
  (void)sizeof (pr::client<hides_on_reconnect_t>);
#elif LUMEX_DIALOGUE_CASE == 81
  // 81: a row named like identity.
  (void)sizeof (pr::client<hides_identity_t>);
#endif
#if LUMEX_DIALOGUE_CASE == 0
  discover_with<client_t> (client.get_executor ());
#endif
  (void)client;
  (void)handler;
  (void)one;
}
} // namespace check_client
#endif
