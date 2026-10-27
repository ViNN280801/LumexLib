// Compile checks of the protocol table DSL, built by
// cmake.protocol_compile_checks. Exactly one of LUMEX_PROTOCOL_GOOD_CASE or
// LUMEX_PROTOCOL_BAD_CASE=<n> is defined; the fixture compiles with warnings
// as errors. Every bad case is a table that must be refused with the message
// of its own check (see CMakeLists.txt for the list). The tables are
// synthetic: an invented device, no real protocol.

#include <ratio>

#include "lumex/core/protocol/LumexProtocol"

namespace check_tables
{
enum class mode
{
  idle,
  busy
};

enum class other_mode
{
  off
};

struct codec
{
};

struct header
{
};

LUMEX_PROTOCOL_LAYOUT (state_t, (u8_<>, u16_le_<>), ("mode", "level"));

// clang-format off
#define GOOD_ROWS(X, X_AS)                                                    \
  X (PING, 0x01, "Liveness", (), (), (), (idempotent))                        \
  X (SET_LEVEL, 0x02, "Set the level",                                        \
     (u16_le_<scale<std::ratio<1, 100> > >, u8_<>), ("level", "channel"),     \
     (state_t), (timeout (100), retries (1), accepted_in (mode::idle)))       \
  X_AS (ERASE, "DELETE", 0x03, "Erase", (), (), (state_t), (dangerous))
// clang-format on

#if defined(LUMEX_PROTOCOL_GOOD_CASE)
LUMEX_PROTOCOL_DEFINE (good_protocol, codec, header, GOOD_ROWS);
LUMEX_PROTOCOL_CHECK_IDENTIFIERS (GOOD_ROWS);
#define GOOD_LAYER_ROWS(X, X_AS)                                              \
  X_AS (MIN_LEVEL, "min", 0x04, "Lowest level", (), (), (state_t), ())
LUMEX_PROTOCOL_EXTEND (good_layer, good_protocol, GOOD_LAYER_ROWS);
static_assert (::lumex::core::protocol::dsl::command_count<good_layer>::value
                   == 4,
               "the good layer has four rows");
// The keys are different on a one-byte wire (the index needs them to be).
struct good_header
{
  static constexpr std::size_t key_width = 1;
};
LUMEX_PROTOCOL_DEFINE (good_wire, codec, good_header, GOOD_ROWS);
static_assert (::lumex::core::protocol::index::keys_unique<good_wire>::value,
               "the keys of the good table are different on the wire");
#define GOOD_PARAMS(P, P_AS)                                                  \
  P (SPEED, (1), "Speed", (u8_<>), RW, (limits (0, 9)))                       \
  P_AS (DELETE_ALL, "DELETE", ("AZ"), "Delete", (u8_<>), W, ())
LUMEX_PROTOCOL_REGISTRY (good_registry, GOOD_PARAMS);
static_assert (
    ::lumex::core::protocol::registry::is_registry<good_registry>::value,
    "the good registry is a registry");

#elif LUMEX_PROTOCOL_BAD_CASE == 1
// 1: a row with fewer field names than fields.
#define BAD_ROWS(X, X_AS)                                                     \
  X (SET_LEVEL, 0x02, "Set", (u16_le_<>, u8_<>), ("level"), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 2
// 2: a row with more field names than fields.
#define BAD_ROWS(X, X_AS)                                                     \
  X (SET_LEVEL, 0x02, "Set", (u16_le_<>), ("level", "channel"), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 3
// 3: a layout with fewer names than fields.
LUMEX_PROTOCOL_LAYOUT (bad_layout, (u8_<>, u8_<>, u8_<>), ("a", "b"));

#elif LUMEX_PROTOCOL_BAD_CASE == 4
// 4: a row identifier that is a platform macro name, without X_AS.
#define BAD_ROWS(X, X_AS) X (DELETE, 0x03, "Erase", (), (), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 5
// 5: the same through the standalone check, without a protocol.
#define BAD_ROWS(X, X_AS)                                                     \
  X (PING, 0x01, "Liveness", (), (), (), ())                                  \
  X (ERROR, 0x02, "Last error", (), (), (), ())
LUMEX_PROTOCOL_CHECK_IDENTIFIERS (BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 6
// 6: a layer row named after a platform macro.
LUMEX_PROTOCOL_DEFINE (good_protocol, codec, header, GOOD_ROWS);
#define BAD_LAYER_ROWS(X, X_AS) X (min, 0x04, "Lowest", (), (), (), ())
LUMEX_PROTOCOL_EXTEND (bad_layer, good_protocol, BAD_LAYER_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 7
// 7: two reply types.
#define BAD_ROWS(X, X_AS)                                                     \
  X (PING, 0x01, "Liveness", (), (), (state_t, state_t), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 8
// 8: an option given twice.
#define BAD_ROWS(X, X_AS)                                                     \
  X (PING, 0x01, "Liveness", (), (), (), (timeout (10), timeout (20)))
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 9
// 9: a value that is no option.
#define BAD_ROWS(X, X_AS) X (PING, 0x01, "Liveness", (), (), (), (42))
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 10
// 10: a type that is no field in a field group.
#define BAD_ROWS(X, X_AS) X (PING, 0x01, "Liveness", (int), ("n"), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 11
// 11: get_option of an option the row does not have.
LUMEX_PROTOCOL_DEFINE (good_protocol, codec, header, GOOD_ROWS);
static_assert (::lumex::core::protocol::dsl::get_option<
                   ::lumex::core::protocol::dsl::kind::stream> (
                   good_protocol::PING_t::options ())
                       .interval_ms
                   == 0,
               "unreachable");

#elif LUMEX_PROTOCOL_BAD_CASE == 12
// 12: accepted_in with enumerators of two enumerations.
#define BAD_ROWS(X, X_AS)                                                     \
  X (PING, 0x01, "Liveness", (), (), (),                                      \
     (accepted_in (mode::idle, other_mode::off)))
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 13
// 13: a field with two scales.
#define BAD_ROWS(X, X_AS)                                                     \
  X (PING, 0x01, "Liveness",                                                  \
     (u16_le_<scale<std::ratio<1, 10>>, scale<std::ratio<1, 100>>>),          \
     ("level"), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);
#elif LUMEX_PROTOCOL_BAD_CASE == 14
// 14: a row identifier that is a C++ keyword.
#define BAD_ROWS(X, X_AS) X (delete, 0x01, "Erase", (), (), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 15
// 15: a row identifier that is a macro of this translation unit (not of a
// platform header).
#define WIDGET_CODE 5
#define BAD_ROWS(X, X_AS) X (WIDGET_CODE, 0x01, "Code", (), (), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 16
// 16: a field group names a layout of the user's namespace that a word of
// the vocabulary hides (stream).
LUMEX_PROTOCOL_LAYOUT (stream, (u8_<>), ("rate"));
#define BAD_ROWS(X, X_AS) X (PING, 0x01, "Liveness", (stream), ("s"), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 17
// 17: a reply names a word of the vocabulary (level).
#define BAD_ROWS(X, X_AS) X (PING, 0x01, "Liveness", (), (), (level), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 18
// 18: a reply names a layout hidden by the row's own type (row status,
// layout status_t).
LUMEX_PROTOCOL_LAYOUT (status_t, (u8_<>), ("mode"));
#define BAD_ROWS(X, X_AS) X (status, 0x01, "Status", (), (), (status_t), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 19
// 19: a reply that is no field, layout or reply kind.
#define BAD_ROWS(X, X_AS) X (PING, 0x01, "Liveness", (), (), (int), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 20
// 20: a row of an aligned binary layout with a misplaced 2-byte field.
struct aligned_header
{
  typedef ::lumex::core::protocol::layout::binary_layout<
      ::lumex::core::protocol::layout::natural_alignment<0>>
      layout_type;
};
#define BAD_ROWS(X, X_AS)                                                     \
  X (SET, 0x01, "Set", (u8_<>, u16_le_<>), ("a", "b"), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, aligned_header, BAD_ROWS);
inline void
use_bad ()
{
  ::lumex::core::protocol::layout::bytes_t out;
  (void)::lumex::core::protocol::layout::encode_request<bad_protocol,
                                                        bad_protocol::SET_t> (
      out, 1, 2);
}

#elif LUMEX_PROTOCOL_BAD_CASE == 21
// 21: a floating field in a decimal fixed-width layout.
inline void
use_bad ()
{
  ::lumex::core::protocol::layout::bytes_t out;
  (void)::lumex::core::protocol::layout::encode<
      ::lumex::core::protocol::layout::fixed_width_text_layout<10>,
      ::lumex::core::protocol::field::f32_le_<>> (out, 1.0f);
}

#elif LUMEX_PROTOCOL_BAD_CASE == 22
// 22: an optional tail that is not the last field.
LUMEX_PROTOCOL_LAYOUT (bad_layout, (optional_tail_<u8_<>>, u8_<>),
                       ("tail", "after"));
inline void
use_bad ()
{
  (void)::lumex::core::protocol::layout::decode<
      ::lumex::core::protocol::layout::binary_layout<>, bad_layout> (
      ::lumex::core::protocol::layout::bytes_view_t ());
}

#elif LUMEX_PROTOCOL_BAD_CASE == 23
// 23: the reply of a row that has none.
LUMEX_PROTOCOL_DEFINE (good_protocol, codec, header, GOOD_ROWS);
inline void
use_bad ()
{
  (void)::lumex::core::protocol::layout::decode_reply<good_protocol,
                                                      good_protocol::PING_t> (
      ::lumex::core::protocol::layout::bytes_view_t ());
}

#elif LUMEX_PROTOCOL_BAD_CASE == 24
// 24: decoding a reply kind of the dialogue with the core.
template <typename A, typename N> struct ack_nak
{
  typedef ::lumex::core::protocol::dsl::reply_tag lumex_protocol_reply;
};
#define BAD_ROWS(X, X_AS)                                                     \
  X (PING, 0x01, "Liveness", (), (), (ack_nak<u8_<>, u8_<>>), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);
inline void
use_bad ()
{
  (void)::lumex::core::protocol::layout::decode_reply<bad_protocol,
                                                      bad_protocol::PING_t> (
      ::lumex::core::protocol::layout::bytes_view_t ());
}

#elif LUMEX_PROTOCOL_BAD_CASE == 25
// 25: a request with fewer arguments than fields.
LUMEX_PROTOCOL_DEFINE (good_protocol, codec, header, GOOD_ROWS);
inline void
use_bad ()
{
  (void)::lumex::core::protocol::layout::make_request<
      good_protocol::SET_LEVEL_t> (1.5);
}

#elif LUMEX_PROTOCOL_BAD_CASE == 26
// 26: two parts of a split word that share a bit.
LUMEX_PROTOCOL_LAYOUT (bad_layout,
                       (split_<u8_<>, bits_<u8_<>, 0, 4>, bits_<u8_<>, 3, 2>>),
                       ("word"));
inline void
use_bad ()
{
  (void)::lumex::core::protocol::layout::decode<
      ::lumex::core::protocol::layout::binary_layout<>, bad_layout> (
      ::lumex::core::protocol::layout::bytes_view_t ());
}

#elif LUMEX_PROTOCOL_BAD_CASE == 27
// 27: a fractional argument for an integer field without a scale.
LUMEX_PROTOCOL_DEFINE (good_protocol, codec, header, GOOD_ROWS);
inline void
use_bad ()
{
  (void)::lumex::core::protocol::layout::make_request<
      good_protocol::SET_LEVEL_t> (1.5, 2.5);
}

#elif LUMEX_PROTOCOL_BAD_CASE == 28
// 28: a header naming a vocabulary that does not derive from the core one.
struct lone_vocabulary
{
};
struct lone_header
{
  typedef lone_vocabulary lumex_vocabulary;
};
#define BAD_ROWS(X, X_AS) X (PING, 0x01, "Liveness", (), (), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, lone_header, BAD_ROWS);

#elif LUMEX_PROTOCOL_BAD_CASE == 29
// 29: a layout that names itself among its fields.
LUMEX_PROTOCOL_LAYOUT (node_t, (u8_<>, node_t), ("value", "next"));

#elif LUMEX_PROTOCOL_BAD_CASE == 30
// 30: a registry key given an enumerator of another registry.
#define FIRST_PARAMS(P, P_AS) P (SPEED, (1), "Speed", (u8_<>), RW, ())
#define SECOND_PARAMS(P, P_AS) P (LEVEL, (1), "Level", (u8_<>), RW, ())
LUMEX_PROTOCOL_REGISTRY (first_registry, FIRST_PARAMS);
LUMEX_PROTOCOL_REGISTRY (second_registry, SECOND_PARAMS);
void
use_wrong_registry ()
{
  first_registry::id value = first_registry::SPEED;
  ::lumex::core::protocol::registry::registry_key_<
      first_registry,
      ::lumex::core::protocol::field::u8_<>>::assign (value,
                                                      second_registry::LEVEL);
}

#elif LUMEX_PROTOCOL_BAD_CASE == 31
// 31: two rows with one integer key on the wire, asked for the index.
struct narrow_header
{
  static constexpr std::size_t key_width = 1;
};
#define BAD_ROWS(X, X_AS)                                                     \
  X (ONE, 0x01, "One", (), (), (), ())                                        \
  X (TWO, 0x101, "Two", (), (), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, narrow_header, BAD_ROWS);
void
use_index ()
{
  (void)::lumex::core::protocol::index::protocol_index<bad_protocol> ();
}

#elif LUMEX_PROTOCOL_BAD_CASE == 32
// 32: a registry row with two types.
#define BAD_PARAMS(P, P_AS) P (SPEED, (1), "Speed", (u8_<>, u8_<>), RW, ())
LUMEX_PROTOCOL_REGISTRY (bad_registry, BAD_PARAMS);

#elif LUMEX_PROTOCOL_BAD_CASE == 33
// 33: a registry row named after a platform macro without P_AS.
#define BAD_PARAMS(P, P_AS) P (DELETE, (1), "Delete", (u8_<>), RW, ())
LUMEX_PROTOCOL_REGISTRY (bad_registry, BAD_PARAMS);

#elif LUMEX_PROTOCOL_BAD_CASE == 34
// 34: a registry value given a string.
#define VALUE_PARAMS(P, P_AS) P (SPEED, (1), "Speed", (u8_<>), RW, ())
LUMEX_PROTOCOL_REGISTRY (value_registry, VALUE_PARAMS);
void
use_string_value ()
{
  ::lumex::core::protocol::registry::parameter_value_t value
      = ::lumex::core::protocol::registry::parameter_value_t ();
  ::lumex::core::protocol::registry::registry_value_<
      value_registry, 0,
      ::lumex::core::protocol::field::u8_<>>::assign (value, "fast");
}

#elif LUMEX_PROTOCOL_BAD_CASE == 35
// 35: the index of a binary layer whose header names no key width and no
// key reader (0x01 is an int, not the width of the key on the wire).
#define BAD_ROWS(X, X_AS) X (ONE, 0x01, "One", (), (), (), ())
LUMEX_PROTOCOL_DEFINE (bad_protocol, codec, header, BAD_ROWS);
void
use_unwidthed_index ()
{
  (void)::lumex::core::protocol::index::build_index<bad_protocol> ();
}

#elif LUMEX_PROTOCOL_BAD_CASE == 36
// 36: a registry with a floating entry used by registry_value_.
#define FLOAT_PARAMS(P, P_AS) P (RATIO, (1), "Ratio", (f32_le_<>), RW, ())
LUMEX_PROTOCOL_REGISTRY (float_registry, FLOAT_PARAMS);
void
use_float_registry ()
{
  ::lumex::core::protocol::registry::parameter_value_t value
      = ::lumex::core::protocol::registry::parameter_value_t ();
  ::lumex::core::protocol::registry::registry_value_<
      float_registry, 0,
      ::lumex::core::protocol::field::i32_le_<>>::assign (value, 1.5);
}
#endif
} // namespace check_tables

int
main ()
{
  return 0;
}
