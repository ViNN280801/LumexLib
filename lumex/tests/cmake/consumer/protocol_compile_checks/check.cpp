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
#endif
} // namespace check_tables

int
main ()
{
  return 0;
}
