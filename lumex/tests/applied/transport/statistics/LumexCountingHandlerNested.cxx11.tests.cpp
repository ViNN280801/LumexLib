// A user wrapper around an already type-erased handler
// (asio::bind_executor (ex, any_completion_handler (asio::bind_allocator (a,
// h))))), destroyed without ever being called with the io_context that holds
// its operation. The wrapper forwards the allocator of the erased handler, a
// view into it: the counting wrapper must not hand that view to its own
// erasure, which would free its storage through it after the inner erasure is
// gone. The allocator's copy constructor reports a copy made from a destroyed
// object; under AddressSanitizer the defect is a heap-use-after-free.
// Regression of the review of WP13 (RT_N2), on byte_stream and
// message_transport over the mocks.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;

std::atomic<int> g_dead_copies (0);

// Copying from an allocator that was already destroyed is the defect.
template <typename T> struct poison_allocator
{
  typedef T value_type;
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

class LumexCountingHandlerNestedTest : public ::testing::Test
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
    LumexCountingHandlerNestedTest,
    GivenAReadWithABoundErasedHandler_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_read_some (
        tr::mutable_bytes_t (buffer_),
        asio::bind_executor (context.get_executor (),
                             tr::io_handler_t (asio::bind_allocator (
                                 poison_allocator<void> (), io_handler ()))));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexCountingHandlerNestedTest,
    GivenAWriteAllWithABoundErasedHandler_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  std::vector<std::uint8_t> const data (3, std::uint8_t (1));
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_write_all (
        tr::const_bytes_t (data),
        asio::bind_executor (context.get_executor (),
                             tr::io_handler_t (asio::bind_allocator (
                                 poison_allocator<void> (), io_handler ()))));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexCountingHandlerNestedTest,
    GivenAnOpenWithABoundErasedHandler_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_open (asio::bind_executor (
        context.get_executor (),
        tr::status_handler_t (asio::bind_allocator (poison_allocator<void> (),
                                                    status_handler ()))));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexCountingHandlerNestedTest,
    GivenAReceiveWithABoundErasedHandler_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_datagram mock
        = tr::testing::make_mock_datagram (context.get_executor (), "d");
    mock.transport.async_receive (asio::bind_executor (
        context.get_executor (),
        tr::datagram::message_handler_t (asio::bind_allocator (
            poison_allocator<void> (), message_handler ()))));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

TEST_F (
    LumexCountingHandlerNestedTest,
    GivenASendWithABoundErasedHandler_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_datagram mock
        = tr::testing::make_mock_datagram (context.get_executor (), "d");
    mock.transport.async_send (
        tr::datagram::message_t (),
        asio::bind_executor (
            context.get_executor (),
            tr::status_handler_t (asio::bind_allocator (
                poison_allocator<void> (), status_handler ()))));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}

// Control: a wrapper whose handler has an allocator of its own (not the view
// of an erased handler) keeps forwarding it, and it stays valid.
TEST_F (
    LumexCountingHandlerNestedTest,
    GivenABoundHandlerWithItsOwnAllocator_WhenTheContextDiesUncalled_ThenNoDeadAllocatorIsRead)
{
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_read_some (
        tr::mutable_bytes_t (buffer_),
        asio::bind_executor (
            context.get_executor (),
            asio::bind_allocator (poison_allocator<void> (), io_handler ())));
  }
  EXPECT_EQ (g_dead_copies.load (), 0);
}
