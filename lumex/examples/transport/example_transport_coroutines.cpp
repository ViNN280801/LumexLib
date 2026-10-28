// C++20 coroutines with lumex::transport (this example is built as C++20): a
// stream_session read with asio::use_awaitable, raced against a timer with
// the awaitable operator ||, bounded with asio::cancel_after, composed with
// an asio::experimental::parallel_group, and a state change of the session
// awaited after the peer drops the connection. The peer is a loopback server
// started here. Where the toolchain has no coroutines (GCC 8, MinGW 8.3 in
// -std=c++2a mode: ASIO_HAS_CO_AWAIT is not defined) the example prints that
// and exits 0, so it builds and runs on every compiler of the library.
//
// Every operation of a transport or a session reports the strand it runs
// on, so the timer of asio::cancel_after and of a race lives there. The
// coroutine itself runs on a strand of its own (asio::make_strand), never on
// the bare executor of an io_runner with several threads: a handler on the
// bare executor could run beside that timer, which Asio does not allow.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

#include <asio/detail/config.hpp>
#if defined(ASIO_HAS_CO_AWAIT)
// The experimental parts of the vendored Asio (a system include directory).
// awaitable_operators.hpp does not compile without coroutines.
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/parallel_group.hpp>
#endif

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

#if defined(ASIO_HAS_CO_AWAIT)
namespace ss = tr::session;

namespace
{
using ms = std::chrono::milliseconds;
using tr::error::transport_errc;
using namespace asio::experimental::awaitable_operators;

// A loopback peer: "PING" is answered with "PONG", "QUIET" is not answered,
// "DROP" closes the connection (the next connection is served again).
class script_peer
{
public:
  script_peer ()
      : acceptor_ (context_, asio::ip::tcp::endpoint (
                                 asio::ip::address_v4::loopback (), 0)),
        stopping_ (false), thread_ ([this] { serve (); })
  {
  }

  ~script_peer ()
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
    while (!stopping_)
      {
        asio::ip::tcp::socket socket (context_);
        asio::error_code error;
        acceptor_.accept (socket, error);
        if (error || stopping_)
          return;
        std::vector<char> buffer (64);
        for (;;)
          {
            std::size_t const bytes
                = socket.read_some (asio::buffer (buffer), error);
            if (error)
              break;
            std::string const request (buffer.data (), bytes);
            if (request == "DROP")
              break;
            if (request == "PING")
              asio::write (socket, asio::buffer (std::string ("PONG")), error);
          }
      }
  }

  asio::io_context context_;
  asio::ip::tcp::acceptor acceptor_;
  std::atomic<bool> stopping_;
  std::thread thread_;
};

std::string
text_of (std::vector<std::uint8_t> const &buffer, std::size_t bytes)
{
  return std::string (buffer.begin (),
                      buffer.begin () + static_cast<std::ptrdiff_t> (bytes));
}

tr::const_bytes_t
view_of (std::string const &text)
{
  return tr::const_bytes_t (
      reinterpret_cast<std::uint8_t const *> (text.data ()), text.size ());
}

