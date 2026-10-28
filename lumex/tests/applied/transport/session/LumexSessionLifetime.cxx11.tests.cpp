// The lifetime of a session and of the handlers it stores: a session
// destroyed with a pending async_wait_state_change whose handler has a
// stateful allocator (the context never runs it, or it runs and ends it with
// cancelled), the I/O path with plain and type-erased handlers (the
// use-after-free of WP12 F1 must not come back through the session's
// observer), destruction inside the state callback and inside completion
// handlers, and the release of the implementation after the destruction in
// every state of a cycle.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tr::error::transport_errc;
using state = ss::channel_state;

std::atomic<int> g_dead_copies (0);

// Copying from an allocator that was already destroyed is the defect.
template <typename T> struct poison_allocator
{
  using value_type = T;
  volatile int magic;

  poison_allocator () : magic (0x5a5a) {}
  poison_allocator (poison_allocator const &other) : magic (other.magic)
  {
    check (other.magic);
  }
  template <typename U>
  poison_allocator (poison_allocator<U> const &other) : magic (other.magic)
  {
    check (other.magic);
  }
  ~poison_allocator () { magic = 0xdead; }
  poison_allocator &operator= (poison_allocator const &) = delete;

  static void
  check (int value)
  {
    if (value != 0x5a5a)
      g_dead_copies.fetch_add (1);
  }
  T *
  allocate (std::size_t count)
  {
    return std::allocator<T> ().allocate (count);
  }
  void
  deallocate (T *pointer, std::size_t count)
  {
    std::allocator<T> ().deallocate (pointer, count);
  }
  template <typename U>
  bool
  operator== (poison_allocator<U> const &) const
  {
    return true;
  }
  template <typename U>
  bool
  operator!= (poison_allocator<U> const &) const
  {
    return false;
  }
};

// Counts what goes through the allocator of the user's handler.
struct allocation_counts
{
  std::atomic<int> allocations;
  std::atomic<int> deallocations;
  allocation_counts () : allocations (0), deallocations (0) {}
};

template <typename T> struct counting_allocator
{
  using value_type = T;
  allocation_counts *counts;

  explicit counting_allocator (allocation_counts *where) : counts (where) {}
  template <typename U>
  counting_allocator (counting_allocator<U> const &other)
      : counts (other.counts)
  {
  }
  T *
  allocate (std::size_t count)
  {
    counts->allocations.fetch_add (1);
    return std::allocator<T> ().allocate (count);
  }
  void
  deallocate (T *pointer, std::size_t count)
  {
    counts->deallocations.fetch_add (1);
    std::allocator<T> ().deallocate (pointer, count);
  }
  template <typename U>
  bool
  operator== (counting_allocator<U> const &other) const
  {
    return counts == other.counts;
  }
  template <typename U>
  bool
  operator!= (counting_allocator<U> const &other) const
  {
    return counts != other.counts;
  }
};

struct state_handler
{
  void
  operator() (std::error_code, state)
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

class LumexSessionLifetimeTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    g_dead_copies.store (0);
  }

  std::shared_ptr<stream_factory_state> factory_
      = std::make_shared<stream_factory_state> ();
  std::vector<std::uint8_t> buffer_ = std::vector<std::uint8_t> (4);
};

// A factory that keeps `sentinel` alive while the session implementation
// (which owns the factory) lives.
ss::stream_session::factory_t
sentinel_factory (std::shared_ptr<stream_factory_state> const &state,
                  std::shared_ptr<int> const &sentinel)
{
  ss::stream_session::factory_t const inner = make_stream_factory (state);
  return [inner, sentinel] (tr::executor_type const &executor)
    { return inner (executor); };
}
} // namespace

