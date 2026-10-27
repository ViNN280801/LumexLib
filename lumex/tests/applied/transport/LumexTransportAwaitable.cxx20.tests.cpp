// The asio::use_awaitable token of the transport under C++20, over the mock
// stream and a loopback TCP echo peer (the fixture and its instantiation are
// those of LumexTransportTokens.cxx11.tests.cpp), and an awaited operation
// that asio::cancel_after cancels.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <system_error>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "LumexTransportTokenFixture.hpp"

using lumex_transport_tokens::LumexTransportTokenTest;
using lumex_transport_tokens::ping;
namespace tr = lumex::applied::transport;

TEST_P (LumexTransportTokenTest,
        GivenUseAwaitable_WhenOpenWriteAndRead_ThenTheReplyArrives)
{
#if defined(ASIO_HAS_CO_AWAIT)
  tr::stream::byte_stream &stream = this->stream ();
  std::vector<std::uint8_t> const request = ping ();
  std::vector<std::uint8_t> reply (request.size ());
  std::promise<void> done;
  asio::co_spawn (
      runner_.get_executor (),
      [&] () -> asio::awaitable<void>
        {
          co_await stream.async_open (asio::use_awaitable);
          co_await stream.async_write_all (tr::const_bytes_t (request),
                                           asio::use_awaitable);
          std::size_t received = 0;
          while (received < reply.size ())
            received += co_await stream.async_read_some (
                tr::mutable_bytes_t (reply.data () + received,
                                     reply.size () - received),
                asio::use_awaitable);
        },
      [&] (std::exception_ptr failure)
        {
          if (failure)
            done.set_exception (failure);
          else
            done.set_value ();
        });
  done.get_future ().get ();
  EXPECT_EQ (reply, request);
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}

TEST (
    LumexTransportAwaitableCancellation,
    GivenAnAwaitedReadWithCancelAfter_WhenTheTimeoutPasses_ThenItEndsCancelled)
{
#if defined(ASIO_HAS_CO_AWAIT)
  tr::stream::io_runner runner (1);
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "awaited");
  ASSERT_TRUE (mock.stream.open (tr::deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> buffer (3);
  std::future<std::error_code> result = asio::co_spawn (
      runner.get_executor (),
      [&mock, &buffer] () -> asio::awaitable<std::error_code>
        {
          std::tuple<std::error_code, std::size_t> const read
              = co_await mock.stream.async_read_some (
                  tr::mutable_bytes_t (buffer),
                  asio::cancel_after (std::chrono::milliseconds (20),
                                      asio::as_tuple (asio::use_awaitable)));
          co_return std::get<0> (read);
        },
      asio::use_future);
  ASSERT_EQ (result.wait_for (std::chrono::seconds (5)),
             std::future_status::ready);
  EXPECT_TRUE (result.get () == tr::error::transport_errc::cancelled);
#else
  GTEST_SKIP () << "the toolchain has no coroutines (ASIO_HAS_CO_AWAIT)";
#endif
}
