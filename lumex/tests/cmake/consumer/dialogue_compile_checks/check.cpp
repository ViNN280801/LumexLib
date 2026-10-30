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
#include <type_traits>

#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"

#if defined(LUMEX_DIALOGUE_GOOD_CASE)
#define LUMEX_DIALOGUE_CASE 0
#else
#define LUMEX_DIALOGUE_CASE LUMEX_DIALOGUE_BAD_CASE
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
                          CHECK_IDENTIFY CHECK_MORE>;
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
#else
#define CHECK_ROW(X)
#endif

// clang-format off
#define CHECK_ROWS(X, X_AS) \
  X (ACK, 0x06, "Ack", (), (), (), ()) \
  X (NAK, 0x15, "Nak", (), (), (u8_), ()) \
  X (CHANGE, 0x30, "Change", (), (), (state_t), ()) \
  X (INFO, 0x10, "Info", (), (), (info_t), ()) \
  X (TICK, 0x11, "Tick", (u8_), ("seconds"), (), ()) \
  X (STATE, 0x12, "State", (), (), (state_t), \
     (stream (100), edge_event<0, 0, 1> ())) \
  X (GO, 0x01, "Go", (), (), (ack_nak<ACK_t, NAK_t>), (min_interval (10))) \
  X (SET, 0x02, "Set", (u8_), ("value"), (), (reply_row<CHANGE_t> ())) \
  X (ASK, 0x03, "Ask", (), (), (u8_), (match_with (&match_any))) \
  X (ZERO, 0x04, "Zero", (u8_), ("value"), (echo_zero_on_error<u8_>), ()) \
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
#endif
} // namespace check_tables
