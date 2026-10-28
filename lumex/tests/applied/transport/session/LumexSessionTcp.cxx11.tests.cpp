// stream_session over TCP on loopback: the peer goes away (its connection
// closes and its listener stops), comes back on the same port a little later,
// and the session reconnects by itself and carries data again.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tr::error::transport_errc;
using tcp = asio::ip::tcp;

// Ends the peer thread also when an assertion leaves the test early: it lets
// the peer finish and connects to it until it does (an accept that still
// waits takes the connection and goes on).
struct peer_guard
{
  std::thread &peer;
  std::promise<void> &finish;
  std::atomic<bool> &done;
  std::uint16_t port;

  ~peer_guard ()
  {
    finish.set_value ();
    clock_type::time_point const end
        = clock_type::now () + std::chrono::seconds (10);
    while (!done.load () && clock_type::now () < end)
      {
        asio::io_context context;
        tcp::socket probe (context);
        asio::error_code ignored;
        probe.connect (tcp::endpoint (asio::ip::address_v4::loopback (), port),
                       ignored);
        std::this_thread::sleep_for (ms (50));
      }
    peer.join ();
  }
};
} // namespace

TEST (
    LumexSessionTcpTest,
    GivenATcpPeerThatRestarts_WhenItIsBackOnItsPort_ThenTheSessionReconnectsAndCarriesDataAgain)
{
  asio::io_context peer_context;
  tcp::acceptor first_listener (
      peer_context, tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::uint16_t const port = first_listener.local_endpoint ().port ();
  std::promise<void> finish;
  std::shared_future<void> const finished = finish.get_future ().share ();

  std::atomic<bool> peer_done (false);
  std::thread peer (
      [&peer_context, &first_listener, port, finished, &peer_done] ()
        {
          asio::error_code ignored;
          {
            tcp::socket first (peer_context);
            first_listener.accept (first);
            // The peer goes away: the connection and the listener close.
            first.shutdown (tcp::socket::shutdown_both, ignored);
            first.close (ignored);
            first_listener.close (ignored);
          }
          std::this_thread::sleep_for (ms (300));
          // The peer is back on the same port.
          tcp::acceptor listener (peer_context);
          listener.open (tcp::v4 ());
          listener.set_option (tcp::acceptor::reuse_address (true));
          listener.bind (
              tcp::endpoint (asio::ip::address_v4::loopback (), port));
          listener.listen ();
          tcp::socket second (peer_context);
          listener.accept (second);
          std::string const hello = "hi";
          asio::write (second, asio::buffer (hello));
          finished.wait_for (std::chrono::seconds (30));
          second.close (ignored);
          peer_done.store (true);
        });
  peer_guard const guard = { peer, finish, peer_done, port };

  {
    tr::stream::io_runner runner (2);
    std::shared_ptr<state_recorder> const recorder
        = std::make_shared<state_recorder> ();
    ss::session_options_t options
        = recorded_options (recorder, ms (20), ms (200));
    options.open_timeout = ms (5000);
    ss::net_selector_t selector;
    selector.host = "127.0.0.1";
    selector.port = port;
    ss::stream_session session (runner.get_executor (),
                                ss::make_tcp_factory (selector), options);
    tr::status_result_t const opened = session.open (ms (5000));
    ASSERT_TRUE (opened.has_value ()) << opened.error ().message ();
    EXPECT_EQ (session.statistics ().opens, 1u);

    std::vector<std::uint8_t> buffer (8);
    outcome<std::size_t> lost;
    session.async_read_some (tr::mutable_bytes_t (buffer),
                             [&lost] (std::error_code error, std::size_t bytes)
                               { lost.set (error, bytes); });
    ASSERT_TRUE (lost.wait (ms (5000)));
    EXPECT_TRUE (lost.error == transport_errc::disconnected)
        << lost.error.message ();

    ASSERT_TRUE (eventually (
        [&session] { return session.session_statistics ().reconnects == 1; },
        ms (20000)));
    EXPECT_TRUE (session.is_open ());
    std::vector<ss::channel_state> const states = recorder->states ();
    ASSERT_GE (states.size (), 4u);
    EXPECT_EQ (states[2], ss::channel_state::reconnecting);
    EXPECT_EQ (states[3], ss::channel_state::open);

    std::size_t received = 0;
    while (received < 2)
      {
        std::size_t const got
            = session
                  .async_read_some (
                      tr::mutable_bytes_t (buffer.data () + received,
                                           buffer.size () - received),
                      asio::use_future)
                  .get ();
        received += got;
      }
    EXPECT_EQ (buffer[0], 'h');
    EXPECT_EQ (buffer[1], 'i');
    session.close ();
  }
}
