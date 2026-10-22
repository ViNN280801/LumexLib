// A pending operation whose handler keeps its owner alive (the "capture
// self" idiom: the handler holds the owner, the owner holds the transport,
// the transport's implementation holds the handler in a member, as the mocks
// and the open of TCP do) is a reference cycle: once the owner is released
// and the runner stopped, it leaks. It must not do more than leak: the
// destructors of the runner and of the io_context of the handler's executor
// return. Regression of the review of WP12b (H1..H4, H6, H7): outstanding
// work kept on the handler's executor made the destructor of that
// io_context wait forever. H0 and H5 (no executor of the handler's own) are
// the controls. Each scenario runs on a thread of its own and fails when it
// does not end within 5 s (its thread is then left behind); CTest runs each
// test in a process of its own with a timeout. Every scenario waits until its
// operation reached the implementation before it lets go.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;

// True when `scenario` ends within `timeout`; otherwise its thread is left
// behind (the process ends it at exit).
bool
finishes_within (std::function<void ()> const &scenario,
                 std::chrono::seconds timeout)
{
  std::shared_ptr<std::promise<void>> const done
      = std::make_shared<std::promise<void>> ();
  std::future<void> ended = done->get_future ();
  std::thread worker (
      [scenario, done] ()
        {
          scenario ();
          done->set_value ();
        });
  if (ended.wait_for (timeout) == std::future_status::ready)
    {
      worker.join ();
      return true;
    }
  worker.detach ();
  return false;
}

// Waits until the strand of `impl` ran what was posted before: the operation
// then sits in the implementation, not in the queue of the runner. (A step
// still queued would die with the runner's io_context and drop the last
// owner there, and the stream would then post its close to a destroyed
// strand: a defect of the test, not of the transport.)
template <typename Impl>
void
reach_the_device (std::shared_ptr<Impl> const &impl)
{
  std::shared_ptr<std::promise<void>> const reached
      = std::make_shared<std::promise<void>> ();
  std::future<void> done = reached->get_future ();
  asio::post (impl->strand (), [reached] () { reached->set_value (); });
  done.wait ();
}

struct stream_client
{
  tr::testing::mock_stream mock;
  std::vector<std::uint8_t> buffer;
};

struct datagram_client
{
  tr::testing::mock_datagram mock;
};

struct tcp_client
{
  tr::stream::byte_stream stream;
};

enum class binding
{
  none,
  runner_strand,
  other_context
};

// A pending mock read whose handler keeps its owner.
void
mock_read_cycle (binding how)
{
  std::unique_ptr<asio::io_context> other (new asio::io_context);
  std::unique_ptr<tr::stream::io_runner> runner (
      new tr::stream::io_runner (1));
  stream_client made
      = { tr::testing::make_mock_stream (runner->get_executor (), "cycle"),
          std::vector<std::uint8_t> (3) };
  std::shared_ptr<stream_client> client
      = std::make_shared<stream_client> (std::move (made));
  if (!client->mock.stream.open (tr::deadline_t (2000)).has_value ())
    return;
  {
    std::function<void (std::error_code, std::size_t)> const keep_owner
        = [client] (std::error_code, std::size_t) {};
    tr::mutable_bytes_t const buffer (client->buffer);
    if (how == binding::none)
      client->mock.stream.async_read_some (buffer, keep_owner);
    else if (how == binding::runner_strand)
      client->mock.stream.async_read_some (
          buffer,
          asio::bind_executor (asio::make_strand (runner->get_executor ()),
                               keep_owner));
    else
      client->mock.stream.async_read_some (
          buffer, asio::bind_executor (other->get_executor (), keep_owner));
    reach_the_device (client->mock.stream.impl ());
  }
  client.reset ();
  runner->stop ();
  runner.reset ();
  other.reset ();
}

