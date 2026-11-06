// Sessions of lumex::transport: a stream_session keeps one TCP connection
// open, notices that the peer dropped it and opens a new one by its
// reconnect_policy_t. Shown here: open with a timeout, the channel_state
// transitions (the on_state_change callback and async_wait_state_change from
// a snapshot), the reconnect after a dropped connection, a factory that
// fails until the "device" appears (attempts, backoff, faulted, start () for
// a new cycle), the counters and close (). Then the session as a byte stream
// under a framed adapter (as_byte_stream (): a half frame of the old
// connection is never joined to the first bytes of the new one) and the
// delays of the reconnect on a virtual clock (no real waiting). The peers are
// loopback servers started here, so the example runs anywhere without
// hardware. A serial session differs only in its factory
// (ss::make_serial_factory): see example_transport_hotplug.cpp.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
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
namespace ss = tr::session;

namespace
{
using ms = std::chrono::milliseconds;

// The state callback runs on the session's strand while main prints too:
// one line at a time.
void
say (std::string const &line)
{
  static std::mutex mutex;
  std::lock_guard<std::mutex> const lock (mutex);
  std::cout << line << '\n';
}

// The transport log of this binary (the sessions log under transport.session)
// goes through the same lock, so lines of both never mix.
void
print_record (tr::log::log_record_t const &record)
{
  say (std::string ("  [log ") + record.component + ' ' + record.instance
       + "] " + record.message);
}

void
print_state_change (ss::state_change_t const &change)
{
  std::ostringstream line;
  line << "  state " << ss::to_string (change.previous) << " -> "
       << ss::to_string (change.current) << " (attempt " << change.attempt
       << ", change #" << change.sequence << ")";
  if (change.error)
    line << ": " << change.error.message ();
  say (line.str ());
}

// A peer on loopback that answers the first request of every connection and
// drops the first connection right after the answer (a device that reset).
class flaky_peer
{
public:
  flaky_peer ()
      : acceptor_ (context_, asio::ip::tcp::endpoint (
                                 asio::ip::address_v4::loopback (), 0)),
        stopping_ (false), thread_ ([this] { serve (); })
  {
  }

  ~flaky_peer ()
  {
    stopping_ = true;
    // A connection of our own wakes the accept () that waits.
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
  void
  serve ()
  {
    for (std::size_t connection = 1; !stopping_; ++connection)
      {
        asio::ip::tcp::socket socket (context_);
        asio::error_code error;
        acceptor_.accept (socket, error);
        if (error || stopping_)
          return;
        std::vector<std::uint8_t> buffer (64);
        socket.read_some (asio::buffer (buffer), error);
        if (error)
          continue;
        std::string const answer = "answer " + std::to_string (connection);
        asio::write (socket, asio::buffer (answer), error);
        if (connection == 1)
          continue; // the socket is destroyed: the connection drops
        // The later connections stay up until the client closes them.
        while (!error)
          socket.read_some (asio::buffer (buffer), error);
      }
  }

  asio::io_context context_;
  asio::ip::tcp::acceptor acceptor_;
  std::atomic<bool> stopping_;
  std::thread thread_;
};

using io_outcome = std::pair<std::error_code, std::size_t>;

// A session read, waited for here; the handler runs on the session strand.
io_outcome
read_once (ss::stream_session &link, std::vector<std::uint8_t> &buffer)
{
  std::promise<io_outcome> done;
  link.async_read_some (tr::mutable_bytes_t (buffer),
                        [&done] (std::error_code error, std::size_t bytes)
                          { done.set_value (io_outcome (error, bytes)); });
  return done.get_future ().get ();
}

io_outcome
write_once (ss::stream_session &link, std::string const &text)
{
  std::vector<std::uint8_t> const bytes (text.begin (), text.end ());
  std::promise<io_outcome> done;
  link.async_write_all (tr::const_bytes_t (bytes),
                        [&done] (std::error_code error, std::size_t written)
                          { done.set_value (io_outcome (error, written)); });
  return done.get_future ().get ();
}

// Waits until the session is in `wanted`. A wait from a snapshot misses no
// change, however fast the session moves between two waits; asio::use_future
// turns the (error, snapshot) completion into a future. On the limit the
// session is closed: a terminal cancellation of a wait would end a pending
// connect anyway (the same as close ()).
bool
wait_for_state (ss::stream_session &link, ss::channel_state wanted, ms limit)
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + limit;
  ss::state_snapshot_t seen = link.state_snapshot ();
  while (seen.state != wanted)
    {
      std::future<ss::state_snapshot_t> next
          = link.async_wait_state_change (seen, asio::use_future);
      if (next.wait_until (end) != std::future_status::ready)
        {
          link.close ();
          return false;
        }
      try
        {
          seen = next.get ();
        }
      catch (std::system_error const &)
        {
          return false; // cancelled: the session was closed
        }
    }
  return true;
}

// Waits until the session counted `wanted` reconnects: each change of the
// state wakes it, from a snapshot, so none is missed (the counter goes up
// just before the state becomes open).
bool
wait_for_reconnects (ss::stream_session &link, std::uint64_t wanted, ms limit)
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + limit;
  for (;;)
    {
      // The snapshot comes first: a change after it wakes the wait below.
      ss::state_snapshot_t const seen = link.state_snapshot ();
      if (link.session_statistics ().reconnects >= wanted)
        break;
      std::future<ss::state_snapshot_t> next
          = link.async_wait_state_change (seen, asio::use_future);
      if (next.wait_until (end) != std::future_status::ready)
        {
          link.close ();
          return false;
        }
      try
        {
          next.get ();
        }
      catch (std::system_error const &)
        {
          return false; // cancelled: the session was closed
        }
    }
  return true;
}

