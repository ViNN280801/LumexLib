// Frames over a byte stream: a framed_stream with the SLIP codec and with a
// line (delimiter) codec of lumex::framing over a TCP stream that talks to an
// echo peer on loopback (started here, so the example runs anywhere without
// hardware). The same code frames a serial port: only the stream differs. The
// frame counters of the framing layer are printed at the end.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "lumex/applied/transport/LumexTransport"

#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
#include "lumex/core/framing/LumexFraming"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace tr = lumex::applied::transport;

#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
namespace fr = lumex::core::framing;

namespace
{
std::string
hex (std::vector<std::uint8_t> const &bytes)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string text;
  for (std::size_t i = 0; i < bytes.size (); ++i)
    {
      text += digits[bytes[i] >> 4];
      text += digits[bytes[i] & 0x0F];
      text += ' ';
    }
  return text;
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

// Writes every payload as a frame and reads the frames back (the peer echoes
// the bytes, so the frames come back unchanged); false on any failure.
template <typename Codec>
bool
exchange (tr::framed::framed_stream<Codec> &link,
          std::vector<std::vector<std::uint8_t>> const &payloads)
{
  for (std::size_t i = 0; i < payloads.size (); ++i)
    {
      tr::status_result_t const written = link.write_frame (
          tr::const_bytes_t (payloads[i]), tr::deadline_t (1000));
      if (!written.has_value ())
        {
          std::cout << "write failed: " << written.error ().message () << '\n';
          return false;
        }
    }
  for (std::size_t i = 0; i < payloads.size (); ++i)
    {
      tr::framed::frame_result_t const frame
          = link.read_frame (tr::deadline_t (1000));
      if (!frame.has_value ())
        {
          // A deadline ends with transport_errc::timeout; the link stays open.
          std::cout << "read failed: " << frame.error ().message () << '\n';
          return false;
        }
      std::cout << "  frame " << i << " (" << frame.value ().size ()
                << " B): " << hex (frame.value ())
                << (frame.value () == payloads[i] ? "(as written)"
                                                  : "(DIFFERS)")
                << '\n';
      if (frame.value () != payloads[i])
        return false;
    }
  tr::framed::framed_statistics_t const stats = link.statistics ();
  std::cout << "  frames out " << stats.frames_out << ", frames in "
            << stats.frames_in << ", framing errors " << stats.total_errors ()
            << ", dropped " << stats.dropped_frames << '\n';
  return true;
}

template <typename Codec>
bool
over_loopback (tr::stream::io_runner &runner, Codec codec,
               std::vector<std::vector<std::uint8_t>> const &payloads)
{
  asio::io_context peer_context;
  asio::ip::tcp::acceptor acceptor (
      peer_context,
      asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::thread peer ([&acceptor] { echo_one_connection (acceptor); });

  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  bool ok = false;
  {
    tr::framed::framed_stream<Codec> link (
        tr::net::make_tcp_stream (runner.get_executor (), options),
        std::move (codec));
    tr::status_result_t const opened
        = link.transport ().open (tr::deadline_t (3000));
    if (opened.has_value ())
      ok = exchange (link, payloads);
    else
      std::cout << "open failed: " << opened.error ().message () << '\n';
    // The pending read of the decoder ends with the close.
    link.transport ().close ();
  }
  // The peer may still wait in accept () (the open failed): a connection of
  // our own wakes it, whatever happened. One that was accepted is closed by
  // the link above, so the join cannot hang.
  {
    asio::io_context wake_context;
    asio::ip::tcp::socket wake (wake_context);
    asio::error_code ignored;
    wake.connect (asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (),
                                           options.port),
                  ignored);
  }
  peer.join ();
  return ok;
}
} // namespace

int
main ()
{
  // One thread runs every transport; the synchronous calls wait on this one.
  tr::stream::io_runner runner (1);

  std::cout
      << "--- SLIP: END (0xC0) and ESC (0xDB) in a payload are escaped\n";
  std::vector<std::vector<std::uint8_t>> binary;
  binary.push_back (
      std::vector<std::uint8_t> ({ 0x01, 0xC0, 0x02, 0xDB, 0x03 }));
  binary.push_back (std::vector<std::uint8_t> ({ 0x7E }));
  binary.push_back (std::vector<std::uint8_t> (24, 0xAA));
  if (!over_loopback (runner, fr::slip::slip_codec (), binary))
    return 1;

  std::cout << "--- Lines: LF ends a frame in both directions\n";
  fr::delimiter::delimiter_options_t line_options;
  line_options.rx_delimiter = { '\n' };
  line_options.tx_suffix = { '\n' };
  std::vector<std::vector<std::uint8_t>> lines;
  std::string const first = "VERSION?";
  std::string const second = "STATUS?";
  lines.push_back (std::vector<std::uint8_t> (first.begin (), first.end ()));
  lines.push_back (std::vector<std::uint8_t> (second.begin (), second.end ()));
  if (!over_loopback (runner, fr::delimiter::delimiter_codec (line_options),
                      lines))
    return 1;
  return 0;
}
#else
int
main ()
{
  std::cout << "built without the framing module (LUMEX_BUILD_FRAMING): "
               "nothing to show\n";
  return 0;
}
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
