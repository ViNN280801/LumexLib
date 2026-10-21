// The asio::use_awaitable token of the transport under C++20, over the mock
// stream and a loopback TCP echo peer (the fixture and its instantiation are
// those of LumexTransportTokens.cxx11.tests.cpp).

#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "LumexTransportTokenFixture.hpp"

using lumex_transport_tokens::LumexTransportTokenTest;
using lumex_transport_tokens::ping;
namespace tr = lumex::applied::transport;

TEST_P (LumexTransportTokenTest,
        GivenUseAwaitable_WhenOpenWriteAndRead_ThenTheReplyArrives)
{
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
}