TEST_F (
    LumexSessionLifetimeTest,
    GivenAPendingStateWait_WhenTheSessionAndTheContextDieUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    ss::stream_session session (context.get_executor (),
                                make_stream_factory (factory_));
    session.async_wait_state_change (
        state::closed,
        asio::bind_allocator (poison_allocator<void> (), state_handler ()));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenAPendingStateWaitStoredOnTheStrand_WhenTheSessionAndTheContextDieUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        context.get_executor (), make_stream_factory (factory_)));
    session->async_wait_state_change (
        state::closed,
        asio::bind_allocator (poison_allocator<void> (), state_handler ()));
    // The wait step runs and stores the handler; the close the destructor
    // posts never runs.
    context.poll ();
    session.reset ();
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenAnErasedHandlerForTheStateWait_WhenTheSessionAndTheContextDieUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    ss::stream_session session (context.get_executor (),
                                make_stream_factory (factory_));
    session.async_wait_state_change (
        state::closed, ss::state_handler_t (asio::bind_allocator (
                           poison_allocator<void> (), state_handler ())));
    context.poll ();
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenAPendingStateWaitWithAnAllocator_WhenTheSessionIsDestroyed_ThenItEndsCancelledAndTheAllocatorBalances)
{
  allocation_counts counts;
  {
    tr::stream::io_runner runner (2);
    // After the runner: the mock streams it keeps die before their context.
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), make_stream_factory (factory)));
    std::promise<std::error_code> done;
    session->async_wait_state_change (
        state::closed,
        asio::bind_allocator (counting_allocator<void> (&counts),
                              [&done] (std::error_code error, state)
                                { done.set_value (error); }));
    session.reset ();
    std::future<std::error_code> ended = done.get_future ();
    ASSERT_EQ (ended.wait_for (std::chrono::seconds (2)),
               std::future_status::ready);
    EXPECT_TRUE (ended.get () == transport_errc::cancelled);
  }
  EXPECT_GT (counts.allocations.load (), 0);
  EXPECT_EQ (counts.allocations.load (), counts.deallocations.load ());
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenQueuedSessionIo_WhenTheSessionAndTheContextDieUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    ss::stream_session session (context.get_executor (),
                                make_stream_factory (factory_));
    session.async_read_some (
        tr::mutable_bytes_t (buffer_),
        asio::bind_allocator (poison_allocator<void> (), io_handler ()));
    session.async_read_some (tr::mutable_bytes_t (buffer_),
                             tr::io_handler_t (asio::bind_allocator (
                                 poison_allocator<void> (), io_handler ())));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenSessionIoOnAClosedSession_WhenItsNotOpenCompletionDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    ss::stream_session session (context.get_executor (),
                                make_stream_factory (factory_));
    session.async_read_some (tr::mutable_bytes_t (buffer_),
                             tr::io_handler_t (asio::bind_allocator (
                                 poison_allocator<void> (), io_handler ())));
    // Runs the step only: it posts the not_open completion, which dies with
    // the context.
    context.run_one ();
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenAnErasedHandlerForSessionIo_WhenTheLinkIsLost_ThenItRunsAndNoDeadAllocatorIsRead)
{
  {
    tr::stream::io_runner runner (2);
    // After the runner: the mock streams it keeps die before their context.
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    ss::stream_session session (runner.get_executor (),
                                make_stream_factory (factory));
    ASSERT_TRUE (session.open (ms (2000)).has_value ());
    std::promise<std::error_code> done;
    session.async_read_some (tr::mutable_bytes_t (buffer_),
                             tr::io_handler_t (asio::bind_allocator (
                                 poison_allocator<void> (),
                                 [&done] (std::error_code error, std::size_t)
                                   { done.set_value (error); })));
    std::this_thread::sleep_for (ms (20));
    factory->last ().disconnect ();
    std::future<std::error_code> ended = done.get_future ();
    ASSERT_EQ (ended.wait_for (std::chrono::seconds (2)),
               std::future_status::ready);
    EXPECT_TRUE (ended.get () == transport_errc::disconnected);
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenPendingSessionIoWithAnErasedHandler_WhenTheRunnerStopsAndEverythingDies_ThenNoDeadAllocatorIsRead)
{
  {
    tr::stream::io_runner runner (2);
    // After the runner: the mock streams it keeps die before their context.
    std::shared_ptr<stream_factory_state> const factory
        = std::make_shared<stream_factory_state> ();
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), make_stream_factory (factory)));
    ASSERT_TRUE (session->open (ms (2000)).has_value ());
    session->async_read_some (tr::mutable_bytes_t (buffer_),
                              tr::io_handler_t (asio::bind_allocator (
                                  poison_allocator<void> (), io_handler ())));
    session->async_read_some (
        tr::mutable_bytes_t (buffer_),
        asio::bind_allocator (poison_allocator<void> (), io_handler ()));
    session->async_wait_state_change (
        state::open,
        asio::bind_allocator (poison_allocator<void> (), state_handler ()));
    std::this_thread::sleep_for (ms (20));
    runner.stop ();
    session.reset ();
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenASession_WhenItIsDestroyedInsideItsOwnStateCallback_ThenNoCallbackFollows)
{
  tr::stream::io_runner runner (2);
  // After the runner: the mock streams it keeps die before their context.
  std::shared_ptr<stream_factory_state> const factory
      = std::make_shared<stream_factory_state> ();
  std::unique_ptr<ss::stream_session> session;
  std::atomic<int> calls_after (0);
  std::atomic<bool> destroyed (false);
  std::promise<void> done;
  ss::session_options_t options;
  options.on_state_change = [&session, &calls_after, &destroyed,
                             &done] (ss::state_change_t const &change)
    {
      if (destroyed.load ())
        {
          calls_after.fetch_add (1);
          return;
        }
      if (change.current == state::open)
        {
          session.reset ();
          destroyed.store (true);
          done.set_value ();
        }
    };
  session.reset (new ss::stream_session (
      runner.get_executor (), make_stream_factory (factory), options));
  session->start ();
  std::future<void> ended = done.get_future ();
  ASSERT_EQ (ended.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  // The close the destructor posted moves the state to closed; the callback
  // of the destroyed session does not hear it.
  std::this_thread::sleep_for (ms (50));
  EXPECT_EQ (calls_after.load (), 0);
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenASession_WhenItIsDestroyedInsideAStateWaitHandler_ThenNothingBreaks)
{
  tr::stream::io_runner runner (2);
  // After the runner: the mock streams it keeps die before their context.
  std::shared_ptr<stream_factory_state> const factory
      = std::make_shared<stream_factory_state> ();
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner.get_executor (), make_stream_factory (factory)));
  std::promise<state> done;
  session->async_wait_state_change (
      state::closed,
      [&session, &done] (std::error_code, state now)
        {
          session.reset ();
          done.set_value (now);
        });
  session->start ();
  std::future<state> ended = done.get_future ();
  ASSERT_EQ (ended.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (ended.get (), state::connecting);
  std::this_thread::sleep_for (ms (30));
}

