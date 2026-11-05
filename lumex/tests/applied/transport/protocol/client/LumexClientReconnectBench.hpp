// The bench of the reconnect tests of the client: a device of layer a that
// was identified, the handler that counts the reconnects, and the readers of
// what went on the wire. Included only by the reconnect tests (it
// instantiates client<gauge>).

#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_RECONNECT_BENCH_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_RECONNECT_BENCH_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectSupport.hpp"

namespace lumex_client_test
{
/// The handler of a reconnect: counts and keeps the last call.
struct reconnects_seen_t
{
  int calls = 0;
  std::error_code error;
  pr::identity_t identity;
};

inline std::shared_ptr<reconnects_seen_t>
watch_reconnects (pr::client<gauge> &client)
{
  auto const seen = std::make_shared<reconnects_seen_t> ();
  client.on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &identity)
        {
          ++seen->calls;
          seen->error = error;
          seen->identity = identity;
        });
  return seen;
}

/// A bench of a device of layer a, identified.
struct identified_bench_t : stream_bench_t<gauge_a, gauge>
{
  identified_bench_t ()
  {
    device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
    auto const out = identify ();
    EXPECT_FALSE (out->error) << out->error.message ();
    seen = watch_reconnects (*client);
  }

  /// Hangs up, and runs until the device was checked again.
  void
  reconnect ()
  {
    int const before = seen->calls;
    lose ();
    EXPECT_TRUE (run_until ([&] () { return seen->calls != before; }));
    settle ();
  }

  std::shared_ptr<reconnects_seen_t> seen;
};

inline std::uint64_t
requests_of (tr::testing::simulator_statistics_t const &statistics,
             char const *row)
{
  for (std::size_t i = 0; i < statistics.by_row.size (); ++i)
    if (std::string (statistics.by_row[i].row) == row)
      return statistics.by_row[i].requests;
  return 0;
}

/// The frames the client wrote from byte `mark` on (SLIP, payloads below the
/// escape bytes).
inline std::vector<std::vector<std::uint8_t>>
frames_after (std::vector<std::uint8_t> const &bytes, std::size_t mark)
{
  std::vector<std::vector<std::uint8_t>> frames;
  std::vector<std::uint8_t> current;
  for (std::size_t i = mark; i < bytes.size (); ++i)
    {
      if (bytes[i] == 0xC0)
        {
          if (!current.empty ())
            frames.push_back (current);
          current.clear ();
        }
      else
        current.push_back (bytes[i]);
    }
  return frames;
}

/// The first byte (the key) of each frame written from `mark` on.
inline std::vector<int>
keys_after (std::vector<std::uint8_t> const &bytes, std::size_t mark)
{
  std::vector<int> keys;
  std::vector<std::vector<std::uint8_t>> const frames
      = frames_after (bytes, mark);
  for (std::size_t i = 0; i < frames.size (); ++i)
    keys.push_back (frames[i][0]);
  return keys;
}
} // namespace lumex_client_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_RECONNECT_BENCH_HPP
