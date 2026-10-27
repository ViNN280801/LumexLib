// The C++20 composition of session operations (REVIEW13 round 4 re-review
// A1 to A4): awaitable_operators || with an operation that completes at once
// and with a timer, asio::cancel_after on a read and a state wait awaited
// with as_tuple, and a parallel_group that waits for one. Each composition
// cancels the session read with a terminal cancellation; the session stays
// open.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <variant>
#include <vector>

// The experimental parts of the vendored Asio (a system include directory,
// as for LumexAsio.hpp); awaitable_operators.hpp does not compile without
// coroutines.
#include <asio/detail/config.hpp>
#if defined(ASIO_HAS_CO_AWAIT)
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/parallel_group.hpp>
#endif
#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
#if defined(ASIO_HAS_CO_AWAIT)
using namespace asio::experimental::awaitable_operators;
#endif
using tr::error::transport_errc;
typedef ss::channel_state state;

class LumexSessionCompositionTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 2 };
  // After the runner: the mock streams it keeps die before their context.
  std::shared_ptr<stream_factory_state> factory_
      = std::make_shared<stream_factory_state> ();
};
} // namespace

// A1: || with an operation that completes at once: the session read gets a
// terminal cancellation right after its initiation.
TEST_F (
    LumexSessionCompositionTest,
    GivenASessionReadOrAnImmediateOperation_WhenAwaited_ThenTheReadIsCancelledAndTheOrReturns)
{
#if defined(ASIO_HAS_CO_AWAIT)
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::future<std::size_t> result = asio::co_spawn (
      runner_.get_executor (),
      [&session] () -> asio::awaitable<std::size_t>
        {
          std::vector<std::uint8_t> buffer (4);
          std::variant<std::size_t, std::monostate> const r = co_await (
              session.async_read_some (tr::mutable_bytes_t (buffer),
                                       asio::use_awaitable)
              || asio::post (asio::use_awaitable));
          co_return r.index ();
        },
      asio::use_future);
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready)
      << "the || waits for the session read for ever";
  EXPECT_EQ (result.get (), 1u);
  EXPECT_EQ (session.state (), state::open);
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}

// A2: || with a timer that fires first.
TEST_F (
    LumexSessionCompositionTest,
    GivenASessionReadOrATimer_WhenTheTimerFiresFirst_ThenTheReadIsCancelled)
{
#if defined(ASIO_HAS_CO_AWAIT)
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::future<std::size_t> result = asio::co_spawn (
      runner_.get_executor (),
      [&session] () -> asio::awaitable<std::size_t>
        {
          std::vector<std::uint8_t> buffer (4);
          asio::steady_timer timer (co_await asio::this_coro::executor);
          timer.expires_after (ms (50));
          std::variant<std::size_t, std::monostate> const r = co_await (
              session.async_read_some (tr::mutable_bytes_t (buffer),
                                       asio::use_awaitable)
              || timer.async_wait (asio::use_awaitable));
          co_return r.index ();
        },
      asio::use_future);
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (result.get (), 1u);
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}

// A3: asio::cancel_after on a read and on a state wait.
TEST_F (
    LumexSessionCompositionTest,
    GivenCancelAfterOnASessionReadAndAStateWait_WhenTheyExpire_ThenBothEndCancelled)
{
#if defined(ASIO_HAS_CO_AWAIT)
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::future<int> result = asio::co_spawn (
      runner_.get_executor (),
      [&session] () -> asio::awaitable<int>
        {
          std::vector<std::uint8_t> buffer (4);
          auto [e1, n] = co_await session.async_read_some (
              tr::mutable_bytes_t (buffer),
              asio::cancel_after (ms (50),
                                  asio::as_tuple (asio::use_awaitable)));
          auto [e2, s] = co_await session.async_wait_state_change (
              state::open, asio::cancel_after (
                               ms (50), asio::as_tuple (asio::use_awaitable)));
          (void)n;
          (void)s;
          co_return (e1 == transport_errc::cancelled ? 1 : 0)
              + (e2 == transport_errc::cancelled ? 2 : 0);
        },
      asio::use_future);
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (result.get (), 3);
  EXPECT_EQ (session.state (), state::open);
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}

// A4: a parallel_group of a read and a timer, wait_for_one.
TEST_F (
    LumexSessionCompositionTest,
    GivenAParallelGroupOfASessionReadAndATimer_WhenWaitingForOne_ThenTheReadIsCancelled)
{
#if defined(ASIO_HAS_CO_AWAIT)
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  asio::steady_timer timer (runner_.get_executor ());
  timer.expires_after (ms (50));
  std::promise<std::error_code> read_error;
  asio::experimental::make_parallel_group (
      session.async_read_some (tr::mutable_bytes_t (buffer), asio::deferred),
      timer.async_wait (asio::deferred))
      .async_wait (asio::experimental::wait_for_one (),
                   [&read_error] (std::array<std::size_t, 2>,
                                  std::error_code e1, std::size_t,
                                  std::error_code)
                     { read_error.set_value (e1); });
  std::future<std::error_code> f = read_error.get_future ();
  ASSERT_EQ (f.wait_for (std::chrono::seconds (2)), std::future_status::ready);
  EXPECT_TRUE (f.get () == transport_errc::cancelled);
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}