bool
reconnect_after_a_dropped_connection ()
{
  say ("--- 1. open, a dropped connection, the reconnect");
  tr::stream::io_runner runner (1);
  flaky_peer peer;

  ss::session_options_t options;
  options.name = "device";
  options.policy.initial_delay = ms (100); // the wait after a failed attempt
  options.policy.jitter = 0.0;             // the same delays on every run
  options.open_timeout = ms (2000);        // one attempt, not the whole cycle
  options.on_state_change = &print_state_change;

  ss::net_selector_t where;
  where.host = "127.0.0.1";
  where.port = peer.port ();
  ss::stream_session link (runner.get_executor (),
                           ss::make_tcp_factory (where), options);
  say ("  before start: " + std::string (ss::to_string (link.state ())));

  // open () starts the session and waits until it is open. On the deadline
  // it returns timeout, and the session goes on connecting.
  tr::status_result_t const opened = link.open (tr::deadline_t (3000));
  if (!opened.has_value ())
    {
      say ("  open failed: " + opened.error ().message ());
      return false;
    }

  io_outcome const sent = write_once (link, "ping");
  std::vector<std::uint8_t> buffer (64);
  io_outcome const answer = read_once (link, buffer);
  say ("  wrote " + std::to_string (sent.second) + " B, read '"
       + std::string (buffer.begin (),
                      buffer.begin ()
                          + static_cast<std::ptrdiff_t> (answer.second))
       + "'");

  // The peer drops the connection now: the next read ends with
  // disconnected, and the session is already reconnecting when this handler
  // runs.
  ss::state_snapshot_t const before = link.state_snapshot ();
  io_outcome const lost = read_once (link, buffer);
  say ("  next read: " + lost.first.message ());
  say ("  link lost seen by is_link_lost: "
       + std::string (ss::is_link_lost (lost.first) ? "yes" : "no")
       + ", state now " + ss::to_string (link.state ()));

  // Nothing is resent and nothing waits for the link: an operation started
  // while the session is not open ends with not_open at once.
  io_outcome const early = write_once (link, "ping");
  say ("  write while not open: " + early.first.message ());

  std::future<ss::state_snapshot_t> changed
      = link.async_wait_state_change (before, asio::use_future);
  ss::state_snapshot_t const after = changed.get ();
  say ("  first change after the snapshot: "
       + std::string (ss::to_string (after.state)) + ", change #"
       + std::to_string (after.sequence));

  if (!wait_for_state (link, ss::channel_state::open, ms (5000)))
    {
      say ("  the session did not reopen");
      return false;
    }
  write_once (link, "ping");
  io_outcome const again = read_once (link, buffer);
  say ("  after the reconnect: read '"
       + std::string (buffer.begin (),
                      buffer.begin ()
                          + static_cast<std::ptrdiff_t> (again.second))
       + "'");

  ss::session_statistics_t const counters = link.session_statistics ();
  say ("  attempts " + std::to_string (counters.attempts) + ", failed "
       + std::to_string (counters.failed_attempts) + ", reconnects "
       + std::to_string (counters.reconnects));
  // The counters of the transport of the LAST attempt: the second connection.
  tr::statistics::statistics_t const traffic = link.statistics ();
  say ("  last transport: " + std::to_string (traffic.bytes_written)
       + " B out, " + std::to_string (traffic.bytes_read) + " B in");

  // close () is safe from any thread and in any state, and idempotent. It
  // is asynchronous: the state is closed once the session's strand ran it.
  link.close ();
  wait_for_state (link, ss::channel_state::closed, ms (2000));
  say ("  after close: " + std::string (ss::to_string (link.state ())));
  return counters.reconnects == 1 && again.second > 0
         && link.state () == ss::channel_state::closed;
}

