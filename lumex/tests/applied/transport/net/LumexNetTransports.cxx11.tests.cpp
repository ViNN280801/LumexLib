// TCP and UDP (lumex/applied/transport/net) on loopback against plain Asio
// peers: connect and echo both ways, a refused connection, the bounded
// connect to an address nobody answers, a host name that does not resolve, a
// close during the open, the peer closing (disconnected), UDP send and
// receive, a datagram cut to max_datagram_size, and a report ID UDP rejects.

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::deadline_t;
using tr::error::transport_errc;
using tr::net::tcp_options_t;
using tr::net::udp_options_t;
typedef asio::ip::tcp tcp;
typedef asio::ip::udp udp;
typedef std::chrono::steady_clock clock_type;

long long
elapsed_ms (clock_type::time_point start)
{
  return std::chrono::duration_cast<std::chrono::milliseconds> (
             clock_type::now () - start)
      .count ();
}

// The warnings of the UDP component, collected by a sink (the sink runs on the
// strand, the test reads after the receive returned).
std::mutex &
udp_warnings_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

std::vector<std::string> &
udp_warnings ()
{
  static std::vector<std::string> warnings;
  return warnings;
}

void
udp_warning_sink (tr::log::log_record_t const &record)
{
  if (record.level != tr::log::log_level::warning
      || std::string (record.component) != "transport.udp")
    return;
  std::lock_guard<std::mutex> const lock (udp_warnings_mutex ());
  udp_warnings ().push_back (record.message);
}

// An address in a block reserved for documentation (TEST-NET-1): a connect
// to it gets no answer, or at once "unreachable" on a host without a route.
char const KSILENT_HOST[] = "192.0.2.1";

// A listening socket on a free loopback port, served by blocking calls on
// the test's own thread.
class tcp_peer
{
public:
  tcp_peer ()
      : acceptor_ (context_,
                   tcp::endpoint (asio::ip::address_v4::loopback (), 0)),
        socket_ (context_)
  {
  }

  std::uint16_t
  port () const
  {
    return acceptor_.local_endpoint ().port ();
  }

  void
  accept ()
  {
    acceptor_.accept (socket_);
  }

  // Reads exactly `count` bytes.
  std::vector<std::uint8_t>
  read (std::size_t count)
  {
    std::vector<std::uint8_t> data (count);
    asio::read (socket_, asio::buffer (data));
    return data;
  }

  void
  write (std::vector<std::uint8_t> const &data)
  {
    asio::write (socket_, asio::buffer (data));
  }

  void
  close ()
  {
    asio::error_code ignored;
    socket_.shutdown (tcp::socket::shutdown_both, ignored);
    socket_.close (ignored);
  }

private:
  asio::io_context context_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
};

tcp_options_t
loopback_tcp (std::uint16_t port)
{
  tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = port;
  return options;
}

std::uint16_t
free_tcp_port ()
{
  asio::io_context context;
  tcp::acceptor acceptor (
      context, tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  return acceptor.local_endpoint ().port ();
}
} // namespace

TEST (LumexTcpStreamTest,
      GivenAListeningPeer_WhenConnected_ThenBytesEchoBothWays)
{
  tcp_peer peer;
  tr::stream::io_runner runner;
  tcp_options_t options = loopback_tcp (peer.port ());
  options.keep_alive = true;
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);
  EXPECT_EQ (stream.name (), "127.0.0.1:" + std::to_string (peer.port ()));

  std::thread server (
      [&peer]
        {
          peer.accept ();
          peer.write (peer.read (5));
        });
  ASSERT_TRUE (stream.open (deadline_t (5000)).has_value ());
  EXPECT_TRUE (stream.is_open ());
  std::vector<std::uint8_t> const sent = { 'h', 'e', 'l', 'l', 'o' };
  ASSERT_TRUE (stream.write_all (tr::const_bytes_t (sent), deadline_t (5000))
                   .has_value ());
  std::vector<std::uint8_t> received;
  while (received.size () < sent.size ())
    {
      std::vector<std::uint8_t> chunk (16);
      tr::io_result_t const got
          = stream.read_some (tr::mutable_bytes_t (chunk), deadline_t (5000));
      ASSERT_TRUE (got.has_value ()) << got.error ().message ();
      received.insert (received.end (), chunk.begin (),
                       chunk.begin ()
                           + static_cast<std::ptrdiff_t> (got.value ()));
    }
  server.join ();
  EXPECT_EQ (received, sent);
}

