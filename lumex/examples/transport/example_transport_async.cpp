// The asynchronous API of lumex::transport with callbacks: open, write and a
// read loop chained by their completion handlers, which run on the stream's
// strand of the io_runner. A second thread closes the stream while a read is
// pending: the read ends with transport_errc::cancelled at once (close and
// cancel are safe from any thread).
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

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

namespace
{
// A client: its handlers keep it alive through shared_from_this.
class client : public std::enable_shared_from_this<client>
{
public:
  client (tr::stream::byte_stream stream, std::promise<std::error_code> &done)
      : stream_ (std::move (stream)), buffer_ (64), done_ (done)
  {
  }

  void
  start ()
  {
    std::shared_ptr<client> const self = shared_from_this ();
    stream_.async_open (
        [self] (std::error_code error)
          {
            if (error)
              return self->finish (error);
            self->send ();
          });
  }

  void
  close ()
  {
    stream_.close ();
  }

private:
  void
  send ()
  {
    std::shared_ptr<client> const self = shared_from_this ();
    std::string const text = "STATUS?\n";
    request_.assign (text.begin (), text.end ());
    stream_.async_write_all (tr::const_bytes_t (request_),
                             [self] (std::error_code error, std::size_t)
                               {
                                 if (error)
                                   return self->finish (error);
                                 self->read ();
                               });
  }

  void
  read ()
  {
    std::shared_ptr<client> const self = shared_from_this ();
    stream_.async_read_some (
        tr::mutable_bytes_t (buffer_),
        [self] (std::error_code error, std::size_t bytes)
          {
            if (error)
              return self->finish (error);
            std::cout << "got " << bytes << " B: "
                      << tr::log::format_dump (tr::log::log_bytes_t (
                             self->buffer_.data (), bytes))
                      << '\n';
            self->read ();
          });
  }

  void
  finish (std::error_code error)
  {
    done_.set_value (error);
  }

  tr::stream::byte_stream stream_;
  std::vector<std::uint8_t> request_;
  std::vector<std::uint8_t> buffer_;
  std::promise<std::error_code> &done_;
};
} // namespace

int
main ()
{
  tr::stream::io_runner runner (2);

  // An echo peer on loopback, run by the same io_runner.
  asio::ip::tcp::acceptor acceptor (
      runner.context (),
      asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  asio::ip::tcp::socket peer (runner.context ());
  std::vector<std::uint8_t> peer_buffer (64);
  acceptor.async_accept (
      peer,
      [&] (asio::error_code const &error)
        {
          if (error)
            return;
          peer.async_read_some (
              asio::buffer (peer_buffer),
              [&] (asio::error_code const &read_error, std::size_t bytes)
                {
                  if (!read_error)
                    asio::async_write (
                        peer, asio::buffer (peer_buffer.data (), bytes),
                        [] (asio::error_code const &, std::size_t) {});
                });
        });

  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  std::promise<std::error_code> done;
  std::shared_ptr<client> const user = std::make_shared<client> (
      tr::net::make_tcp_stream (runner.get_executor (), options), done);
  user->start ();

  // The echo arrives at once; the next read waits until the close.
  std::this_thread::sleep_for (std::chrono::milliseconds (300));
  user->close ();
  std::error_code const error = done.get_future ().get ();
  std::cout << "the read loop ended: " << error.message () << " (cancelled: "
            << (error == tr::error::transport_errc::cancelled ? "yes" : "no")
            << ")\n";
  asio::error_code ignored;
  peer.close (ignored);
  acceptor.close (ignored);
  return 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
