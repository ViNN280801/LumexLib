// The reference cycle of REVIEW13 L1 (RT_O0 to RT_O4): core -> current
// transport -> handler the transport keeps while its operation is pending ->
// core. The documented order is kept (the session destroyed before the
// io_context): the runner is stopped, the session destroyed, then the runner
// destroyed. The factory captures a sentinel, so the sentinel lives while
// the session implementation (which owns the factory) lives.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tcp = asio::ip::tcp;

ss::stream_session::factory_t
watched (ss::stream_session::factory_t inner,
         std::shared_ptr<int> const &sentinel)
{
  return [inner, sentinel] (tr::executor_type const &executor)
    { return inner (executor); };
}

// A loopback peer that accepts one connection and keeps it until destroyed.
struct tcp_peer
{
  asio::io_context context;
  tcp::acceptor acceptor;
  tcp::socket accepted;
  std::thread thread;
  std::uint16_t port;

  tcp_peer ()
      : acceptor (context,
                  tcp::endpoint (asio::ip::address_v4::loopback (), 0)),
        accepted (context), port (acceptor.local_endpoint ().port ())
  {
    thread = std::thread (
        [this] ()
          {
            asio::error_code ignored;
            acceptor.accept (accepted, ignored);
          });
  }

  ~tcp_peer ()
  {
    // Unblocks an accept that still waits (the open failed).
    asio::io_context probe_context;
    tcp::socket probe (probe_context);
    asio::error_code ignored;
    probe.connect (tcp::endpoint (asio::ip::address_v4::loopback (), port),
                   ignored);
    thread.join ();
  }
};
} // namespace

// RT_O0 (control): the runner keeps running; the posted close breaks the
// cycle.
TEST (
    LumexSessionLeakTest,
    GivenAPendingReadOnAMock_WhenTheSessionIsDestroyedWhileTheRunnerRuns_ThenItIsReleased)
{
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  std::vector<std::uint8_t> buffer (4);
  {
    tr::stream::io_runner runner (2);
    // After the runner: the mock streams it keeps die before their context.
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (),
        watched (make_stream_factory (factory), sentinel)));
    sentinel.reset ();
    ASSERT_TRUE (session->open (ms (2000)).has_value ());
    session->async_read_some (tr::mutable_bytes_t (buffer),
                              [] (std::error_code, std::size_t) {});
    std::this_thread::sleep_for (ms (20));
    session.reset ();
    EXPECT_TRUE (eventually ([&watch] { return watch.expired (); }));
  }
  EXPECT_TRUE (watch.expired ());
}

// RT_O1: the read handler stored in the mock implementation, which the core
// owns through its current transport.
TEST (
    LumexSessionLeakTest,
    GivenAPendingReadOnAMock_WhenTheRunnerStopsBeforeTheSessionDies_ThenNothingLeaks)
{
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  std::vector<std::uint8_t> buffer (4);
  {
    tr::stream::io_runner runner (2);
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (),
        watched (make_stream_factory (factory), sentinel)));
    sentinel.reset ();
    ASSERT_TRUE (session->open (ms (2000)).has_value ());
    session->async_read_some (tr::mutable_bytes_t (buffer),
                              [] (std::error_code, std::size_t) {});
    std::this_thread::sleep_for (ms (20));
    runner.stop ();
    session.reset ();
  }
  EXPECT_TRUE (watch.expired ())
      << "the session implementation (and its mock stream) leaked";
}

// RT_O2: a real TCP read pending: the handler lives in an Asio operation,
// which the io_context destroys at shutdown.
TEST (
    LumexSessionLeakTest,
    GivenAPendingTcpRead_WhenTheRunnerStopsBeforeTheSessionDies_ThenNothingLeaks)
{
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  std::vector<std::uint8_t> buffer (4);
  tcp_peer peer;
  {
    tr::stream::io_runner runner (2);
    ss::net_selector_t selector;
    selector.host = "127.0.0.1";
    selector.port = peer.port;
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (),
        watched (ss::make_tcp_factory (selector), sentinel)));
    sentinel.reset ();
    ASSERT_TRUE (session->open (ms (5000)).has_value ());
    session->async_read_some (tr::mutable_bytes_t (buffer),
                              [] (std::error_code, std::size_t) {});
    std::this_thread::sleep_for (ms (20));
    runner.stop ();
    session.reset ();
  }
  EXPECT_TRUE (watch.expired ())
      << "the session implementation (and its TCP stream) leaked";
}

// RT_O3: a real TCP open pending (a connect to TEST-NET-1, 192.0.2.1, that
// does not answer): the TCP implementation keeps the open handler. Skipped
// where that address answers or fails at once.
TEST (
    LumexSessionLeakTest,
    GivenAPendingTcpOpen_WhenTheRunnerStopsBeforeTheSessionDies_ThenNothingLeaks)
{
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  bool connecting = false;
  {
    tr::stream::io_runner runner (2);
    ss::net_selector_t selector;
    selector.host = "192.0.2.1";
    selector.port = 9;
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (),
        watched (ss::make_tcp_factory (selector), sentinel)));
    sentinel.reset ();
    session->start ();
    std::this_thread::sleep_for (ms (200));
    ss::session_statistics_t const stats = session->session_statistics ();
    connecting = session->state () == ss::channel_state::connecting
                 && stats.attempts == 1 && stats.failed_attempts == 0;
    runner.stop ();
    session.reset ();
  }
  if (!connecting)
    GTEST_SKIP () << "the connect to 192.0.2.1 did not stay pending here";
  EXPECT_TRUE (watch.expired ())
      << "the session implementation (and its TCP stream) leaked";
}

// RT_O4: the mock's open pending (hold_next_open).
TEST (
    LumexSessionLeakTest,
    GivenAPendingOpenOnAMock_WhenTheRunnerStopsBeforeTheSessionDies_ThenNothingLeaks)
{
  std::shared_ptr<int> sentinel = std::make_shared<int> (0);
  std::weak_ptr<int> const watch = sentinel;
  {
    tr::stream::io_runner runner (2);
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    factory->hold_call = 1;
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (),
        watched (make_stream_factory (factory), sentinel)));
    sentinel.reset ();
    session->start ();
    ASSERT_TRUE (
        eventually ([&factory] { return factory->call_count () == 1; }));
    std::this_thread::sleep_for (ms (20));
    runner.stop ();
    session.reset ();
  }
  EXPECT_TRUE (watch.expired ())
      << "the session implementation (and its mock stream) leaked";
}