// A pending mock receive whose handler keeps its owner.
void
mock_receive_cycle (binding how)
{
  std::unique_ptr<asio::io_context> other (new asio::io_context);
  std::unique_ptr<tr::stream::io_runner> runner (
      new tr::stream::io_runner (1));
  datagram_client made
      = { tr::testing::make_mock_datagram (runner->get_executor (), "cycle") };
  std::shared_ptr<datagram_client> client
      = std::make_shared<datagram_client> (std::move (made));
  if (!client->mock.transport.open (tr::deadline_t (2000)).has_value ())
    return;
  {
    std::function<void (std::error_code, tr::datagram::message_t)> const
        keep_owner
        = [client] (std::error_code, tr::datagram::message_t) {};
    if (how == binding::none)
      client->mock.transport.async_receive (keep_owner);
    else if (how == binding::runner_strand)
      client->mock.transport.async_receive (asio::bind_executor (
          asio::make_strand (runner->get_executor ()), keep_owner));
    else
      client->mock.transport.async_receive (
          asio::bind_executor (other->get_executor (), keep_owner));
    reach_the_device (client->mock.transport.impl ());
  }
  client.reset ();
  runner->stop ();
  runner.reset ();
  other.reset ();
}

// A pending TCP open (a connect to a non-routable address of TEST-NET-1)
// whose handler keeps its owner: the handler waits in the pending open of
// the implementation.
void
tcp_open_cycle (binding how)
{
  std::unique_ptr<asio::io_context> other (new asio::io_context);
  std::unique_ptr<tr::stream::io_runner> runner (
      new tr::stream::io_runner (1));
  tr::net::tcp_options_t options;
  options.host = "192.0.2.1";
  options.port = 9;
  tcp_client made
      = { tr::net::make_tcp_stream (runner->get_executor (), options) };
  std::shared_ptr<tcp_client> client
      = std::make_shared<tcp_client> (std::move (made));
  {
    std::function<void (std::error_code)> const keep_owner
        = [client] (std::error_code) {};
    if (how == binding::none)
      client->stream.async_open (keep_owner);
    else if (how == binding::runner_strand)
      client->stream.async_open (asio::bind_executor (
          asio::make_strand (runner->get_executor ()), keep_owner));
    else
      client->stream.async_open (
          asio::bind_executor (other->get_executor (), keep_owner));
    reach_the_device (client->stream.impl ());
  }
  std::this_thread::sleep_for (std::chrono::milliseconds (200));
  client.reset ();
  runner->stop ();
  runner.reset ();
  other.reset ();
}

void
expect_it_ends (void (*scenario) (binding), binding how)
{
  EXPECT_TRUE (finishes_within ([scenario, how] () { scenario (how); },
                                std::chrono::seconds (5)))
      << "the destructors did not return within 5 s";
}
} // namespace

TEST (
    LumexTransportHandlerCycle,
    H0_GivenAMockReadWithoutExecutorKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&mock_read_cycle, binding::none);
}

TEST (
    LumexTransportHandlerCycle,
    H1_GivenAMockReadOnARunnerStrandKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&mock_read_cycle, binding::runner_strand);
}

TEST (
    LumexTransportHandlerCycle,
    H2_GivenAMockReadOnAnotherContextKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&mock_read_cycle, binding::other_context);
}

TEST (
    LumexTransportHandlerCycle,
    H3_GivenAMockReceiveOnARunnerStrandKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&mock_receive_cycle, binding::runner_strand);
}

TEST (
    LumexTransportHandlerCycle,
    H4_GivenAMockReceiveOnAnotherContextKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&mock_receive_cycle, binding::other_context);
}

TEST (
    LumexTransportHandlerCycle,
    H5_GivenATcpOpenWithoutExecutorKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&tcp_open_cycle, binding::none);
}

TEST (
    LumexTransportHandlerCycle,
    H6_GivenATcpOpenOnARunnerStrandKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&tcp_open_cycle, binding::runner_strand);
}

TEST (
    LumexTransportHandlerCycle,
    H7_GivenATcpOpenOnAnotherContextKeepingItsOwner_WhenAllIsDestroyed_ThenTheDestructorsReturn)
{
  expect_it_ends (&tcp_open_cycle, binding::other_context);
}
