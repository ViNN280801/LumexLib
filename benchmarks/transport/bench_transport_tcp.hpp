// bench_transport_tcp.hpp
// A `client<P>` over a real TCP connection on the loopback interface, for the
// benchmarks that need real sockets and real timers: the round trip of one
// exchange (bench_dialogue.cpp) and the load of many devices
// (bench_sockets.cpp).
//
// The client lives on an `io_runner` and uses the real timers of Asio. The
// peer is a blocking socket of the benchmark (the device's side of the wire):
// the benchmark writes the frames itself, from its own thread, so what the
// client side does with them is the only thing under test.
#ifndef LUMEX_BENCHMARKS_TRANSPORT_TCP_HPP
#define LUMEX_BENCHMARKS_TRANSPORT_TCP_HPP

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include "asio/ip/tcp.hpp"
#include "asio/post.hpp"

#include "bench_transport_rig.hpp"

namespace bench_transport
{
using tcp = asio::ip::tcp;

/// Runs `body` on the thread of `runner` and waits until it has run.
template <typename Body>
void
on_runner (tr::stream::io_runner &runner, Body body)
{
  std::promise<void> done;
  asio::post (runner.context (),
              [&] ()
                {
                  body ();
                  done.set_value ();
                });
  done.get_future ().wait ();
}

/// A listener on the loopback interface; every `connect` makes one client on
/// a runner and the peer socket that is its other end.
class loopback_t
{
public:
  loopback_t ()
      : acceptor_ (context_,
                   tcp::endpoint (asio::ip::address_v4::loopback (), 0))
  {
  }

  loopback_t (loopback_t const &) = delete;
  loopback_t &operator= (loopback_t const &) = delete;

  /// Opens a TCP stream of `runner` to the listener, accepts it, builds the
  /// client on the runner thread and calls `setup (client)` there.
  /// `peer` receives the socket of the other end (no delay, blocking).
  std::unique_ptr<client_type>
  connect (tr::stream::io_runner &runner, std::unique_ptr<tcp::socket> &peer,
           std::function<void (client_type &)> const &setup)
  {
    tr::net::tcp_options_t options;
    options.host = "127.0.0.1";
    options.port = acceptor_.local_endpoint ().port ();
    tr::stream::byte_stream stream
        = tr::net::make_tcp_stream (runner.get_executor (), options);
    std::promise<std::error_code> opened;
    stream.async_open ([&] (std::error_code error)
                         { opened.set_value (error); });
    peer.reset (new tcp::socket (context_));
    acceptor_.accept (*peer);
    peer->set_option (tcp::no_delay (true));
    if (opened.get_future ().get ())
      die ("a TCP stream did not open");
    std::unique_ptr<client_type> client;
    on_runner (runner,
               [&] ()
                 {
                   client.reset (new client_type (
                       pr::frame_link (
                           tr::framed::framed_stream<fr::slip::slip_codec> (
                               std::move (stream), fr::slip::slip_codec ())),
                       pr::client_options_for<protocol_type> ()));
                   if (setup)
                     setup (*client);
                 });
    return client;
  }

private:
  asio::io_context context_;
  tcp::acceptor acceptor_;
};
} // namespace bench_transport

#endif // !LUMEX_BENCHMARKS_TRANSPORT_TCP_HPP