TEST (LumexTcpStreamTest,
      GivenNoListener_WhenOpened_ThenARefusalAndNotATimeout)
{
  tr::stream::io_runner runner;
  tr::stream::byte_stream stream = tr::net::make_tcp_stream (
      runner.get_executor (), loopback_tcp (free_tcp_port ()));
  // Windows retries a refused loopback connect for about two seconds.
  tr::status_result_t const result = stream.open (deadline_t (10000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_FALSE (result.error () == transport_errc::timeout);
  EXPECT_TRUE (result.error () == std::errc::connection_refused)
      << result.error ().message ();
  EXPECT_FALSE (stream.is_open ());
}

TEST (
    LumexTcpStreamTest,
    GivenAnAddressNobodyAnswers_WhenOpenedWithADeadline_ThenItReturnsByTheDeadline)
{
  tr::stream::io_runner runner;
  tcp_options_t options;
  options.host = KSILENT_HOST;
  options.port = 5000;
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);
  clock_type::time_point const start = clock_type::now ();
  tr::status_result_t const result = stream.open (deadline_t (300));
  ASSERT_FALSE (result.has_value ());
  EXPECT_LT (elapsed_ms (start), 1000);
  EXPECT_FALSE (stream.is_open ());
}

TEST (LumexTcpStreamTest,
      GivenAHostThatDoesNotResolve_WhenOpened_ThenInvalidEndpoint)
{
  tr::stream::io_runner runner;
  tcp_options_t options;
  options.host = "lumex-no-such-host.invalid";
  options.port = 5000;
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);
  tr::status_result_t const result = stream.open (deadline_t (30000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::invalid_endpoint)
      << result.error ().message ();
}

TEST (LumexTcpStreamTest,
      GivenAnOpenInProgress_WhenClosed_ThenItEndsCancelledAtOnce)
{
  tr::stream::io_runner runner;
  tcp_options_t options;
  options.host = KSILENT_HOST;
  options.port = 5000;
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);
  std::shared_ptr<std::promise<std::error_code>> const done
      = std::make_shared<std::promise<std::error_code>> ();
  std::future<std::error_code> result = done->get_future ();
  stream.async_open ([done] (std::error_code error)
                       { done->set_value (error); });
  std::this_thread::sleep_for (std::chrono::milliseconds (50));
  clock_type::time_point const closed_at = clock_type::now ();
  stream.close ();
  ASSERT_EQ (result.wait_for (std::chrono::seconds (5)),
             std::future_status::ready);
  EXPECT_LT (elapsed_ms (closed_at), 200);
  std::error_code const error = result.get ();
  // A host without a route may refuse before the close.
  if (error != std::errc::network_unreachable
      && error != std::errc::host_unreachable)
    EXPECT_TRUE (error == transport_errc::cancelled) << error.message ();
  EXPECT_FALSE (stream.is_open ());
}

TEST (LumexTcpStreamTest,
      GivenAConnection_WhenThePeerCloses_ThenTheReadEndsDisconnected)
{
  tcp_peer peer;
  tr::stream::io_runner runner;
  tr::stream::byte_stream stream = tr::net::make_tcp_stream (
      runner.get_executor (), loopback_tcp (peer.port ()));
  std::thread server ([&peer] { peer.accept (); });
  ASSERT_TRUE (stream.open (deadline_t (5000)).has_value ());
  server.join ();
  peer.close ();
  std::vector<std::uint8_t> buffer (8);
  tr::io_result_t const result
      = stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (5000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::disconnected)
      << result.error ().message ();
}

