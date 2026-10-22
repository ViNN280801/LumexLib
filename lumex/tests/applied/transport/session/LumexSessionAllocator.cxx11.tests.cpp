// An already type-erased handler that the user wraps once more
// (bind_executor (ex, any_completion_handler (bind_allocator (a, h))))
// before it reaches a session (REVIEW13 RT_N1 to RT_N5): the allocator the
// inner erasure exposes is a view into it, and a wrapper that forwards it
// reads it after the inner erasure is gone when the handler is destroyed
// uncalled. The poison allocator counts copies from a destroyed allocator;
// AddressSanitizer reports the read itself (the session tests' ASan build).

#include <atomic>
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

std::atomic<int> g_dead (0);

template <typename T> struct poison
{
  typedef T value_type;
  volatile int magic;

  poison () : magic (0x5a5a) {}
  poison (poison const &other) : magic (other.magic) { check (other.magic); }
  template <typename U> poison (poison<U> const &other) : magic (other.magic)
  {
    check (other.magic);
  }
  ~poison () { magic = 0xdead; }
  poison &operator= (poison const &) = delete;

  static void
  check (int value)
  {
    if (value != 0x5a5a)
      g_dead.fetch_add (1);
  }

  T *
  allocate (std::size_t n)
  {
    return std::allocator<T> ().allocate (n);
  }

  void
  deallocate (T *p, std::size_t n)
  {
    std::allocator<T> ().deallocate (p, n);
  }

  template <typename U>
  bool
  operator== (poison<U> const &) const
  {
    return true;
  }

  template <typename U>
  bool
  operator!= (poison<U> const &) const
  {
    return false;
  }
};

struct state_handler
{
  void
  operator() (std::error_code, ss::channel_state)
  {
  }
};

struct status_handler
{
  void
  operator() (std::error_code)
  {
  }
};

struct io_handler
{
  void
  operator() (std::error_code, std::size_t)
  {
  }
};
} // namespace

// RT_N1: the session's I/O path (observed_handler around the user's wrapper).
TEST (
    LumexSessionAllocatorTest,
    GivenSessionIoWithABoundErasedHandler_WhenEverythingDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  g_dead.store (0);
  std::vector<std::uint8_t> buffer (4);
  {
    tr::stream::io_runner runner (2);
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), make_stream_factory (factory)));
    ASSERT_TRUE (session->open (ms (2000)).has_value ());
    session->async_read_some (
        tr::mutable_bytes_t (buffer),
        asio::bind_executor (runner.get_executor (),
                             tr::io_handler_t (asio::bind_allocator (
                                 poison<void> (), io_handler ()))));
    std::this_thread::sleep_for (ms (20));
    runner.stop ();
    session.reset ();
  }
  EXPECT_EQ (g_dead.load (), 0);
}

// RT_N2: the same on a plain byte_stream (the counting handler of the base).
TEST (
    LumexSessionAllocatorTest,
    GivenPlainStreamIoWithABoundErasedHandler_WhenEverythingDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  g_dead.store (0);
  std::vector<std::uint8_t> buffer (4);
  {
    asio::io_context context;
    tr::testing::mock_stream made
        = tr::testing::make_mock_stream (context.get_executor (), "plain");
    made.stream.async_open ([] (std::error_code) {});
    context.poll ();
    made.stream.async_read_some (
        tr::mutable_bytes_t (buffer),
        asio::bind_executor (context.get_executor (),
                             tr::io_handler_t (asio::bind_allocator (
                                 poison<void> (), io_handler ()))));
    context.poll ();
  }
  EXPECT_EQ (g_dead.load (), 0);
}

// RT_N3: the session over TCP: the pending read lives in an Asio operation;
// the runner stops, the session dies, the runner dies and destroys it
// uncalled.
TEST (
    LumexSessionAllocatorTest,
    GivenTcpSessionIoWithABoundErasedHandler_WhenEverythingDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  typedef asio::ip::tcp tcp;
  g_dead.store (0);
  std::vector<std::uint8_t> buffer (4);
  asio::io_context peer_context;
  tcp::acceptor acceptor (
      peer_context, tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::uint16_t const port = acceptor.local_endpoint ().port ();
  tcp::socket accepted (peer_context);
  std::thread peer (
      [&acceptor, &accepted] ()
        {
          asio::error_code ignored;
          acceptor.accept (accepted, ignored);
        });
  bool opened = false;
  {
    tr::stream::io_runner runner (2);
    ss::net_selector_t selector;
    selector.host = "127.0.0.1";
    selector.port = port;
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), ss::make_tcp_factory (selector)));
    opened = session->open (ms (5000)).has_value ();
    if (opened)
      {
        session->async_read_some (
            tr::mutable_bytes_t (buffer),
            asio::bind_executor (runner.get_executor (),
                                 tr::io_handler_t (asio::bind_allocator (
                                     poison<void> (), io_handler ()))));
        std::this_thread::sleep_for (ms (50));
      }
    runner.stop ();
    session.reset ();
  }
  if (!opened)
    {
      asio::io_context probe_context;
      tcp::socket probe (probe_context);
      asio::error_code ignored;
      probe.connect (tcp::endpoint (asio::ip::address_v4::loopback (), port),
                     ignored);
    }
  peer.join ();
  ASSERT_TRUE (opened);
  EXPECT_EQ (g_dead.load (), 0);
}

// RT_N4: the session's own erasure of a state wait: the core dies with the
// context and destroys the waiter uncalled.
TEST (
    LumexSessionAllocatorTest,
    GivenAStateWaitWithABoundErasedHandler_WhenEverythingDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  g_dead.store (0);
  {
    tr::stream::io_runner runner (2);
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), make_stream_factory (factory)));
    session->async_wait_state_change (
        ss::channel_state::closed,
        asio::bind_executor (runner.get_executor (),
                             ss::state_handler_t (asio::bind_allocator (
                                 poison<void> (), state_handler ()))));
    std::this_thread::sleep_for (ms (50));
    runner.stop ();
    session.reset ();
  }
  EXPECT_EQ (g_dead.load (), 0);
}

// RT_N5: the same for an open wait, the session waiting for its retry timer.
TEST (
    LumexSessionAllocatorTest,
    GivenAnOpenWaitWithABoundErasedHandler_WhenEverythingDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  g_dead.store (0);
  {
    tr::stream::io_runner runner (2);
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    factory->throws = 1000;
    ss::session_options_t options;
    options.policy.initial_delay = ms (60000);
    options.policy.max_delay = ms (60000);
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), make_stream_factory (factory), options));
    session->async_open (asio::bind_executor (
        runner.get_executor (), tr::status_handler_t (asio::bind_allocator (
                                    poison<void> (), status_handler ()))));
    std::this_thread::sleep_for (ms (50));
    runner.stop ();
    session.reset ();
  }
  EXPECT_EQ (g_dead.load (), 0);
}
