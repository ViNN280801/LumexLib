// A device client the way a real application builds it from lumex::transport
// and lumex::framing: a reconnecting stream_session over TCP, a line codec
// that cuts the byte stream into replies (the framed adapters own a plain
// byte_stream, not a session, so a client that reconnects feeds the codec
// itself), the transport log and the counters. The "device" is a line
// protocol server on loopback, started here: it answers HELLO and READ n,
// sends a damaged (too long) line once, and resets the connection on RESET.
// After a reset the client waits for the session to reconnect and identifies
// itself again: the session reopens the link, it never resends anything.
//
// One thing to read for: every callback of the session runs on its strand,
// the application thread only waits on a condition variable. The client is
// owned by shared pointers, because the pending read of the session holds it.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
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
namespace ss = tr::session;
namespace fr = lumex::core::framing;

namespace
{
using ms = std::chrono::milliseconds;
using tr::error::transport_errc;

void
say (std::string const &line)
{
  static std::mutex mutex;
  std::lock_guard<std::mutex> const lock (mutex);
  std::cout << line << '\n';
}

// The transport log: everything is counted by level, warnings and errors are
// shown as they happen (the sink is a plain function, so its state is global).
std::atomic<std::size_t> &
logged (tr::log::log_level level)
{
  static std::atomic<std::size_t> counts[6];
  return counts[static_cast<std::size_t> (level)];
}

void
count_and_show (tr::log::log_record_t const &record)
{
  ++logged (record.level);
  if (record.level >= tr::log::log_level::warning)
    say (std::string ("  [log ") + record.component + ' ' + record.instance
         + "] " + record.message);
}

// The device: one line in, one line out. A connection ends on RESET.
class line_device
{
public:
  line_device ()
      : acceptor_ (context_, asio::ip::tcp::endpoint (
                                 asio::ip::address_v4::loopback (), 0)),
        stopping_ (false), thread_ ([this] { serve (); })
  {
  }

  ~line_device ()
  {
    stopping_ = true;
    asio::io_context wake_context;
    asio::ip::tcp::socket wake (wake_context);
    asio::error_code ignored;
    wake.connect (
        asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), port ()),
        ignored);
    thread_.join ();
  }

  std::uint16_t
  port () const
  {
    return acceptor_.local_endpoint ().port ();
  }

private:
  static void
  send (asio::ip::tcp::socket &socket, std::string const &text)
  {
    asio::error_code ignored;
    asio::write (socket, asio::buffer (text), ignored);
  }

  void
  serve ()
  {
    while (!stopping_)
      {
        asio::ip::tcp::socket socket (context_);
        asio::error_code error;
        acceptor_.accept (socket, error);
        if (error || stopping_)
          return;
        std::string pending;
        std::vector<char> buffer (64);
        bool reset = false;
        while (!reset && !error)
          {
            std::size_t const bytes
                = socket.read_some (asio::buffer (buffer), error);
            if (error)
              break;
            pending.append (buffer.data (), bytes);
            std::string::size_type end;
            while (!reset && (end = pending.find ('\n')) != std::string::npos)
              {
                std::string const request = pending.substr (0, end);
                pending.erase (0, end + 1);
                if (request == "HELLO")
                  send (socket, "HELLO bench 1\n");
                else if (request.compare (0, 5, "READ ") == 0)
                  send (socket, "VALUE " + request.substr (5) + " 42\n");
                else if (request == "NOISE")
                  // A line the client's codec refuses (longer than its
                  // limit), then the real answer.
                  send (socket, std::string (60, '#') + "\nOK\n");
                else if (request == "RESET")
                  reset = true; // no answer: the socket is closed
              }
          }
      }
  }

  asio::io_context context_;
  asio::ip::tcp::acceptor acceptor_;
  std::atomic<bool> stopping_;
  std::thread thread_;
};

struct reply_t
{
  std::string line;
  std::error_code error;
};

class device_client : public std::enable_shared_from_this<device_client>
{
public:
  device_client (tr::executor_type const &executor, std::uint16_t port)
      : executor_ (executor), port_ (port), link_up_ (false), reading_ (false),
        buffer_ (128), lines_in_ (0), bad_lines_ (0)
  {
    fr::delimiter::delimiter_options_t options;
    options.rx_delimiter = std::vector<std::uint8_t> (1, '\n');
    options.tx_suffix = std::vector<std::uint8_t> (1, '\n');
    options.max_frame_size = 32;
    rx_codec_.reset (new fr::delimiter::delimiter_codec (options));
    tx_codec_.reset (new fr::delimiter::delimiter_codec (options));
  }