TEST (LumexTcpStreamTest,
      GivenAPendingRead_WhenAnotherThreadCloses_ThenTheReadEndsWithin200Ms)
{
  tcp_peer peer;
  tr::stream::io_runner runner;
  tr::stream::byte_stream stream = tr::net::make_tcp_stream (
      runner.get_executor (), loopback_tcp (peer.port ()));
  std::thread server ([&peer] { peer.accept (); });
  ASSERT_TRUE (stream.open (deadline_t (5000)).has_value ());
  server.join ();
  clock_type::time_point closed_at;
  std::thread closer (
      [&stream, &closed_at]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (100));
          closed_at = clock_type::now ();
          stream.close ();
        });
  std::vector<std::uint8_t> buffer (8);
  tr::io_result_t const result
      = stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (10000));
  closer.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::cancelled);
  EXPECT_LT (elapsed_ms (closed_at), 200);
  EXPECT_FALSE (stream.is_open ());
}

TEST (LumexUdpSocketTest,
      GivenAPeer_WhenADatagramIsSentAndAnswered_ThenEachArrivesWhole)
{
  asio::io_context context;
  udp::socket peer (context,
                    udp::endpoint (asio::ip::address_v4::loopback (), 0));
  tr::stream::io_runner runner;
  udp_options_t options;
  options.host = "127.0.0.1";
  options.port = peer.local_endpoint ().port ();
  tr::datagram::message_transport socket
      = tr::net::make_udp_socket (runner.get_executor (), options);
  ASSERT_TRUE (socket.open (deadline_t (5000)).has_value ());

  tr::datagram::message_t request;
  request.payload = { 1, 2, 3, 4, 5, 6, 7 };
  ASSERT_TRUE (socket.send (request, deadline_t (5000)).has_value ());
  std::vector<std::uint8_t> got (64);
  udp::endpoint sender;
  std::size_t const length = peer.receive_from (asio::buffer (got), sender);
  got.resize (length);
  EXPECT_EQ (got, request.payload);

  std::vector<std::uint8_t> const reply = { 9, 8, 7 };
  peer.send_to (asio::buffer (reply), sender);
  tr::datagram::message_result_t const answer
      = socket.receive (deadline_t (5000));
  ASSERT_TRUE (answer.has_value ()) << answer.error ().message ();
  EXPECT_EQ (answer.value ().report_id, 0);
  EXPECT_EQ (answer.value ().payload, reply);
}

TEST (
    LumexUdpSocketTest,
    GivenADatagramLongerThanTheBuffer_WhenReceived_ThenItIsCutToMaxDatagramSize)
{
  asio::io_context context;
  udp::socket peer (context,
                    udp::endpoint (asio::ip::address_v4::loopback (), 0));
  tr::stream::io_runner runner;
  udp_options_t options;
  options.host = "127.0.0.1";
  options.port = peer.local_endpoint ().port ();
  options.max_datagram_size = 4;
  tr::datagram::message_transport socket
      = tr::net::make_udp_socket (runner.get_executor (), options);
  ASSERT_TRUE (socket.open (deadline_t (5000)).has_value ());
  tr::datagram::message_t hello;
  hello.payload = { 0 };
  ASSERT_TRUE (socket.send (hello, deadline_t (5000)).has_value ());
  std::vector<std::uint8_t> got (8);
  udp::endpoint sender;
  peer.receive_from (asio::buffer (got), sender);

  {
    std::lock_guard<std::mutex> const lock (udp_warnings_mutex ());
    udp_warnings ().clear ();
  }
  tr::log::set_log_sink (&udp_warning_sink);
  tr::log::set_log_level (tr::log::log_level::warning);

  std::vector<std::uint8_t> const longer = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  peer.send_to (asio::buffer (longer), sender);
  tr::datagram::message_result_t const answer
      = socket.receive (deadline_t (5000));
  tr::log::set_log_sink (nullptr);
  ASSERT_TRUE (answer.has_value ()) << answer.error ().message ();
  EXPECT_EQ (answer.value ().payload,
             std::vector<std::uint8_t> ({ 1, 2, 3, 4 }));
  // The cut is logged on every platform: POSIX cuts silently, Windows reports
  // an error, the transport tells both the same way.
  std::lock_guard<std::mutex> const lock (udp_warnings_mutex ());
  ASSERT_EQ (udp_warnings ().size (), 1U);
  EXPECT_NE (udp_warnings ()[0].find ("cut to 4 B"), std::string::npos)
      << udp_warnings ()[0];
}

