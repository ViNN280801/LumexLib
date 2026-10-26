// The synchronous API of lumex::transport: an io_runner runs the transports,
// every call has a deadline. A TCP stream talks to an echo peer on loopback
// (started here, so the example runs anywhere); the transport log goes to a
// sink of the program. Pass a port name ("COM3", "/dev/ttyUSB0") to open a
// serial port the same way: only the factory differs.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "lumex/applied/transport/LumexTransport"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace tr = lumex::applied::transport;

namespace
{
void
print_record (tr::log::log_record_t const &record)
{
  std::cout << "  [log " << record.component << ' ' << record.instance << "] "
            << record.message;
  if (!record.bytes.empty ())
    std::cout << ": " << tr::log::format_dump (record.bytes);
  std::cout << '\n';
}

// A peer that echoes one connection, on its own thread with plain Asio.
void
echo_one_connection (asio::ip::tcp::acceptor &acceptor)
{
  asio::ip::tcp::socket socket (acceptor.get_executor ());
  asio::error_code error;
  acceptor.accept (socket, error);
  std::vector<std::uint8_t> buffer (256);
  while (!error)
    {
      std::size_t const bytes
          = socket.read_some (asio::buffer (buffer), error);
      if (!error)
        asio::write (socket, asio::buffer (buffer.data (), bytes), error);
    }
}

void
talk (tr::stream::byte_stream &stream)
{
  tr::status_result_t const opened = stream.open (tr::deadline_t (3000));
  if (!opened.has_value ())
    {
      std::cout << "open " << stream.name ()
                << " failed: " << opened.error ().message () << '\n';
      return;
    }
  std::string const request = "*IDN?\r\n";
  std::vector<std::uint8_t> const bytes (request.begin (), request.end ());
  tr::status_result_t const written
      = stream.write_all (tr::const_bytes_t (bytes), tr::deadline_t (1000));
  if (!written.has_value ())
    {
      std::cout << "write failed: " << written.error ().message () << '\n';
      return;
    }
  std::vector<std::uint8_t> reply (64);
  tr::io_result_t const got
      = stream.read_some (tr::mutable_bytes_t (reply), tr::deadline_t (1000));
  if (!got.has_value ())
    {
      // A deadline ends with transport_errc::timeout; the stream stays open.
      std::cout << "read failed: " << got.error ().message () << '\n';
      return;
    }
  std::cout << "reply of " << got.value () << " B: "
            << tr::log::format_dump (
                   tr::log::log_bytes_t (reply.data (), got.value ()))
            << '\n';
}
} // namespace

int
main (int argc, char **argv)
{
  tr::log::set_log_sink (&print_record);
  tr::log::set_log_level (tr::log::log_level::trace);

  // One thread runs every transport; the synchronous calls wait on this one.
  tr::stream::io_runner runner (1);

  if (argc > 1)
    {
      tr::serial::serial_stream_options_t options;
      options.path = argv[1];
      options.settings.baud_rate = 9600;
      tr::stream::byte_stream serial
          = tr::serial::make_serial_stream (runner.get_executor (), options);
      talk (serial);
      return 0;
    }

  asio::io_context peer_context;
  asio::ip::tcp::acceptor acceptor (
      peer_context,
      asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::thread peer ([&acceptor] { echo_one_connection (acceptor); });

  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  {
    tr::stream::byte_stream tcp
        = tr::net::make_tcp_stream (runner.get_executor (), options);
    talk (tcp);
    tcp.close ();
  }
  peer.join ();
  return 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
