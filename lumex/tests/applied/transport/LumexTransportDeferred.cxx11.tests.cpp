// The asio::deferred token on every asynchronous operation of byte_stream and
// message_transport (over the mocks): the deferred operation compiles, and
// launched later with a callback it completes with the result. Regression of
// the review of WP13 (TK2b): the trailing return types named the
// implementation as an rvalue while the bodies pass the member, an lvalue, and
// deferred keeps the argument types in its own (C2440).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::datagram::message_t;

std::vector<std::uint8_t>
three ()
{
  return std::vector<std::uint8_t> (3, std::uint8_t (0x2A));
}

// The result of a launched operation, for the test thread.
struct io_result
{
  std::error_code error;
  std::size_t bytes;
};

struct status_callback
{
  std::shared_ptr<std::promise<std::error_code>> done;
  void
  operator() (std::error_code error)
  {
    done->set_value (error);
  }
};

struct io_callback
{
  std::shared_ptr<std::promise<io_result>> done;
  void
  operator() (std::error_code error, std::size_t bytes)
  {
    io_result const result = { error, bytes };
    done->set_value (result);
  }
};

struct message_callback
{
  std::shared_ptr<std::promise<message_t>> done;
  void
  operator() (std::error_code error, message_t message)
  {
    if (error)
      message.report_id = 0xEE;
    done->set_value (std::move (message));
  }
};

template <typename T>
bool
ready (std::future<T> &future)
{
  return future.wait_for (std::chrono::seconds (2))
         == std::future_status::ready;
}

// Launches a deferred operation with a status callback; the error.
template <typename Operation>
std::error_code
launch_status (Operation &&operation)
{
  std::shared_ptr<std::promise<std::error_code>> const done
      = std::make_shared<std::promise<std::error_code>> ();
  std::future<std::error_code> result = done->get_future ();
  status_callback const callback = { done };
  std::forward<Operation> (operation) (callback);
  if (!ready (result))
    return make_error_code (std::errc::timed_out);
  return result.get ();
}

// Launches a deferred read or write with a callback; the error and bytes.
template <typename Operation>
io_result
launch_io (Operation &&operation)
{
  std::shared_ptr<std::promise<io_result>> const done
      = std::make_shared<std::promise<io_result>> ();
  std::future<io_result> result = done->get_future ();
  io_callback const callback = { done };
  std::forward<Operation> (operation) (callback);
  if (!ready (result))
    {
      io_result const timed_out
          = { make_error_code (std::errc::timed_out), 0 };
      return timed_out;
    }
  return result.get ();
}

class LumexTransportDeferredTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 1 };
  tr::testing::mock_stream stream_
      = tr::testing::make_mock_stream (runner_.get_executor (), "deferred");
  tr::testing::mock_datagram datagram_
      = tr::testing::make_mock_datagram (runner_.get_executor (), "deferred");
};
} // namespace

TEST_F (LumexTransportDeferredTest,
        GivenADeferredOpen_WhenLaunchedWithACallback_ThenTheStreamOpens)
{
  auto open = stream_.stream.async_open (asio::deferred);
  EXPECT_FALSE (stream_.stream.is_open ());
  EXPECT_FALSE (launch_status (std::move (open)));
  EXPECT_TRUE (stream_.stream.is_open ());
}

TEST_F (LumexTransportDeferredTest,
        GivenADeferredReadSome_WhenLaunchedAfterTheBytesArrive_ThenItReadsThem)
{
  ASSERT_TRUE (stream_.stream.open (tr::deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> buffer (3);
  auto read = stream_.stream.async_read_some (tr::mutable_bytes_t (buffer),
                                              asio::deferred);
  stream_.control.feed (three ());
  io_result const result = launch_io (std::move (read));
  EXPECT_FALSE (result.error);
  EXPECT_EQ (result.bytes, 3u);
  EXPECT_EQ (buffer, three ());
}

TEST_F (LumexTransportDeferredTest,
        GivenADeferredWriteSome_WhenLaunched_ThenTheBytesAreWritten)
{
  ASSERT_TRUE (stream_.stream.open (tr::deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> const data = three ();
  auto write = stream_.stream.async_write_some (tr::const_bytes_t (data),
                                                asio::deferred);
  EXPECT_TRUE (stream_.control.written ().empty ());
  io_result const result = launch_io (std::move (write));
  EXPECT_FALSE (result.error);
  EXPECT_EQ (result.bytes, 3u);
  EXPECT_EQ (stream_.control.written (), data);
}

TEST_F (
    LumexTransportDeferredTest,
    GivenADeferredWriteAll_WhenLaunchedOnAChunkedStream_ThenEveryByteIsWritten)
{
  ASSERT_TRUE (stream_.stream.open (tr::deadline_t (2000)).has_value ());
  stream_.control.set_write_chunk (1);
  std::vector<std::uint8_t> const data = three ();
  auto write = stream_.stream.async_write_all (tr::const_bytes_t (data),
                                               asio::deferred);
  io_result const result = launch_io (std::move (write));
  EXPECT_FALSE (result.error);
  EXPECT_EQ (result.bytes, 3u);
  EXPECT_EQ (stream_.control.written (), data);
}

TEST_F (
    LumexTransportDeferredTest,
    GivenADeferredMessageOpen_WhenLaunchedWithACallback_ThenTheTransportOpens)
{
  auto open = datagram_.transport.async_open (asio::deferred);
  EXPECT_FALSE (datagram_.transport.is_open ());
  EXPECT_FALSE (launch_status (std::move (open)));
  EXPECT_TRUE (datagram_.transport.is_open ());
}

TEST_F (
    LumexTransportDeferredTest,
    GivenADeferredReceive_WhenLaunchedAfterTheMessageArrives_ThenItReceivesIt)
{
  ASSERT_TRUE (datagram_.transport.open (tr::deadline_t (2000)).has_value ());
  auto receive = datagram_.transport.async_receive (asio::deferred);
  message_t message;
  message.report_id = 4;
  message.payload = three ();
  datagram_.control.deliver (message);
  std::shared_ptr<std::promise<message_t>> const done
      = std::make_shared<std::promise<message_t>> ();
  std::future<message_t> result = done->get_future ();
  message_callback const callback = { done };
  std::move (receive) (callback);
  ASSERT_TRUE (ready (result));
  message_t const got = result.get ();
  EXPECT_EQ (got.report_id, 4);
  EXPECT_EQ (got.payload, three ());
}

TEST_F (LumexTransportDeferredTest,
        GivenADeferredSend_WhenLaunched_ThenTheMessageIsSent)
{
  ASSERT_TRUE (datagram_.transport.open (tr::deadline_t (2000)).has_value ());
  message_t message;
  message.report_id = 2;
  message.payload = three ();
  auto send = datagram_.transport.async_send (message, asio::deferred);
  EXPECT_TRUE (datagram_.control.sent ().empty ());
  EXPECT_FALSE (launch_status (std::move (send)));
  std::vector<message_t> const sent = datagram_.control.sent ();
  ASSERT_EQ (sent.size (), 1u);
  EXPECT_EQ (sent[0].report_id, 2);
  EXPECT_EQ (sent[0].payload, three ());
}