// A datagram one byte longer than the buffer is the boundary a receive that
// reads exactly max_datagram_size bytes would take for a fit.
TEST (
    LumexUdpSocketTest,
    GivenADatagramOneByteLongerThanTheBuffer_WhenReceived_ThenItIsCutAndLogged)
{
  asio::io_context context;
  udp::socket peer (context,
                    udp::endpoint (asio::ip::address_v4::loopback (), 0));
  tr::stream::io_runner runner;
  udp_options_t options;
  options.host = "127.0.0.1";
  options.port = peer.local_endpoint ().port ();
  options.max_datagram_size = 4;
  tr::datagram::message_transport socket
      = tr::net::make_udp_socket (runner.get_executor (), options);
  ASSERT_TRUE (socket.open (deadline_t (5000)).has_value ());
  tr::datagram::message_t hello;
  hello.payload = { 0 };
  ASSERT_TRUE (socket.send (hello, deadline_t (5000)).has_value ());
  std::vector<std::uint8_t> got (8);
  udp::endpoint sender;
  peer.receive_from (asio::buffer (got), sender);
  {
    std::lock_guard<std::mutex> const lock (udp_warnings_mutex ());
    udp_warnings ().clear ();
  }
  tr::log::set_log_sink (&udp_warning_sink);
  tr::log::set_log_level (tr::log::log_level::warning);

  peer.send_to (asio::buffer (std::vector<std::uint8_t> ({ 1, 2, 3, 4, 5 })),
                sender);
  tr::datagram::message_result_t const cut
      = socket.receive (deadline_t (5000));
  // A datagram that fits exactly is returned whole and logs nothing.
  peer.send_to (asio::buffer (std::vector<std::uint8_t> ({ 6, 7, 8, 9 })),
                sender);
  tr::datagram::message_result_t const whole
      = socket.receive (deadline_t (5000));
  tr::log::set_log_sink (nullptr);
  ASSERT_TRUE (cut.has_value ()) << cut.error ().message ();
  ASSERT_TRUE (whole.has_value ()) << whole.error ().message ();
  EXPECT_EQ (cut.value ().payload, std::vector<std::uint8_t> ({ 1, 2, 3, 4 }));
  EXPECT_EQ (whole.value ().payload,
             std::vector<std::uint8_t> ({ 6, 7, 8, 9 }));
  std::lock_guard<std::mutex> const lock (udp_warnings_mutex ());
  EXPECT_EQ (udp_warnings ().size (), 1U);
}

TEST (LumexUdpSocketTest, GivenAReportId_WhenSent_ThenInvalidArgument)
{
  asio::io_context context;
  udp::socket peer (context,
                    udp::endpoint (asio::ip::address_v4::loopback (), 0));
  tr::stream::io_runner runner;
  udp_options_t options;
  options.host = "127.0.0.1";
  options.port = peer.local_endpoint ().port ();
  tr::datagram::message_transport socket
      = tr::net::make_udp_socket (runner.get_executor (), options);
  ASSERT_TRUE (socket.open (deadline_t (5000)).has_value ());
  tr::datagram::message_t message;
  message.report_id = 3;
  message.payload = { 1 };
  tr::status_result_t const result = socket.send (message, deadline_t (5000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == std::errc::invalid_argument);
}

TEST (LumexUdpSocketTest,
      GivenAPendingReceive_WhenAnotherThreadCloses_ThenItEndsCancelled)
{
  asio::io_context context;
  udp::socket peer (context,
                    udp::endpoint (asio::ip::address_v4::loopback (), 0));
  tr::stream::io_runner runner;
  udp_options_t options;
  options.host = "127.0.0.1";
  options.port = peer.local_endpoint ().port ();
  tr::datagram::message_transport socket
      = tr::net::make_udp_socket (runner.get_executor (), options);
  ASSERT_TRUE (socket.open (deadline_t (5000)).has_value ());
  clock_type::time_point closed_at;
  std::thread closer (
      [&socket, &closed_at]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (100));
          closed_at = clock_type::now ();
          socket.close ();
        });
  tr::datagram::message_result_t const result
      = socket.receive (deadline_t (10000));
  closer.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::cancelled);
  EXPECT_LT (elapsed_ms (closed_at), 200);
}