// The factory of a device that is not plugged in: it throws, which is a
// failed attempt like a failed open.
class plug_factory
{
public:
  explicit plug_factory (std::shared_ptr<std::atomic<bool>> const &plugged,
                         std::uint16_t port)
      : plugged_ (plugged), port_ (port)
  {
  }

  tr::stream::byte_stream
  operator() (tr::executor_type const &executor) const
  {
    if (!plugged_->load ())
      throw std::runtime_error ("the device is not plugged in");
    tr::net::tcp_options_t options;
    options.host = "127.0.0.1";
    options.port = port_;
    return tr::net::make_tcp_stream (executor, options);
  }

private:
  std::shared_ptr<std::atomic<bool>> plugged_;
  std::uint16_t port_;
};

bool
attempts_backoff_and_faulted ()
{
  say ("--- 2. a device that is absent: attempts, backoff, faulted");
  ss::reconnect_policy_t policy;
  policy.initial_delay = ms (10);
  policy.max_delay = ms (40);
  policy.multiplier = 2.0;
  policy.jitter = 0.0;
  policy.max_attempts = 4;
  // The wait after failed attempt n; 0.5 is the middle of the jitter range,
  // so these are the delays without jitter.
  std::ostringstream delays;
  for (std::uint32_t attempt = 1; attempt <= 5; ++attempt)
    delays << ' ' << ss::reconnect_delay (policy, attempt, 0.5).count ()
           << " ms";
  say ("  waits after failed attempts 1..5:" + delays.str ()
       + " (capped by max_delay)");

  tr::stream::io_runner runner (1);
  flaky_peer peer;
  std::shared_ptr<std::atomic<bool>> const plugged
      = std::make_shared<std::atomic<bool>> (false);

  ss::session_options_t options;
  options.name = "absent";
  options.policy = policy;
  options.on_state_change = &print_state_change;
  ss::stream_session link (runner.get_executor (),
                           plug_factory (plugged, peer.port ()), options);

  // The first cycle ends when max_attempts failed attempts are used up.
  tr::status_result_t const first = link.open (tr::deadline_t (5000));
  say (
      "  open: "
      + (first.has_value () ? std::string ("ok") : first.error ().message ()));
  ss::session_statistics_t counters = link.session_statistics ();
  say ("  state " + std::string (ss::to_string (link.state ())) + ", attempts "
       + std::to_string (counters.attempts) + ", failed "
       + std::to_string (counters.failed_attempts) + ", last error '"
       + link.last_error ().message () + "'");
  if (link.state () != ss::channel_state::faulted)
    return false;

  // faulted waits for the program: start () begins a new cycle. Here the
  // device is plugged in first.
  plugged->store (true);
  link.start ();
  if (!wait_for_state (link, ss::channel_state::open, ms (5000)))
    {
      say ("  the session did not open after start ()");
      return false;
    }
  counters = link.session_statistics ();
  say ("  after start (): " + std::string (ss::to_string (link.state ()))
       + ", attempts " + std::to_string (counters.attempts) + ", failed "
       + std::to_string (counters.failed_attempts));
  return counters.failed_attempts == 4;
}

