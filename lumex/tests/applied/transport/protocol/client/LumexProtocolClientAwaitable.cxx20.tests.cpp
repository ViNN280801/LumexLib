// The generated client with asio::use_awaitable (C++20, where the toolchain
// has coroutines). The synthetic mixer table.

#include <cstdint>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

#if defined(ASIO_HAS_CO_AWAIT)
#include "asio/awaitable.hpp"
#include "asio/co_spawn.hpp"
#include "asio/use_awaitable.hpp"
#endif

namespace
{
using namespace lumex_client_test;
} // namespace

TEST (LumexProtocolClientAwaitable, Client_Awaitable)
{
#if defined(ASIO_HAS_CO_AWAIT)
  client_bench_t<mixer_v1, mixer> bench;
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1));
  bench.device->set_reply<mixer_v1::RATE_t> (std::uint8_t (55));
  bench.device->set_reply<mixer_v1::ONLY_V1_t> (std::uint8_t (6));
  bool ended = false;
  std::uint8_t value = 0;
  std::uint8_t layered = 0;
  std::error_code wrong;
  std::string layer;
  asio::co_spawn (
      bench.context,
      [&] () -> asio::awaitable<void>
        {
          value = co_await bench.client->RATE (std::uint8_t (1),
                                               asio::use_awaitable);
          pr::identity_t const identity
              = co_await bench.client->async_identify (asio::use_awaitable);
          layer = identity.layer;
          layered = co_await bench.client->as<mixer_v1> ().ONLY_V1 (
              asio::use_awaitable);
          try
            {
              co_await bench.client->RATE (300, asio::use_awaitable);
            }
          catch (std::system_error const &error)
            {
              wrong = error.code ();
            }
          ended = true;
        },
      asio::detached);
  ASSERT_TRUE (bench.run_until ([&] () { return ended; }));
  EXPECT_EQ (value, 55);
  EXPECT_EQ (layer, "mixer_v1_t");
  EXPECT_EQ (layered, 6);
  EXPECT_EQ (wrong, protocol_errc::invalid_value);
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}
