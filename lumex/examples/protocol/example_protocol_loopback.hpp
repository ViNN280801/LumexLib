// A simulated device behind a TCP port of the loopback interface, for the
// examples of the dialogue layer. The device is a testing::device_simulator<P>
// on a mock stream; the code here only carries the bytes between the mock and
// the sockets of the clients, so that a program talks to it over a real TCP
// connection: the same client, framing and stream as with a serial port.
// A thread runs the device and moves its virtual clock with the real one, so
// that behaviours that take time (a slow device) work as they would. Nothing
// here is the library's API: it is a helper of the examples.
#ifndef LUMEX_EXAMPLES_PROTOCOL_LOOPBACK_HPP
#define LUMEX_EXAMPLES_PROTOCOL_LOOPBACK_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "lumex/applied/transport/LumexTransport"

#if defined(LUMEX_TRANSPORT_WITH_PROTOCOL)                                    \
    && defined(LUMEX_TRANSPORT_WITH_TESTING)
namespace loopback
{
namespace tr = lumex::applied::transport;

/// One accepted connection: bytes of the socket are written to the mock
/// stream (where the simulator reads them as the requests of the client),
/// bytes the simulator sends are written to the socket.
class connection_t : public std::enable_shared_from_this<connection_t>
{
public:
  connection_t (asio::ip::tcp::socket socket, tr::stream::byte_stream &stream,
                tr::testing::mock_stream_control control)
      : socket_ (std::move (socket)), stream_ (stream), control_ (control),
        inbound_ (256), outbound_ (256), done_ (false)
  {
  }

  void
  start ()
  {
    std::shared_ptr<connection_t> const self = shared_from_this ();
    stream_.async_open (
        [self] (std::error_code error)
          {
            if (error)
              {
                self->abort ();
                return;
              }
            self->read_socket ();
            self->read_mock ();
          });
  }

  /// Ends the connection: the client sees the peer go.
  void
  abort ()
  {
    if (done_)
      return;
    done_ = true;
    asio::error_code ignored;
    socket_.close (ignored);
    control_.disconnect ();
    stream_.close ();
  }

private:
  void
  read_socket ()
  {
    std::shared_ptr<connection_t> const self = shared_from_this ();
    socket_.async_read_some (
        asio::buffer (inbound_),
        [self] (asio::error_code error, std::size_t bytes)
          {
            if (error)
              {
                self->abort ();
                return;
              }
            self->stream_.async_write_all (
                tr::const_bytes_t (self->inbound_.data (), bytes),
                [self] (std::error_code failed, std::size_t)
                  {
                    if (failed)
                      self->abort ();
                    else
                      self->read_socket ();
                  });
          });
  }

  void
  read_mock ()
  {
    std::shared_ptr<connection_t> const self = shared_from_this ();
    stream_.async_read_some (
        tr::mutable_bytes_t (outbound_.data (), outbound_.size ()),
        [self] (std::error_code error, std::size_t bytes)
          {
            if (error)
              {
                self->abort ();
                return;
              }
            asio::async_write (self->socket_,
                               asio::buffer (self->outbound_.data (), bytes),
                               [self] (asio::error_code failed, std::size_t)
                                 {
                                   if (failed)
                                     self->abort ();
                                   else
                                     self->read_mock ();
                                 });
          });
  }

  asio::ip::tcp::socket socket_;
  tr::stream::byte_stream &stream_;
  tr::testing::mock_stream_control control_;
  std::vector<std::uint8_t> inbound_;
  std::vector<std::uint8_t> outbound_;
  bool done_;
};

/// A device of the table `P` that listens on 127.0.0.1 (a port the system
/// picks). One client at a time: a new connection ends the one before.
template <typename P> class device_t
{
public:
  /// A device that frames with `codec` (a codec of lumex::framing).
  template <typename Codec>
  explicit device_t (Codec codec)
      : work_ (context_.get_executor ()),
        acceptor_ (context_, asio::ip::tcp::endpoint (
                                 asio::ip::address_v4::loopback (), 0)),
        stopping_ (false)
  {
    tr::testing::mock_options_t options;
    options.name = "device";
    options.timers = clock_.source ();
    mock_.reset (new tr::testing::mock_stream (
        tr::testing::make_mock_stream (context_.get_executor (), options)));
    simulator_.reset (new tr::testing::device_simulator<P> (
        mock_->control, std::move (codec), clock_));
    accept ();
    thread_ = std::thread ([this] { run (); });
  }

  ~device_t ()
  {
    asio::post (context_,
                [this]
                  {
                    asio::error_code ignored;
                    acceptor_.close (ignored);
                    if (active_)
                      active_->abort ();
                  });
    stopping_ = true;
    thread_.join ();
  }

  device_t (device_t const &) = delete;
  device_t &operator= (device_t const &) = delete;

  /// The port to connect to.
  std::uint16_t
  port () const
  {
    return acceptor_.local_endpoint ().port ();
  }

  /// The simulator: set its replies and its behaviour from any thread.
  tr::testing::device_simulator<P> &
  simulator ()
  {
    return *simulator_;
  }

  /// Drops the connection the way a device that was reset does.
  void
  drop_connection ()
  {
    asio::post (context_,
                [this]
                  {
                    if (active_)
                      active_->abort ();
                  });
  }

private:
  void
  accept ()
  {
    acceptor_.async_accept (
        [this] (asio::error_code error, asio::ip::tcp::socket socket)
          {
            if (error)
              return;
            if (active_)
              active_->abort ();
            active_.reset (new connection_t (std::move (socket), mock_->stream,
                                             mock_->control));
            active_->start ();
            accept ();
          });
  }

  void
  run ()
  {
    std::chrono::milliseconds const step (1);
    while (!stopping_)
      {
        context_.run_for (step);
        clock_.advance (step);
      }
    // The close above ended the pending operations; let their handlers run.
    for (int round = 0; round < 20; ++round)
      context_.poll ();
  }

  asio::io_context context_;
  asio::executor_work_guard<asio::io_context::executor_type> work_;
  tr::testing::virtual_clock clock_;
  asio::ip::tcp::acceptor acceptor_;
  std::unique_ptr<tr::testing::mock_stream> mock_;
  std::unique_ptr<tr::testing::device_simulator<P>> simulator_;
  std::shared_ptr<connection_t> active_;
  std::atomic<bool> stopping_;
  std::thread thread_;
};
} // namespace loopback
#endif

#endif // !LUMEX_EXAMPLES_PROTOCOL_LOOPBACK_HPP