TEST_F (LumexSessionLifetimeTest,
        GivenASession_WhenItIsDestroyedInsideAReadHandler_ThenNothingBreaks)
{
  tr::stream::io_runner runner (2);
  // After the runner: the mock streams it keeps die before their context.
  std::shared_ptr<stream_factory_state> const factory
      = std::make_shared<stream_factory_state> ();
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner.get_executor (), make_stream_factory (factory)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::promise<std::error_code> done;
  session->async_read_some (
      tr::mutable_bytes_t (buffer_),
      [&session, &done] (std::error_code error, std::size_t)
        {
          session.reset ();
          done.set_value (error);
        });
  factory->last ().disconnect ();
  std::future<std::error_code> ended = done.get_future ();
  ASSERT_EQ (ended.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_TRUE (ended.get () == transport_errc::disconnected);
  std::this_thread::sleep_for (ms (30));
}

TEST_F (
    LumexSessionLifetimeTest,
    GivenASessionInEachStateOfACycle_WhenDestroyed_ThenItsImplementationIsReleased)
{
  tr::stream::io_runner runner (2);
  // After the runner: the mock streams it keeps die before their context.
  std::shared_ptr<stream_factory_state> const factory
      = std::make_shared<stream_factory_state> ();
  // Open, with a pending read and a pending state wait.
  {
    std::shared_ptr<int> sentinel = std::make_shared<int> (1);
    std::weak_ptr<int> const watch = sentinel;
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), sentinel_factory (factory, sentinel)));
    sentinel.reset ();
    ASSERT_TRUE (session->open (ms (2000)).has_value ());
    session->async_read_some (tr::mutable_bytes_t (buffer_), io_handler ());
    session->async_wait_state_change (state::open, state_handler ());
    session.reset ();
    EXPECT_TRUE (eventually ([&watch] { return watch.expired (); })) << "open";
  }
  // Waiting for a retry far away.
  {
    std::shared_ptr<stream_factory_state> const failing
        = std::make_shared<stream_factory_state> ();
    failing->open_failures = 1000;
    std::shared_ptr<int> sentinel = std::make_shared<int> (2);
    std::weak_ptr<int> const watch = sentinel;
    ss::session_options_t options;
    options.policy.initial_delay = ms (60000);
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), sentinel_factory (failing, sentinel),
        options));
    sentinel.reset ();
    session->start ();
    ASSERT_TRUE (eventually (
        [&session]
          { return session->session_statistics ().failed_attempts == 1; }));
    session.reset ();
    EXPECT_TRUE (eventually ([&watch] { return watch.expired (); }))
        << "retry wait";
  }
  // An open that never completes, with the open timeout armed.
  {
    std::shared_ptr<stream_factory_state> const holding
        = std::make_shared<stream_factory_state> ();
    holding->hold_call = 1;
    std::shared_ptr<int> sentinel = std::make_shared<int> (3);
    std::weak_ptr<int> const watch = sentinel;
    ss::session_options_t options;
    options.open_timeout = ms (60000);
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), sentinel_factory (holding, sentinel),
        options));
    sentinel.reset ();
    session->start ();
    ASSERT_TRUE (
        eventually ([&holding] { return holding->call_count () == 1; }));
    session.reset ();
    EXPECT_TRUE (eventually ([&watch] { return watch.expired (); }))
        << "pending open";
  }
  // Parked: the device is away.
  {
    std::shared_ptr<int> sentinel = std::make_shared<int> (4);
    std::weak_ptr<int> const watch = sentinel;
    std::unique_ptr<ss::stream_session> session (new ss::stream_session (
        runner.get_executor (), sentinel_factory (factory, sentinel)));
    sentinel.reset ();
    session->set_device_present (false);
    session->start ();
    ASSERT_TRUE (eventually (
        [&session] { return session->state () == state::connecting; }));
    session.reset ();
    EXPECT_TRUE (eventually ([&watch] { return watch.expired (); }))
        << "parked";
  }
}
