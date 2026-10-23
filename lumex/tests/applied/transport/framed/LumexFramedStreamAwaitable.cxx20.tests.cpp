// The asio::use_awaitable token of framed_stream under C++20, over the mock
// stream, and an awaited read that asio::cancel_after cancels.

#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <system_error>
#include <tuple>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"

namespace
{
using namespace lumex_framed_test;
}

TEST (LumexFramedStreamAwaitable,
      GivenUseAwaitable_WhenWriteAndRead_ThenTheFrameComesBack)
{
  log_capture log;
  stream_rig<slip_case> rig;
  rig.open ();
  bytes_t const payload (6, 'w');
  std::promise<bytes_t> done;
  asio::co_spawn (
      rig.runner.get_executor (),
      [&] () -> asio::awaitable<void>
        {
          co_await rig.link.async_write_frame (tr::const_bytes_t (payload),
                                               asio::use_awaitable);
          bytes_t const written = rig.mock.control.written ();
          rig.mock.control.feed (written);
          tr::framed::frame_t frame
              = co_await rig.link.async_read_frame (asio::use_awaitable);
          done.set_value (frame);
        },
      [&] (std::exception_ptr failure)
        {
          if (failure)
            done.set_exception (failure);
        });
  EXPECT_EQ (done.get_future ().get (), payload);
}

TEST (
    LumexFramedStreamAwaitable,
    GivenAnAwaitedReadWithCancelAfter_WhenTheTimeoutPasses_ThenItEndsCancelled)
{
  log_capture log;
  stream_rig<slip_case> rig;
  rig.open ();
  std::future<std::error_code> result = asio::co_spawn (
      rig.runner.get_executor (),
      [&rig] () -> asio::awaitable<std::error_code>
        {
          std::tuple<std::error_code, tr::framed::frame_t> const read
              = co_await rig.link.async_read_frame (
                  asio::cancel_after (std::chrono::milliseconds (20),
                                      asio::as_tuple (asio::use_awaitable)));
          co_return std::get<0> (read);
        },
      asio::use_future);
  ASSERT_EQ (result.wait_for (std::chrono::seconds (5)),
             std::future_status::ready);
  EXPECT_TRUE (result.get () == tr::error::transport_errc::cancelled);
}

TEST (LumexFramedStreamAwaitable,
      GivenAnAwaitedWriteOfARefusedPayload_WhenAwaited_ThenTheErrorIsThrown)
{
  log_capture log;
  stream_rig<slip_case> rig;
  rig.open ();
  bytes_t const big = slip_case::oversized ();
  std::future<void> result = asio::co_spawn (
      rig.runner.get_executor (),
      [&] () -> asio::awaitable<void>
        {
          co_await rig.link.async_write_frame (tr::const_bytes_t (big),
                                               asio::use_awaitable);
        },
      asio::use_future);
  try
    {
      result.get ();
      FAIL () << "the refused payload was written";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == framing_errc::payload_too_large);
    }
}
