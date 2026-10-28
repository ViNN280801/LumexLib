// Message transports of lumex::transport: UDP as a datagram::message_transport
// (one receive returns one datagram, one send writes one), a message_session
// that reconnects it, and framed_datagram<Codec>, which applies a framing
// codec to every datagram on its own. The peer is a UDP socket on loopback
// started here, so the example runs anywhere without hardware. A HID device is
// the same interface (hid::make_hid_device), with the report ID in
// message_t::report_id.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
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
namespace dg = tr::datagram;
namespace ss = tr::session;

namespace
{
using ms = std::chrono::milliseconds;

void
print_record (tr::log::log_record_t const &record)
{
  std::cout << "  [log " << record.component << ' ' << record.instance << "] "
            << record.message << '\n';
}

dg::message_t
message_of (std::string const &text)
{
  dg::message_t message;
  message.payload.assign (text.begin (), text.end ());
  return message;
}

std::string
text_of (dg::message_t const &message)
{
  return std::string (message.payload.begin (), message.payload.end ());
}

// A UDP peer on loopback with a script: for each datagram it receives, the
// handler returns the datagrams to send back to the sender. "stop" ends it.
class udp_peer
{
public:
  using script_t
      = std::function<std::vector<std::string> (std::string const &)>;

  explicit udp_peer (script_t script)
      : socket_ (context_, asio::ip::udp::endpoint (
                               asio::ip::address_v4::loopback (), 0)),
        script_ (std::move (script)), thread_ ([this] { serve (); })
  {
  }

  ~udp_peer ()
  {
    asio::ip::udp::socket stopper (context_, asio::ip::udp::v4 ());
    asio::error_code ignored;
    std::string const stop = "stop";
    stopper.send_to (asio::buffer (stop), socket_.local_endpoint (), 0,
                     ignored);
    thread_.join ();
  }

  std::uint16_t
  port () const
  {
    return socket_.local_endpoint ().port ();
  }

private:
  void
  serve ()
  {
    std::vector<char> buffer (2048);
    for (;;)
      {
        asio::ip::udp::endpoint sender;
        asio::error_code error;
        std::size_t const bytes
            = socket_.receive_from (asio::buffer (buffer), sender, 0, error);
        if (error)
          return;
        std::string const request (buffer.data (), bytes);
        if (request == "stop")
          return;
        std::vector<std::string> const replies = script_ (request);
        for (std::size_t i = 0; i < replies.size (); ++i)
          socket_.send_to (asio::buffer (replies[i]), sender, 0, error);
      }
  }