  // The session needs the shared pointer of the client for its callbacks, so
  // it is made here and not in the constructor.
  void
  init ()
  {
    std::weak_ptr<device_client> const weak = shared_from_this ();
    ss::session_options_t options;
    options.name = "bench";
    options.policy.initial_delay = ms (50);
    options.policy.jitter = 0.0;
    options.on_state_change = [weak] (ss::state_change_t const &change)
      {
        std::shared_ptr<device_client> const self = weak.lock ();
        if (self)
          self->on_state (change);
      };
    ss::net_selector_t where;
    where.host = "127.0.0.1";
    where.port = port_;
    link_.reset (new ss::stream_session (
        executor_, ss::make_tcp_factory (where), options));
  }

  bool
  connect (ms limit)
  {
    return link_->open (limit).has_value ();
  }

  // Waits for the link to be up again (after a reset).
  bool
  wait_for_link (ms limit)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    return changed_.wait_for (lock, limit, [this] { return link_up_; });
  }

  // One request, one reply line. Fails with timeout when the device is
  // silent, with disconnected when the link goes while waiting.
  reply_t
  ask (std::string const &command, ms timeout)
  {
    reply_t reply;
    fr::codec::bytes_t wire;
    std::vector<std::uint8_t> const payload (command.begin (), command.end ());
    tx_codec_->encode (fr::codec::bytes_view_t (payload), wire);
    std::promise<std::error_code> written;
    link_->async_write_all (tr::const_bytes_t (wire.data (), wire.size ()),
                            [&written] (std::error_code error, std::size_t)
                              { written.set_value (error); });
    reply.error = written.get_future ().get ();
    if (reply.error)
      return reply;
    std::unique_lock<std::mutex> lock (mutex_);
    changed_.wait_for (lock, timeout,
                       [this] { return !lines_.empty () || !link_up_; });
    if (!lines_.empty ())
      {
        reply.line = lines_.front ();
        lines_.pop_front ();
      }
    else
      reply.error = tr::error::make_error_code (
          link_up_ ? transport_errc::timeout : transport_errc::disconnected);
    return reply;
  }

  // Closes the session and waits for it to say so. The pending read ends
  // with cancelled and lets go of the client; the session is destroyed with
  // the client, on whichever thread drops the last reference.
  void
  shutdown ()
  {
    link_->close ();
    std::unique_lock<std::mutex> lock (mutex_);
    changed_.wait_for (lock, ms (2000), [this] { return !link_up_; });
  }

  void
  print_summary () const
  {
    ss::session_statistics_t const session = link_->session_statistics ();
    tr::statistics::statistics_t const transport = link_->statistics ();
    std::lock_guard<std::mutex> const lock (mutex_);
    say ("  session: " + std::to_string (session.attempts) + " attempts, "
         + std::to_string (session.failed_attempts) + " failed, "
         + std::to_string (session.reconnects) + " reconnect(s)");
    say ("  last connection: " + std::to_string (transport.bytes_written)
         + " B out, " + std::to_string (transport.bytes_read) + " B in");
    say ("  lines: " + std::to_string (lines_in_) + " good, "
         + std::to_string (bad_lines_) + " refused by the codec");
  }

  std::uint64_t
  reconnects () const
  {
    return link_->session_statistics ().reconnects;
  }

