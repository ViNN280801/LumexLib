// The token styles of the generated client: asio::use_future, asio::deferred,
// asio::yield_context (with LUMEX_TRANSPORT_WITH_SPAWN, skipped without it);
// the callback style runs through the other suites, use_awaitable in the C++20
// suite. The synthetic mixer table.

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace
{
using namespace lumex_client_test;
using threaded_t = thread_bench_t<mixer>;
using polled_t = client_bench_t<mixer>;

template <typename T>
bool
ready (std::future<T> &future)
{
  return future.wait_for (std::chrono::seconds (20))
         == std::future_status::ready;
}
} // namespace

TEST (LumexProtocolClientTokens, Client_UseFuture)
{
  threaded_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (33));
  std::future<std::uint8_t> reply
      = bench.client->RATE (std::uint8_t (1), asio::use_future);
  ASSERT_TRUE (ready (reply));
  EXPECT_EQ (reply.get (), 33);
}

TEST (LumexProtocolClientTokens, Client_UseFutureGivesTheErrorAsAnException)
{
  threaded_t bench;
  std::future<std::uint8_t> reply = bench.client->RATE (300, asio::use_future);
  ASSERT_TRUE (ready (reply));
  try
    {
      reply.get ();
      FAIL ();
    }
  catch (std::system_error const &error)
    {
      EXPECT_EQ (error.code (), protocol_errc::invalid_value);
    }
}

TEST (LumexProtocolClientTokens, Client_UseFutureOfIdentify)
{
  thread_bench_t<mixer_v2, mixer> bench;
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  std::future<pr::identity_t> identity
      = bench.client->async_identify (asio::use_future);
  ASSERT_TRUE (ready (identity));
  EXPECT_EQ (identity.get ().layer, "mixer_v2_t");
}

TEST (LumexProtocolClientTokens, Client_Deferred)
{
  polled_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (9));
  auto op = bench.client->RATE (std::uint8_t (1), asio::deferred);
  bench.settle ();
  // Nothing happens until it is started.
  EXPECT_EQ (bench.requests (), 0u);
  auto const out = new_outcome<mixer::RATE_t> ();
  std::move (op) (into<mixer::RATE_t> (out));
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_EQ (out->value, 9);
  EXPECT_EQ (bench.requests (), 1u);
}

TEST (LumexProtocolClientTokens, Client_DeferredWithAnArgumentThatDoesNotFit)
{
  polled_t bench;
  auto op = bench.client->RATE (300, asio::deferred);
  auto const out = new_outcome<mixer::RATE_t> ();
  std::move (op) (into<mixer::RATE_t> (out));
  EXPECT_EQ (out->calls, 0);
  bench.settle ();
  EXPECT_EQ (out->error, protocol_errc::invalid_value);
  EXPECT_EQ (bench.requests (), 0u);
}

TEST (LumexProtocolClientTokens, Client_DeferredOfIdentifyAndOfALayer)
{
  client_bench_t<mixer_v1, mixer> bench;
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1));
  bench.device->set_reply<mixer_v1::ONLY_V1_t> (std::uint8_t (5));
  auto identify = bench.client->async_identify (asio::deferred);
  auto const who = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { who };
  std::move (identify) (done);
  ASSERT_TRUE (bench.run_until ([&] () { return who->calls != 0; }));
  EXPECT_FALSE (who->error) << who->error.message ();
  auto op = bench.client->as<mixer_v1> ().ONLY_V1 (asio::deferred);
  auto const out = new_outcome<mixer_v1::ONLY_V1_t> ();
  std::move (op) (into<mixer_v1::ONLY_V1_t> (out));
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_EQ (out->value, 5);
}

TEST (LumexProtocolClientTokens, Client_DeferredStartedAfterTheClientIsGone)
{
  polled_t bench;
  auto op = bench.client->RATE (std::uint8_t (1), asio::deferred);
  bench.client.reset ();
  bench.settle ();
  std::promise<std::error_code> ended;
  std::future<std::error_code> result = ended.get_future ();
  std::move (op) ([&ended] (std::error_code error, std::uint8_t)
                    { ended.set_value (error); });
  // On the system executor: the strand is gone with the dialogue.
  ASSERT_TRUE (ready (result));
  EXPECT_EQ (result.get (), transport_errc::closed);
}

TEST (LumexProtocolClientTokens, Client_YieldContext)
{
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  polled_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (21));
  std::uint8_t value = 0;
  std::error_code error;
  std::error_code wrong;
  bool ended = false;
  asio::spawn (
      bench.context,
      [&] (asio::yield_context yield)
        {
          value = bench.client->RATE (std::uint8_t (1), yield[error]);
          bench.client->RATE (300, yield[wrong]);
          ended = true;
        },
      asio::detached);
  ASSERT_TRUE (bench.run_until ([&] () { return ended; }));
  EXPECT_FALSE (error) << error.message ();
  EXPECT_EQ (value, 21);
  EXPECT_EQ (wrong, protocol_errc::invalid_value);
#else
  GTEST_SKIP () << "LUMEX_TRANSPORT_WITH_SPAWN is off";
#endif
}
