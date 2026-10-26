// An invented device for the examples of lumex/core/protocol: a signal
// beacon with a lamp. It stands for no real device; the table shows what
// the module needs: rows with a key, fields, a reply, options, a row whose
// name is a platform macro (X_AS), and a table of parameters.
#ifndef LUMEX_EXAMPLES_PROTOCOL_BEACON_HPP
#define LUMEX_EXAMPLES_PROTOCOL_BEACON_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace beacon
{
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;

/// The lamp mode, with names for the log.
LUMEX_DEFINE_REFLECTED_ENUM (lamp_mode, std::uint8_t, (off, 0), (blink, 1),
                             (steady, 2))

/// The states of the beacon (what `accepted_in` names).
enum class beacon_state
{
  idle,
  active
};

/// The name of the codec that frames the messages; the table only names it.
struct frame_codec
{
};

/// The message format: the key is the first byte of the frame.
struct header
{
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The state of the beacon: lamp mode, level in percent, temperature in
/// tenths of a degree.
LUMEX_PROTOCOL_LAYOUT (
    state_t,
    (enum_<u8_, lamp_mode>, u8_<unit::percent>,
     i16_le_<scale<std::ratio<1, 10>>, unit::degree_celsius>),
    ("mode", "level", "temperature"));

// clang-format off
#define BEACON_ROWS(X, X_AS) \
  X (PING, 0x01, "Liveness check", (), (), (), (idempotent)) \
  X (SET_LEVEL, 0x02, "Set the lamp level", \
     (u16_le_<scale<std::ratio<1, 100> >, unit::percent>), ("level"), (), \
     (timeout (300), retries (2), \
      accepted_in (beacon_state::idle, beacon_state::active))) \
  X (READ_STATE, 0x03, "Read the state", (), (), (state_t), (idempotent)) \
  X (TICK, 0x04, "Periodic state", (), (), (state_t), (stream (500))) \
  X_AS (CLEAR_LOG, "DELETE", 0x05, "Clear the log", (), (), (), (dangerous))
// clang-format on
LUMEX_PROTOCOL_DEFINE (beacon_v1, frame_codec, header, BEACON_ROWS);

// The parameters of the lamp, with their limits and the firmware that has
// them.
// clang-format off
#define LAMP_PARAMS(P, P_AS) \
  P (BRIGHTNESS, (0x10), "Lamp brightness", \
     (u16_le_<scale<std::ratio<1, 10> >, unit::percent>), RW, \
     (limits (0, 100))) \
  P (FLASH_RATE, (0x11), "Flash rate", \
     (u8_<unit::hertz>), RW, (limits (1, 20), firmware (2, 9))) \
  P_AS (RESET, "DELETE", (0x12), "Restore the factory values", (u8_), W, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (lamp, LAMP_PARAMS);
} // namespace beacon

#endif // !LUMEX_EXAMPLES_PROTOCOL_BEACON_HPP