private:
  // On the session's strand.
  void
  on_state (ss::state_change_t const &change)
  {
    say (std::string ("  state ") + ss::to_string (change.previous) + " -> "
         + ss::to_string (change.current));
    bool const up = change.current == ss::channel_state::open;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      link_up_ = up;
      if (up)
        {
          // A new connection starts a new line: whatever the old one left
          // half way is dropped.
          lines_.clear ();
          rx_codec_->reset ();
        }
    }
    changed_.notify_all ();
    if (up)
      pump ();
  }

  // On the session's strand: one read at a time, re-armed by its handler.
  void
  pump ()
  {
    if (reading_)
      return;
    reading_ = true;
    std::shared_ptr<device_client> const self = shared_from_this ();
    link_->async_read_some (tr::mutable_bytes_t (buffer_),
                            [self] (std::error_code error, std::size_t bytes)
                              { self->on_read (error, bytes); });
  }

  void
  on_read (std::error_code error, std::size_t bytes)
  {
    reading_ = false;
    if (error)
      {
        // A lost link starts the reconnect and a new read begins when the
        // session is open again (on_state); a read that was pending while it
        // already reopened has to begin one itself.
        if (ss::is_link_lost (error) && link_->is_open ())
          pump ();
        return;
      }
    rx_codec_->decode (
        fr::codec::bytes_view_t (buffer_.data (), bytes),
        [this] (fr::codec::bytes_view_t line, std::error_code frame_error)
          {
            {
              std::lock_guard<std::mutex> const lock (mutex_);
              if (frame_error)
                ++bad_lines_;
              else
                {
                  ++lines_in_;
                  lines_.push_back (std::string (line.begin (), line.end ()));
                }
            }
            changed_.notify_all ();
          });
    pump ();
  }

  tr::executor_type executor_;
  std::uint16_t port_;
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  std::deque<std::string> lines_;
  bool link_up_;
  // The strand's own state: the codec of the receive side, the read flag and
  // its buffer.
  std::unique_ptr<fr::delimiter::delimiter_codec> rx_codec_;
  std::unique_ptr<fr::delimiter::delimiter_codec> tx_codec_;
  bool reading_;
  std::vector<std::uint8_t> buffer_;
  std::size_t lines_in_;
  std::size_t bad_lines_;
  // Last: the session goes first, and its callbacks hold weak pointers only.
  std::unique_ptr<ss::stream_session> link_;
};

bool
expect (reply_t const &reply, std::string const &command, char const *want)
{
  say ("  " + command + " -> "
       + (reply.error ? "(" + reply.error.message () + ")" : reply.line));
  return !reply.error && reply.line == want;
}

bool
run (tr::stream::io_runner &runner)
{
  line_device device;
  std::shared_ptr<device_client> const client
      = std::make_shared<device_client> (runner.get_executor (),
                                         device.port ());
  client->init ();

  say ("--- connect");
  if (!client->connect (ms (3000)))
    {
      say ("  the device did not answer");
      return false;
    }
  say ("--- requests");
  bool ok
      = expect (client->ask ("HELLO", ms (1000)), "HELLO", "HELLO bench 1");
  ok = expect (client->ask ("READ 1", ms (1000)), "READ 1", "VALUE 1 42")
       && ok;
  // The device sends a line of 60 bytes where the codec allows 32: the codec
  // refuses it, counts it, and the reply that follows is read normally.
  ok = expect (client->ask ("NOISE", ms (1000)), "NOISE", "OK") && ok;

  say ("--- the device resets the connection");
  reply_t const lost = client->ask ("RESET", ms (2000));
  say ("  RESET -> (" + lost.error.message () + ")");
  ok = lost.error == transport_errc::disconnected && ok;
  if (!client->wait_for_link (ms (5000)))
    {
      say ("  the session did not reopen");
      return false;
    }
  // The device has forgotten the client: the application identifies itself
  // again, as its protocol demands. The session resends nothing.
  ok = expect (client->ask ("HELLO", ms (1000)), "HELLO", "HELLO bench 1")
       && ok;
  ok = expect (client->ask ("READ 2", ms (1000)), "READ 2", "VALUE 2 42")
       && ok;

  say ("--- summary");
  client->print_summary ();
  ok = client->reconnects () == 1 && ok;
  client->shutdown ();
  return ok;
}
} // namespace

int
main ()
{
  tr::log::set_log_sink (&count_and_show);
  tr::log::set_log_level (tr::log::log_level::info);
  bool ok = false;
  {
    tr::stream::io_runner runner (1);
    ok = run (runner);
  }
  say ("  transport log: " + std::to_string (logged (tr::log::log_level::info))
       + " info, " + std::to_string (logged (tr::log::log_level::warning))
       + " warning, " + std::to_string (logged (tr::log::log_level::error))
       + " error record(s)");
  return ok ? 0 : 1;
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