void
which_errors_end_the_link ()
{
  say ("--- 3. what is_link_lost accepts");
  tr::error::transport_errc const codes[]
      = { tr::error::transport_errc::timeout,
          tr::error::transport_errc::cancelled,
          tr::error::transport_errc::would_deadlock,
          tr::error::transport_errc::report_too_large,
          tr::error::transport_errc::disconnected,
          tr::error::transport_errc::closed };
  for (std::size_t i = 0; i < sizeof codes / sizeof codes[0]; ++i)
    {
      std::error_code const code = tr::error::make_error_code (codes[i]);
      say ("  " + code.message () + ": "
           + (ss::is_link_lost (code) ? "the link is gone, reconnect"
                                      : "ends this operation only"));
    }
  say ("  a system error (connection reset): "
       + std::string (ss::is_link_lost (
                          std::make_error_code (std::errc::connection_reset))
                          ? "the link is gone, reconnect"
                          : "ends this operation only"));
}

#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
namespace fr = lumex::core::framing;

// A peer that answers the first connection with half a line and drops it, and
// every later connection with a whole line.
class half_line_peer
{
public:
  half_line_peer ()
      : acceptor_ (context_, asio::ip::tcp::endpoint (
                                 asio::ip::address_v4::loopback (), 0)),
        stopping_ (false), thread_ ([this] { serve (); })
  {
  }

  ~half_line_peer ()
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
  void
  serve ()
  {
    for (std::size_t connection = 1; !stopping_; ++connection)
      {
        asio::ip::tcp::socket socket (context_);
        asio::error_code error;
        acceptor_.accept (socket, error);
        if (error || stopping_)
          return;
        std::vector<std::uint8_t> buffer (64);
        socket.read_some (asio::buffer (buffer), error);
        if (error)
          continue;
        std::string const answer = connection == 1 ? "ab" : "cd\n";
        asio::write (socket, asio::buffer (answer), error);
        if (connection == 1)
          continue; // the connection drops with half a line sent
        while (!error)
          socket.read_some (asio::buffer (buffer), error);
      }
  }

  asio::io_context context_;
  asio::ip::tcp::acceptor acceptor_;
  std::atomic<bool> stopping_;
  std::thread thread_;
};

// The session is a byte stream too (as_byte_stream () for a stream session,
// as_message_transport () for a message session): the same session, so an
// adapter of lumex::framing, or the frame link of a client, can sit on it and
// survive its reconnects. The bytes of a frame that was half received when
// the connection dropped are forgotten with the connection.
bool
a_framed_adapter_on_a_session ()
{
  say ("--- 4. a framed adapter on a session that reconnects");
  tr::stream::io_runner runner (1);
  half_line_peer peer;
  ss::session_options_t options;
  options.name = "framed";
  options.policy.initial_delay = ms (50);
  options.policy.jitter = 0.0;
  ss::net_selector_t where;
  where.host = "127.0.0.1";
  where.port = peer.port ();
  ss::stream_session link (runner.get_executor (),
                           ss::make_tcp_factory (where), options);
  if (!link.open (tr::deadline_t (3000)).has_value ())
    return false;

  fr::delimiter::delimiter_options_t lines;
  lines.rx_delimiter = std::vector<std::uint8_t> (1, '\n');
  lines.tx_suffix = std::vector<std::uint8_t> (1, '\n');
  // The adapter owns a byte stream: the session's own view of itself. The
  // session stays the owner of the connection; it lives longer than the
  // adapter.
  tr::framed::framed_stream<fr::delimiter::delimiter_codec> frames (
      link.as_byte_stream (), fr::delimiter::delimiter_codec (lines));
  std::vector<std::uint8_t> const go = { 'g', 'o' };

  frames.write_frame (tr::const_bytes_t (go), tr::deadline_t (1000));
  // The peer sent "ab" and dropped the connection: no frame, and the session
  // is on its way to a new one.
  tr::framed::frame_result_t const none
      = frames.read_frame (tr::deadline_t (300));
  say ("  first connection: half a line ('ab'), then the peer drops it: "
       + (none.has_value () ? std::string ("a frame")
                            : "no frame (" + none.error ().message () + ")"));
  bool const reconnected = wait_for_reconnects (link, 1, ms (3000));
  say ("  the session reconnected by itself: "
       + std::to_string (link.session_statistics ().reconnects)
       + " reconnect(s)");

  frames.write_frame (tr::const_bytes_t (go), tr::deadline_t (1000));
  tr::framed::frame_result_t const frame
      = frames.read_frame (tr::deadline_t (2000));
  std::string const text
      = frame.has_value ()
            ? std::string (frame.value ().begin (), frame.value ().end ())
            : frame.error ().message ();
  say ("  second connection: the frame is '" + text
       + "' (not 'abcd': the half frame went with its connection)");
  return reconnected && frame.has_value () && text == "cd";
}
#endif

