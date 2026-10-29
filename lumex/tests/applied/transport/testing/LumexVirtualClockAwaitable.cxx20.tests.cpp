// A coroutine waits on a timer of the virtual clock (asio::use_awaitable):
// it resumes at the virtual time of its wait, and a cancelled wait reaches it
// as the error of the transport.

#include <chrono>
#include <system_error>
#include <tuple>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"

namespace
{
namespace tr = lumex::applied::transport;
namespace timing = lumex::applied::transport::timing;
using ms = std::chrono::milliseconds;
} // namespace

TEST (LumexVirtualClockAwaitableTest,
      GivenACoroutine_WhenItAwaitsAVirtualWait_ThenItResumesAtTheVirtualTime)
{
#if defined(ASIO_HAS_CO_AWAIT)
  asio::io_context context;
  tr::testing::virtual_clock clock;
  timing::timer timer = clock.source ().make_timer (
      asio::make_strand (context.get_executor ()));
  bool resumed = false;
  timing::time_point_t resumed_at;
  asio::co_spawn (
      timer.get_executor (),
      [&] () -> asio::awaitable<void>
        {
          co_await timer.async_wait_for (ms (500), asio::use_awaitable);
          resumed_at = clock.now ();
          co_await timer.async_wait_for (ms (250), asio::use_awaitable);
          resumed = true;
        },
      asio::detached);
  EXPECT_TRUE (tr::testing::run_until (
      context, clock, [&resumed] { return resumed; }, ms (10000)));
  EXPECT_EQ (resumed_at, timing::time_point_t () + ms (500));
  EXPECT_EQ (clock.now (), timing::time_point_t () + ms (750));
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}

TEST (LumexVirtualClockAwaitableTest,
      GivenACoroutineWaiting_WhenTheTimerIsCancelled_ThenItGetsCancelled)
{
#if defined(ASIO_HAS_CO_AWAIT)
  asio::io_context context;
  tr::testing::virtual_clock clock;
  timing::timer timer = clock.source ().make_timer (
      asio::make_strand (context.get_executor ()));
  bool ended = false;
  std::error_code seen;
  asio::co_spawn (
      timer.get_executor (),
      [&] () -> asio::awaitable<void>
        {
          std::tuple<std::error_code> const result
              = co_await timer.async_wait_for (
                  ms (500), asio::as_tuple (asio::use_awaitable));
          seen = std::get<0> (result);
          ended = true;
        },
      asio::detached);
  EXPECT_FALSE (tr::testing::run_until (
      context, clock, [&ended] { return ended; }, ms (100)));
  EXPECT_EQ (timer.cancel (), 1u);
  EXPECT_TRUE (tr::testing::run_until (
      context, clock, [&ended] { return ended; }, ms (0)));
  EXPECT_EQ (seen, make_error_code (tr::error::transport_errc::cancelled));
  EXPECT_EQ (clock.now (), timing::time_point_t () + ms (100));
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}
