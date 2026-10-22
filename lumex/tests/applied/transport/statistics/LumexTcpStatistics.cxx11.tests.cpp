// byte_stream::statistics () over a real TCP connection on loopback: the same
// counters as over the mock move (the counting is in the base class), for an
// open, a write_all, a read, a deadline and the peer closing.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::deadline_t;
using tr::error::transport_errc;
using tr::statistics::statistics_t;

typedef asio::ip::tcp tcp;
typedef std::chrono::system_clock clock_type;
} // namespace

TEST (LumexTcpStatisticsTest,
      GivenAnEchoPeer_WhenTheStreamWorks_ThenEveryCounterMovesAsOverTheMock)
{
  asio::io_context peer_context;
  tcp::acceptor acceptor (
      peer_context, tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  tcp::socket peer (peer_context);
  std::thread server (
      [&acceptor, &peer]
        {
          acceptor.accept (peer);
          std::vector<std::uint8_t> data (5);
          asio::read (peer, asio::buffer (data));
          asio::write (peer, asio::buffer (data));
          // Hold the connection until the client has timed out once, then
          // close it.
          std::this_thread::sleep_for (std::chrono::milliseconds (400));
          asio::error_code ignored;
          peer.shutdown (tcp::socket::shutdown_both, ignored);
          peer.close (ignored);
        });

  tr::stream::io_runner runner;
  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);

  clock_type::time_point const before = clock_type::now ();
  ASSERT_TRUE (stream.open (deadline_t (5000)).has_value ());
  statistics_t s = stream.statistics ();
  EXPECT_EQ (s.opens, 1u);
  EXPECT_FALSE (s.last_activity < before);

  std::vector<std::uint8_t> const sent = { 'h', 'e', 'l', 'l', 'o' };
  ASSERT_TRUE (stream.write_all (tr::const_bytes_t (sent), deadline_t (5000))
                   .has_value ());
  std::size_t received = 0;
  std::uint64_t reads = 0;
  while (received < sent.size ())
    {
      std::vector<std::uint8_t> chunk (16);
      tr::io_result_t const got
          = stream.read_some (tr::mutable_bytes_t (chunk), deadline_t (5000));
      ASSERT_TRUE (got.has_value ()) << got.error ().message ();
      received += got.value ();
      ++reads;
    }
  s = stream.statistics ();
  EXPECT_EQ (s.writes, 1u);
  EXPECT_EQ (s.bytes_written, 5u);
  EXPECT_EQ (s.reads, reads);
  EXPECT_EQ (s.bytes_read, 5u);
  EXPECT_EQ (s.total_errors (), 0u);

  // The peer stays silent: the deadline passes.
  std::vector<std::uint8_t> chunk (16);
  tr::io_result_t const silent
      = stream.read_some (tr::mutable_bytes_t (chunk), deadline_t (50));
  ASSERT_FALSE (silent.has_value ());
  ASSERT_TRUE (silent.error () == transport_errc::timeout);
  s = stream.statistics ();
  EXPECT_EQ (s.timeouts, 1u);
  EXPECT_EQ (s.errors_of (transport_errc::timeout), 1u);
  EXPECT_EQ (s.cancellations, 0u);

  // The peer closes: the read ends with disconnected.
  tr::io_result_t const closed
      = stream.read_some (tr::mutable_bytes_t (chunk), deadline_t (5000));
  server.join ();
  ASSERT_FALSE (closed.has_value ());
  ASSERT_TRUE (closed.error () == transport_errc::disconnected)
      << closed.error ().message ();
  s = stream.statistics ();
  EXPECT_EQ (s.errors_of (transport_errc::disconnected), 1u);
  EXPECT_EQ (s.total_errors (), 2u);
  EXPECT_EQ (s.reads, reads);

  stream.reset_statistics ();
  s = stream.statistics ();
  EXPECT_EQ (s.opens, 0u);
  EXPECT_EQ (s.bytes_read, 0u);
  EXPECT_EQ (s.total_errors (), 0u);
}