#if defined(LUMEX_TRANSPORT_WITH_TESTING)
// The delays of a session are the timer source's: with the source of a
// testing::virtual_clock the backoff of the policy takes no real time and the
// attempts happen at exact times.
bool
backoff_on_a_virtual_clock ()
{
  say ("--- 5. the backoff of the reconnect on a virtual clock");
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t mock_options;
  mock_options.timers = clock.source ();
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), mock_options);
  std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();

  // A device that appears at the fourth attempt: the first three throw.
  std::shared_ptr<int> const attempts = std::make_shared<int> (0);
  ss::session_options_t options;
  options.name = "virtual";
  options.policy.initial_delay = ms (100);
  options.policy.max_delay = ms (1000);
  options.policy.jitter = 0.0;
  options.open_timeout = ms (1000);
  options.timers = clock.source ();
  tr::timing::time_point_t const start = clock.now ();
  options.on_state_change = [&clock, start] (ss::state_change_t const &change)
    {
      say ("  at "
           + std::to_string (
               std::chrono::duration_cast<ms> (clock.now () - start).count ())
           + " ms of virtual time: " + ss::to_string (change.previous) + " -> "
           + ss::to_string (change.current) + " (attempt "
           + std::to_string (change.attempt) + ")");
    };
  ss::stream_session link (
      context.get_executor (),
      [impl, attempts] (tr::executor_type const &)
        {
          if (++*attempts < 4)
            throw std::runtime_error ("the device is not there yet");
          return tr::stream::byte_stream (impl);
        },
      options);
  link.start ();
  std::chrono::steady_clock::time_point const real_start
      = std::chrono::steady_clock::now ();
  bool const opened = tr::testing::run_until (
      context, clock, [&link] { return link.is_open (); },
      std::chrono::seconds (60));
  // The attempts waited 100, 200 and 400 ms of the virtual clock, no more.
  long long const virtual_ms
      = std::chrono::duration_cast<ms> (clock.now () - start).count ();
  long long const real_ms = std::chrono::duration_cast<ms> (
                                std::chrono::steady_clock::now () - real_start)
                                .count ();
  say (std::string ("  open: ") + (opened ? "yes" : "no") + ", after "
       + std::to_string (*attempts) + " attempts (waits of 100, 200, 400 ms), "
       + "at " + std::to_string (virtual_ms) + " ms of virtual time, in "
       + std::to_string (real_ms) + " ms of real time");
  link.close ();
  tr::testing::run_until (context, clock, [] { return false; }, ms (0));
  return opened && *attempts == 4 && virtual_ms == 700;
}
#endif

} // namespace

int
main ()
{
  tr::log::set_log_sink (&print_record);
  if (!reconnect_after_a_dropped_connection ())
    return 1;
  if (!attempts_backoff_and_faulted ())
    return 1;
  which_errors_end_the_link ();
#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
  if (!a_framed_adapter_on_a_session ())
    return 1;
#else
  say ("--- 4. skipped: built without the framing module");
#endif
#if defined(LUMEX_TRANSPORT_WITH_TESTING)
  if (!backoff_on_a_virtual_clock ())
    return 1;
#else
  say ("--- 5. skipped: built without the testing support");
#endif
  return 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