  asio::io_context context_;
  asio::ip::udp::socket socket_;
  script_t script_;
  std::thread thread_;
};

std::vector<std::string>
device_script (std::string const &request)
{
  std::vector<std::string> replies;
  if (request == "hello")
    replies.push_back ("echo hello");
  else if (request == "burst")
    {
      replies.push_back ("one");
      replies.push_back ("two");
      replies.push_back ("three");
    }
  else if (request == "long")
    replies.push_back (std::string (20, 'x'));
  else if (request == "frames")
    {
      // Two lines in one datagram; a datagram that stops before its line
      // ends; one line in a datagram of its own.
      replies.push_back ("AB\nCD\n");
      replies.push_back ("EF");
      replies.push_back ("GH\n");
    }
  else if (request == "PING\n")
    replies.push_back ("PONG\n");
  // "silent" and anything else: no answer.
  return replies;
}

tr::net::udp_options_t
udp_to (udp_peer const &peer)
{
  tr::net::udp_options_t options;
  options.host = "127.0.0.1";
  options.port = peer.port ();
  return options;
}

bool
datagrams_one_by_one (tr::stream::io_runner &runner, udp_peer const &peer)
{
  std::cout << "--- 1. one send is one datagram, one receive returns one\n";
  dg::message_transport udp
      = tr::net::make_udp_socket (runner.get_executor (), udp_to (peer));
  // UDP has no connection: open () resolves the peer and "connects" the
  // socket to it, so it only fails for a bad address.
  if (!udp.open (tr::deadline_t (2000)).has_value ())
    return false;

  udp.send (message_of ("hello"), tr::deadline_t (500));
  dg::message_result_t const echo = udp.receive (tr::deadline_t (1000));
  if (!echo.has_value ())
    return false;
  std::cout << "  '" << text_of (echo.value ()) << "' (report ID "
            << static_cast<int> (echo.value ().report_id)
            << ": UDP carries none)\n";

  udp.send (message_of ("burst"), tr::deadline_t (500));
  for (int i = 0; i < 3; ++i)
    {
      dg::message_result_t const part = udp.receive (tr::deadline_t (1000));
      if (!part.has_value ())
        return false;
      std::cout << "  datagram " << i << ": '" << text_of (part.value ())
                << "'\n";
    }

  // A silent peer: the deadline ends the call with timeout, the socket stays
  // open for the next receive.
  udp.send (message_of ("silent"), tr::deadline_t (500));
  dg::message_result_t const none = udp.receive (tr::deadline_t (100));
  std::cout << "  no answer: " << none.error ().message ()
            << ", still open: " << (udp.is_open () ? "yes" : "no") << '\n';

  // The asynchronous API takes any token: use_future gives a std::future.
  udp.send (message_of ("hello"), tr::deadline_t (500));
  std::future<dg::message_t> pending = udp.async_receive (asio::use_future);
  std::cout << "  async_receive with use_future: '" << text_of (pending.get ())
            << "'\n";

  tr::statistics::statistics_t const stats = udp.statistics ();
  std::cout << "  statistics: " << stats.messages_sent << " sent, "
            << stats.messages_received << " received, " << stats.bytes_written
            << " B out, " << stats.bytes_read << " B in, " << stats.timeouts
            << " timeout(s)\n";
  return stats.messages_sent == 4 && stats.messages_received == 5
         && stats.timeouts == 1;
}

bool
limits_of_a_datagram (tr::stream::io_runner &runner, udp_peer const &peer)
{
  std::cout << "--- 2. what the transport refuses or cuts\n";
  tr::net::udp_options_t options = udp_to (peer);
  options.max_datagram_size = 8; // the receive buffer: the longest datagram
  dg::message_transport udp
      = tr::net::make_udp_socket (runner.get_executor (), options);
  if (!udp.open (tr::deadline_t (2000)).has_value ())
    return false;

  // A datagram longer than the buffer is cut to it on every platform, so a
  // program never sees a platform-dependent error here (Windows also logs a
  // warning, POSIX cuts silently: a payload exactly as long as the buffer is
  // the sign).
  udp.send (message_of ("long"), tr::deadline_t (500));
  dg::message_result_t const cut = udp.receive (tr::deadline_t (1000));
  if (!cut.has_value ())
    return false;
  std::cout << "  a 20 B datagram arrives as " << cut.value ().payload.size ()
            << " B\n";

  // UDP numbers no reports: a message with a report ID is a program error.
  dg::message_t numbered = message_of ("hello");
  numbered.report_id = 1;
  tr::status_result_t const refused
      = udp.send (numbered, tr::deadline_t (500));
  std::cout << "  report ID 1: " << refused.error ().message ()
            << " (invalid_argument: "
            << (refused.error () == std::errc::invalid_argument ? "yes" : "no")
            << ")\n";
  return cut.value ().payload.size () == 8
         && refused.error () == std::errc::invalid_argument;
}

bool
a_session_over_udp (tr::stream::io_runner &runner, udp_peer const &peer)
{
  std::cout << "--- 3. message_session: the same reconnecting session\n";
  ss::net_selector_t where;
  where.host = "127.0.0.1";
  where.port = peer.port ();
  ss::session_options_t options;
  options.name = "udp-device";
  ss::message_session link (runner.get_executor (),
                            ss::make_udp_factory (where), options);
  if (!link.open (tr::deadline_t (2000)).has_value ())
    return false;

  std::promise<std::error_code> sent;
  link.async_send (message_of ("hello"), [&sent] (std::error_code error)
                     { sent.set_value (error); });
  if (sent.get_future ().get ())
    return false;
  std::future<dg::message_t> answer = link.async_receive (asio::use_future);
  std::cout << "  '" << text_of (answer.get ()) << "', session state "
            << ss::to_string (link.state ()) << ", transport counted "
            << link.statistics ().messages_received << " received\n";
  link.close ();
  return true;
}

#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
namespace fr = lumex::core::framing;

bool
frames_per_datagram (tr::stream::io_runner &runner, udp_peer const &peer)
{
  std::cout
      << "--- 4. framed_datagram: the codec starts afresh per datagram\n";
  fr::delimiter::delimiter_options_t line_options;
  line_options.rx_delimiter = std::vector<std::uint8_t> (1, '\n');
  line_options.tx_suffix = std::vector<std::uint8_t> (1, '\n');
  tr::framed::framed_datagram<fr::delimiter::delimiter_codec> link (
      tr::net::make_udp_socket (runner.get_executor (), udp_to (peer)),
      fr::delimiter::delimiter_codec (line_options));
  if (!link.transport ().open (tr::deadline_t (2000)).has_value ())
    return false;

  // A frame is delivered as a message: the decoded payload and the report ID
  // of its datagram.
  std::string const ask = "frames";
  link.transport ().send (message_of (ask), tr::deadline_t (500));
  // The peer answers with three datagrams: "AB" and "CD" are two frames of
  // the first, "EF" is a partial line that ends with its datagram (counted
  // as truncated; the next datagram does not complete it), "GH" is the third
  // frame.
  for (int i = 0; i < 3; ++i)
    {
      tr::datagram::message_result_t const frame
          = link.read_frame (tr::deadline_t (1000));
      if (!frame.has_value ())
        return false;
      std::cout << "  frame " << i << ": '" << text_of (frame.value ())
                << "'\n";
    }
  std::string const ping = "PING";
  link.write_frame (
      tr::const_bytes_t (reinterpret_cast<std::uint8_t const *> (ping.data ()),
                         ping.size ()),
      tr::deadline_t (500));
  tr::datagram::message_result_t const pong
      = link.read_frame (tr::deadline_t (1000));
  if (!pong.has_value ())
    return false;
  std::cout << "  reply to the framed PING: '" << text_of (pong.value ())
            << "'\n";

  tr::framed::framed_statistics_t const stats = link.statistics ();
  std::cout << "  frames in " << stats.frames_in << ", frames out "
            << stats.frames_out << ", truncated "
            << stats.errors_of (fr::codec::framing_errc::truncated)
            << ", datagrams received "
            << link.transport_statistics ().messages_received << '\n';
  link.close ();
  return stats.frames_in == 4
         && stats.errors_of (fr::codec::framing_errc::truncated) == 1;
}
#endif
} // namespace

int
main ()
{
  tr::log::set_log_sink (&print_record);
  tr::stream::io_runner runner (1);
  udp_peer peer (&device_script);

  if (!datagrams_one_by_one (runner, peer))
    return 1;
  if (!limits_of_a_datagram (runner, peer))
    return 1;
  if (!a_session_over_udp (runner, peer))
    return 1;
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
  if (!frames_per_datagram (runner, peer))
    return 1;
#else
  std::cout << "--- 4. built without the framing module "
               "(LUMEX_BUILD_FRAMING): framed_datagram is not available\n";
#endif
  return 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