asio::awaitable<bool>
scenario (ss::stream_session &link)
{
  // Open: the awaitable form throws std::system_error on failure; as_tuple
  // hands the error over instead, which is what a program that has a
  // reconnect policy of its own usually wants.
  auto [open_error]
      = co_await link.async_open (asio::as_tuple (asio::use_awaitable));
  if (open_error)
    {
      std::cout << "open failed: " << open_error.message () << '\n';
      co_return false;
    }
  std::cout << "  opened, state " << ss::to_string (link.state ()) << '\n';

  std::vector<std::uint8_t> buffer (32);
  std::string const ping = "PING";
  co_await link.async_write_all (view_of (ping), asio::use_awaitable);
  std::size_t bytes = co_await link.async_read_some (
      tr::mutable_bytes_t (buffer), asio::use_awaitable);
  std::cout << "  PING -> '" << text_of (buffer, bytes) << "'\n";

  // A read or a timer, whichever ends first. The loser is cancelled: the
  // read gets a terminal cancellation, which the session passes to its
  // current stream; the session itself stays open.
  std::string const quiet = "QUIET";
  co_await link.async_write_all (view_of (quiet), asio::use_awaitable);
  asio::steady_timer timer (co_await asio::this_coro::executor);
  timer.expires_after (ms (100));
  std::variant<std::size_t, std::monostate> const raced = co_await (
      link.async_read_some (tr::mutable_bytes_t (buffer), asio::use_awaitable)
      || timer.async_wait (asio::use_awaitable));
  std::cout << "  read || timer: "
            << (raced.index () == 0 ? "the read won" : "the timer won")
            << ", state " << ss::to_string (link.state ()) << '\n';

  // The same bound written on the operation: the error comes back as a
  // value, cancelled when the time is up.
  auto [read_error, read_bytes] = co_await link.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::cancel_after (ms (100), asio::as_tuple (asio::use_awaitable)));
  std::cout << "  cancel_after on the read: " << read_error.message ()
            << " (cancelled: "
            << (read_error == transport_errc::cancelled ? "yes" : "no") << ", "
            << read_bytes << " B)\n";

  // A parallel_group runs both and reports which finished first: deferred
  // operations are what it composes. wait_for_one cancels the other.
  timer.expires_after (ms (100));
  auto [order, group_read_error, group_bytes, timer_error]
      = co_await asio::experimental::make_parallel_group (
            link.async_read_some (tr::mutable_bytes_t (buffer),
                                  asio::deferred),
            timer.async_wait (asio::deferred))
            .async_wait (asio::experimental::wait_for_one (),
                         asio::use_awaitable);
  std::cout << "  parallel_group: "
            << (order[0] == 1 ? "the timer" : "the read")
            << " finished first, the read ended with "
            << group_read_error.message () << '\n';
  (void)group_bytes;
  (void)timer_error;

  // The peer drops the connection. A session sees the loss through its own
  // I/O (or through the device presence): the read that is pending when the
  // connection goes ends with disconnected and starts the reconnect. Then
  // wait for the session to come back; a wait from a snapshot misses no
  // change.
  std::string const drop = "DROP";
  ss::state_snapshot_t seen = link.state_snapshot ();
  co_await link.async_write_all (view_of (drop), asio::use_awaitable);
  auto [lost_error, lost_bytes] = co_await link.async_read_some (
      tr::mutable_bytes_t (buffer), asio::as_tuple (asio::use_awaitable));
  std::cout << "  the read on the dropped connection: "
            << lost_error.message () << '\n';
  (void)lost_bytes;
  while (true)
    {
      auto [wait_error, next] = co_await link.async_wait_state_change (
          seen, asio::cancel_after (ms (3000),
                                    asio::as_tuple (asio::use_awaitable)));
      if (wait_error)
        {
          std::cout << "  no state change: " << wait_error.message () << '\n';
          co_return false;
        }
      seen = next;
      std::cout << "  state " << ss::to_string (seen.state) << " (change #"
                << seen.sequence << ")\n";
      if (seen.state == ss::channel_state::open)
        break;
    }
  // Nothing was resent: the session is open again and the program asks anew.
  co_await link.async_write_all (view_of (ping), asio::use_awaitable);
  bytes = co_await link.async_read_some (tr::mutable_bytes_t (buffer),
                                         asio::use_awaitable);
  std::cout << "  after the reconnect PING -> '" << text_of (buffer, bytes)
            << "'\n";
  co_return text_of (buffer, bytes) == "PONG"
      && link.session_statistics ().reconnects == 1;
}
} // namespace

int
main ()
{
  tr::stream::io_runner runner (2);
  script_peer peer;

  ss::session_options_t options;
  options.name = "coroutines";
  options.policy.initial_delay = ms (20);
  options.policy.jitter = 0.0;
  ss::net_selector_t where;
  where.host = "127.0.0.1";
  where.port = peer.port ();
  ss::stream_session link (runner.get_executor (),
                           ss::make_tcp_factory (where), options);

  std::future<bool> result
      = asio::co_spawn (asio::make_strand (runner.get_executor ()),
                        scenario (link), asio::use_future);
  bool ok = false;
  if (result.wait_for (std::chrono::seconds (20)) == std::future_status::ready)
    {
      try
        {
          ok = result.get ();
        }
      catch (std::exception const &error)
        {
          std::cout << "the coroutine ended with an exception: "
                    << error.what () << '\n';
        }
    }
  else
    std::cout << "the coroutine did not finish\n";
  link.close ();
  return ok ? 0 : 1;
}
#else
int
main ()
{
  std::cout << "C++20 coroutines are not available in this build "
               "(ASIO_HAS_CO_AWAIT is not defined): nothing to show\n";
  return 0;
}
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
