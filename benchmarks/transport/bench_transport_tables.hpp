// bench_transport_tables.hpp
// The table the transport benchmarks run on: an invented protocol with a one
// byte key and rows named by their size and rate only. No device, its names,
// codes or values are in it. The loads of the benchmarks:
//
//   - PING:     a 6-byte request and a 6-byte reply (one exchange);
//   - BLOCK:    no request and a 2420-byte reply (one exchange);
//   - WIDE:     no request and a 100-byte reply (the frame `describe` reads);
//   - SIZED_10: a 2420-byte stream message every 100 ms (10 Hz);
//   - SIZED_25: the same every 40 ms (25 Hz);
//   - SMALL_20: a 16-byte stream message every 50 ms (20 Hz);
//   - SMALL_25: the same every 40 ms (25 Hz).
//
// A message size counts the key byte, so a "2420-byte message" is the payload
// the frame codec carries.
#ifndef LUMEX_BENCHMARKS_TRANSPORT_TABLES_HPP
#define LUMEX_BENCHMARKS_TRANSPORT_TABLES_HPP

#include <cstddef>
#include <cstdint>

#include "lumex/applied/transport/protocol/client/LumexClientTypes.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"

namespace bench_transport
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;

struct codec_t
{
};

struct header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::default_timeout_ms<200>>;
};

// 1 key byte + 1209 * 2 + 1 = 2420 bytes.
LUMEX_PROTOCOL_LAYOUT (sized_t, (array_<u16_le_, 1209>, u8_),
                       ("samples", "tail"));
// 1 key byte + 15 = 16 bytes.
LUMEX_PROTOCOL_LAYOUT (small_t, (u32_le_, u32_le_, u32_le_, u16_le_, u8_),
                       ("a", "b", "c", "d", "e"));
// 1 key byte + 99 = 100 bytes.
LUMEX_PROTOCOL_LAYOUT (wide_t,
                       (u32_le_, u32_le_, u32_le_, u32_le_, u32_le_, u32_le_,
                        u32_le_, u32_le_, u32_le_, u32_le_, u32_le_, u32_le_,
                        u32_le_, u32_le_, u32_le_, u32_le_, u32_le_, u32_le_,
                        u32_le_, u32_le_, u32_le_, u32_le_, u32_le_, u32_le_,
                        u16_le_, u8_),
                       ("f00", "f01", "f02", "f03", "f04", "f05", "f06", "f07",
                        "f08", "f09", "f10", "f11", "f12", "f13", "f14", "f15",
                        "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
                        "f24", "f25"));
// 1 key byte + 4 + 1 = 6 bytes.
LUMEX_PROTOCOL_LAYOUT (ping_t, (u32_le_, u8_), ("value", "mode"));

// clang-format off
#define BENCH_TRANSPORT_ROWS(X, X_AS) \
  X (PING, 0x20, "One exchange", (u32_le_, u8_), ("value", "mode"), \
     (ping_t), ()) \
  X (BLOCK, 0x21, "A 2420-byte reply", (), (), (sized_t), ()) \
  X (WIDE, 0x22, "A 100-byte reply", (), (), (wide_t), ()) \
  X (SIZED_10, 0x30, "2420 bytes at 10 Hz", (), (), (sized_t), \
     (stream (100))) \
  X (SIZED_25, 0x31, "2420 bytes at 25 Hz", (), (), (sized_t), \
     (stream (40))) \
  X (SMALL_20, 0x32, "16 bytes at 20 Hz", (), (), (small_t), \
     (stream (50))) \
  X (SMALL_25, 0x33, "16 bytes at 25 Hz", (), (), (small_t), \
     (stream (40)))
// clang-format on

LUMEX_PROTOCOL_DEFINE (bench_protocol_t, codec_t, header_t,
                       BENCH_TRANSPORT_ROWS);
} // namespace bench_transport

#endif // !LUMEX_BENCHMARKS_TRANSPORT_TABLES_HPP
