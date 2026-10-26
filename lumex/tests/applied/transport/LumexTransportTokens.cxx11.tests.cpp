// The completion token styles of the transport (through the umbrella
// LumexTransport), each over the mock stream and a loopback TCP echo peer:
// callbacks, asio::use_future, and asio::yield_context (with
// LUMEX_TRANSPORT_WITH_SPAWN, skipped without it). The C++20 suite adds
// asio::use_awaitable (LumexTransportAwaitable.cxx20.tests.cpp).

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "LumexTransportTokenFixture.hpp"

using lumex_transport_tokens::LumexTransportTokenTest;
using lumex_transport_tokens::ping;
using lumex_transport_tokens::target_kind;
namespace tr = lumex::applied::transport;

INSTANTIATE_TEST_SUITE_P (Targets, LumexTransportTokenTest,
                          ::testing::Values (target_kind::mock,
                                             target_kind::tcp),
                          lumex_transport_tokens::target_name);

TEST_P (LumexTransportTokenTest,
        GivenCallbacks_WhenOpenWriteAndRead_ThenTheReplyArrives)
{
  tr::stream::byte_stream &stream = this->stream ();
  std::vector<std::uint8_t> const request = ping ();
  std::vector<std::uint8_t> reply (request.size ());
  std::size_t received = 0;
  std::promise<std::error_code> done;
  std::function<void ()> read_more;
  read_more = [&] ()
    {
      stream.async_read_some (tr::mutable_bytes_t (reply.data () + received,
                                                   reply.size () - received),
                              [&] (std::error_code error, std::size_t bytes)
                                {
                                  received += bytes;
                                  if (error || received == reply.size ())
                                    done.set_value (error);
                                  else
                                    read_more ();
                                });
    };
  stream.async_open (
      [&] (std::error_code error)
        {
          if (error)
            {
              done.set_value (error);
              return;
            }
          stream.async_write_all (tr::const_bytes_t (request),
                                  [&] (std::error_code written, std::size_t)
                                    {
                                      if (written)
                                        done.set_value (written);
                                      else
                                        read_more ();
                                    });
        });
  std::error_code const error = done.get_future ().get ();
  ASSERT_FALSE (error) << error.message ();
  EXPECT_EQ (reply, request);
}

TEST_P (LumexTransportTokenTest,
        GivenUseFuture_WhenOpenWriteAndRead_ThenTheReplyArrives)
{
  tr::stream::byte_stream &stream = this->stream ();
  std::vector<std::uint8_t> const request = ping ();
  stream.async_open (asio::use_future).get ();
  EXPECT_EQ (
      stream.async_write_all (tr::const_bytes_t (request), asio::use_future)
          .get (),
      request.size ());
  std::vector<std::uint8_t> reply (request.size ());
  std::size_t received = 0;
  while (received < reply.size ())
    received += stream
                    .async_read_some (
                        tr::mutable_bytes_t (reply.data () + received,
                                             reply.size () - received),
                        asio::use_future)
                    .get ();
  EXPECT_EQ (reply, request);
}

TEST_P (LumexTransportTokenTest,
        GivenUseFuture_WhenTheOperationFails_ThenGetThrowsTheCode)
{
  tr::stream::byte_stream &stream = this->stream ();
  std::vector<std::uint8_t> buffer (4);
  try
    {
      stream.async_read_some (tr::mutable_bytes_t (buffer), asio::use_future)
          .get ();
      FAIL () << "a read of a closed stream succeeded";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == tr::error::transport_errc::not_open);
    }
  // The mock's TearDown expects the request: send it.
  stream.async_open (asio::use_future).get ();
  stream.async_write_all (tr::const_bytes_t (ping ()), asio::use_future)
      .get ();
}

TEST_P (LumexTransportTokenTest,
        GivenYieldContext_WhenOpenWriteAndRead_ThenTheReplyArrives)
{
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  tr::stream::byte_stream &stream = this->stream ();
  std::vector<std::uint8_t> const request = ping ();
  std::vector<std::uint8_t> reply (request.size ());
  std::promise<std::error_code> done;
  asio::spawn (
      runner_.get_executor (),
      [&] (asio::yield_context yield)
        {
          std::error_code error;
          stream.async_open (yield[error]);
          if (!error)
            stream.async_write_all (tr::const_bytes_t (request), yield[error]);
          std::size_t received = 0;
          while (!error && received < reply.size ())
            received += stream.async_read_some (
                tr::mutable_bytes_t (reply.data () + received,
                                     reply.size () - received),
                yield[error]);
          done.set_value (error);
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  std::error_code const error = done.get_future ().get ();
  ASSERT_FALSE (error) << error.message ();
  EXPECT_EQ (reply, request);
#else
  if (mock_)
    mock_.reset ();
  GTEST_SKIP () << "LUMEX_TRANSPORT_WITH_SPAWN is off";
#endif
}
