// A consumer of lumex::protocol built with LUMEX_PROTOCOL_WITH_JSON=OFF: the
// macro is not defined, and the registry, the index, describe and the
// Markdown export work. Exit code 0 when they do. The table is synthetic: an
// invented device, no real protocol.

#include <cstdint>
#include <cstdio>
#include <string>

#include "lumex/core/protocol/LumexProtocol"

#ifdef LUMEX_PROTOCOL_WITH_JSON
#error "LUMEX_PROTOCOL_WITH_JSON must not be defined when the option is OFF"
#endif

namespace fixture
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;

struct codec
{
};

struct header
{
  static constexpr std::size_t key_width = 1;
};

// clang-format off
#define LAMP_PARAMS(P, P_AS) \
  P (SPEED, (1), "Speed", (u8_<unit::percent>), RW, (limits (0, 100)))
#define LAMP_ROWS(X, X_AS) \
  X (PING, 0x01, "Liveness check", (), (), (), ()) \
  X (SET_LEVEL, 0x02, "Set the level", (u8_<unit::percent>), ("level"), (), ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (lamp, LAMP_PARAMS);
LUMEX_PROTOCOL_DEFINE (lamp_protocol, codec, header, LAMP_ROWS);
} // namespace fixture

int
main ()
{
  namespace index = lumex::core::protocol::index;
  index::index_result_t const &built
      = index::protocol_index<fixture::lamp_protocol, fixture::lamp> ();
  if (!built.has_value ())
    return 1;
  std::uint8_t const frame[] = { 0x02, 0x2A };
  std::string const line = built->describe (
      lumex::core::protocol::layout::bytes_view_t (frame, 2),
      index::frame_direction::tx);
  if (line != "TX lamp_protocol.SET_LEVEL (Set the level) level=42 %")
    {
      std::printf ("%s\n", line.c_str ());
      return 2;
    }
  std::string const document = index::export_markdown (*built);
  if (document.find ("## Registry lamp") == std::string::npos
      || document.find ("### SET_LEVEL") == std::string::npos)
    return 3;
  return 0;
}
