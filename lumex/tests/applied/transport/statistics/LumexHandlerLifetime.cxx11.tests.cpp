// A handler that is destroyed without ever being called (the io_context is
// destroyed with the operation still queued, the shutdown path of an
// io_runner) while it carries a stateful allocator. The counting wrapper must
// not make Asio read that allocator after the user's handler is gone: the
// allocator's copy constructor reports a copy made from a destroyed object.
// Regression of the review of WP12 (F1): a counting wrapper around an already
// type-erased handler forwarded the inner handler's erased allocator.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;

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

struct io_handler
{
  void
  operator() (std::error_code, std::size_t)
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

struct message_handler
{
  void
  operator() (std::error_code, tr::datagram::message_t)
  {
  }
};

class LumexHandlerLifetimeTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    g_dead_copies.store (0);
  }

  std::vector<std::uint8_t> buffer_ = std::vector<std::uint8_t> (4);
};
} // namespace

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAQueuedAsyncRead_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_read_some (
        tr::mutable_bytes_t (buffer_),
        asio::bind_allocator (poison_allocator<void> (), io_handler ()));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAQueuedAsyncOpen_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_open (
        asio::bind_allocator (poison_allocator<void> (), status_handler ()));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAQueuedAsyncWriteSome_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  std::vector<std::uint8_t> const data (3, std::uint8_t (1));
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_write_some (
        tr::const_bytes_t (data),
        asio::bind_allocator (poison_allocator<void> (), io_handler ()));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAQueuedAsyncWriteAll_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  std::vector<std::uint8_t> const data (3, std::uint8_t (1));
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_write_all (
        tr::const_bytes_t (data),
        asio::bind_allocator (poison_allocator<void> (), io_handler ()));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAQueuedAsyncReceive_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_datagram mock
        = tr::testing::make_mock_datagram (context.get_executor (), "d");
    mock.transport.async_receive (
        asio::bind_allocator (poison_allocator<void> (), message_handler ()));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAQueuedAsyncSend_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_datagram mock
        = tr::testing::make_mock_datagram (context.get_executor (), "d");
    mock.transport.async_send (
        tr::datagram::message_t (),
        asio::bind_allocator (poison_allocator<void> (), status_handler ()));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenACompletionQueuedOnAnotherContext_WhenThatContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  // The read of a closed stream completes at once with not_open; its
  // completion goes to the handler's own executor, a context nobody runs.
  asio::io_context stream_context;
  {
    asio::io_context handler_context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (stream_context.get_executor (), "m");
    mock.stream.async_read_some (
        tr::mutable_bytes_t (buffer_),
        asio::bind_executor (
            handler_context.get_executor (),
            asio::bind_allocator (poison_allocator<void> (), io_handler ())));
    stream_context.run ();
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAHandlerWithAnAllocator_WhenItsOperationsComplete_ThenTheStreamUsesThatAllocatorAndReturnsEverything)
{
  allocation_counts counts;
  {
    tr::stream::io_runner runner (2);
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (runner.get_executor (), "m");
    ASSERT_TRUE (mock.stream.open (tr::deadline_t (500)).has_value ());
    mock.control.feed (std::vector<std::uint8_t> (2, std::uint8_t (1)));
    std::promise<void> done;
    mock.stream.async_read_some (
        tr::mutable_bytes_t (buffer_),
        asio::bind_allocator (counting_allocator<void> (&counts),
                              [&done] (std::error_code, std::size_t)
                                { done.set_value (); }));
    ASSERT_EQ (done.get_future ().wait_for (std::chrono::seconds (2)),
               std::future_status::ready);
    mock.stream.close ();
  }
  EXPECT_GT (counts.allocations.load (), 0);
  EXPECT_EQ (counts.allocations.load (), counts.deallocations.load ());
}

// An already type-erased handler is a valid completion token (code that stores
// handlers erased passes them on): Asio hands it through unchanged, so the
// counting wrapper must not forward the erased handler's own allocator either.
TEST_F (
    LumexHandlerLifetimeTest,
    GivenAnErasedHandlerForAsyncRead_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_read_some (
        tr::mutable_bytes_t (buffer_),
        tr::io_handler_t (
            asio::bind_allocator (poison_allocator<void> (), io_handler ())));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAnErasedHandlerForAsyncOpen_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_open (tr::status_handler_t (
        asio::bind_allocator (poison_allocator<void> (), status_handler ())));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexHandlerLifetimeTest,
    GivenAnErasedHandlerForAsyncReceive_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_datagram mock
        = tr::testing::make_mock_datagram (context.get_executor (), "d");
    mock.transport.async_receive (tr::datagram::message_handler_t (
        asio::bind_allocator (poison_allocator<void> (), message_handler ())));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}
