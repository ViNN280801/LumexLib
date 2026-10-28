/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexAsioErrors.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace net
{
namespace
{
using error::transport_errc;
typedef asio::ip::tcp tcp;
typedef asio::ip::udp udp;

char const KTCP_COMPONENT[] = "transport.tcp";
char const KUDP_COMPONENT[] = "transport.udp";

std::string
endpoint_name (std::string const &host, std::uint16_t port)
{
  return host + ":" + std::to_string (port);
}

// A connection the peer reset or aborted is lost as surely as one it closed:
// the caller sees `disconnected` for all of them.
std::error_code
from_socket (asio::error_code const &error)
{
  if (error == asio::error::connection_reset
      || error == asio::error::connection_aborted
      || error == asio::error::broken_pipe)
    return make_error_code (transport_errc::disconnected);
  return detail::from_asio (error);
}

/**
 * The open in progress, shared by TCP and UDP. The resolver of Windows runs
 * getaddrinfo on a private thread that a cancel cannot stop, so a cancel or
 * close does not wait for it: it completes the open with `cancelled` at once,
 * and the generation tells a late resolve or connect that it was abandoned.
 */
class pending_open
{
public:
  bool
  active () const LUMEX_NOEXCEPT
  {
    return static_cast<bool> (handler_);
  }

  bool
  is_current (unsigned generation) const LUMEX_NOEXCEPT
  {
    return active () && generation == generation_;
  }

  unsigned
  begin (status_handler_t handler)
  {
    handler_ = std::move (handler);
    return ++generation_;
  }

  status_handler_t
  take ()
  {
    status_handler_t handler (std::move (handler_));
    handler_ = nullptr;
    ++generation_;
    return handler;
  }

private:
  status_handler_t handler_;
  unsigned generation_ = 0;
};

// --- TCP
// ---------------------------------------------------------------------

class tcp_stream_impl;
typedef std::shared_ptr<tcp_stream_impl> tcp_impl_ptr;

struct tcp_resolved
{
  tcp_impl_ptr impl;
  unsigned generation;
  void operator() (asio::error_code const &error,
                   tcp::resolver::results_type results);
};

struct tcp_connected
{
  tcp_impl_ptr impl;
  unsigned generation;
  void operator() (asio::error_code const &error, tcp::endpoint const &peer);
};

struct tcp_io_finished
{
  tcp_impl_ptr impl;
  io_handler_t handler;
  std::uint8_t const *data;
  bool is_read;
  void operator() (asio::error_code const &error, std::size_t bytes);
};

class tcp_stream_impl final : public stream::stream_impl
{
public:
  tcp_stream_impl (executor_type const &executor, tcp_options_t const &options)
      : stream_impl (executor, endpoint_name (options.host, options.port)),
        options_ (options), resolver_ (strand ()), socket_ (strand ())
  {
  }

  ~tcp_stream_impl () override
  {
    asio::error_code ignored;
    socket_.close (ignored);
  }

  void
  finish_resolve (unsigned generation, asio::error_code const &error,
                  tcp::resolver::results_type const &results)
  {
    if (!open_.is_current (generation))
      return;
    if (error)
      {
        fail_open (detail::from_asio (error), "resolve failed: ");
        return;
      }
    tcp_connected next
        = { std::static_pointer_cast<tcp_stream_impl> (shared_from_this ()),
            generation };
    asio::async_connect (socket_, results, std::move (next));
  }

  void
  finish_connect (unsigned generation, asio::error_code const &error)
  {
    if (!open_.is_current (generation))
      return;
    if (error)
      {
        fail_open (detail::from_asio (error), "connect failed: ");
        return;
      }
    asio::error_code option_error;
    socket_.set_option (tcp::no_delay (options_.no_delay), option_error);
    if (!option_error)
      socket_.set_option (asio::socket_base::keep_alive (options_.keep_alive),
                          option_error);
    if (option_error)
      {
        fail_open (detail::from_asio (option_error), "options failed: ");
        return;
      }
    set_open (true);
    LUMEX_TRANSPORT_LOG (info, KTCP_COMPONENT, name ().c_str (), "connected",
                         log::log_bytes_t ());
    complete (open_.take (), std::error_code ());
  }

  void
  finish_io (io_handler_t handler, std::uint8_t const *data, bool is_read,
             asio::error_code const &asio_error, std::size_t bytes)
  {
    std::error_code const error = from_socket (asio_error);
    if (bytes > 0)
      LUMEX_TRANSPORT_LOG (trace, KTCP_COMPONENT, name ().c_str (),
                           std::string (is_read ? "read " : "wrote ")
                               + std::to_string (bytes) + " B",
                           log::log_bytes_t (data, bytes));
    if (error == transport_errc::disconnected)
      LUMEX_TRANSPORT_LOG (warning, KTCP_COMPONENT, name ().c_str (),
                           "disconnected: " + asio_error.message (),
                           log::log_bytes_t ());
    complete (std::move (handler), error, bytes);
  }

  // The io_context is being destroyed: the open in progress dies uncalled (a
  // late resolve or connect finds no current open).
  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    status_handler_t const dropped (open_.take ());
  }

protected:
  void
  do_open (status_handler_t handler) override
  {
    if (is_open ())
      {
        complete (std::move (handler), std::error_code ());
        return;
      }
    if (open_.active ())
      {
        complete (std::move (handler), transport_errc::cancelled);
        return;
      }
    unsigned const generation = open_.begin (std::move (handler));
    tcp_resolved next
        = { std::static_pointer_cast<tcp_stream_impl> (shared_from_this ()),
            generation };
    resolver_.async_resolve (options_.host, std::to_string (options_.port),
                             tcp::resolver::numeric_service, std::move (next));
  }

  void
  do_read_some (mutable_bytes_t buffer, io_handler_t handler) override
  {
    tcp_io_finished done
        = { std::static_pointer_cast<tcp_stream_impl> (shared_from_this ()),
            std::move (handler), buffer.data (), true };
    socket_.async_read_some (asio::buffer (buffer.data (), buffer.size ()),
                             std::move (done));
  }

  void
  do_write_some (const_bytes_t data, io_handler_t handler) override
  {
    tcp_io_finished done
        = { std::static_pointer_cast<tcp_stream_impl> (shared_from_this ()),
            std::move (handler), data.data (), false };
    socket_.async_write_some (asio::buffer (data.data (), data.size ()),
                              std::move (done));
  }

  void
  do_cancel () override
  {
    if (open_.active ())
      {
        abandon_open ();
        return;
      }
    asio::error_code ignored;
    socket_.cancel (ignored);
  }

  void
  do_close () override
  {
    if (open_.active ())
      abandon_open ();
    if (socket_.is_open ())
      {
        asio::error_code ignored;
        socket_.shutdown (tcp::socket::shutdown_both, ignored);
        socket_.close (ignored);
        LUMEX_TRANSPORT_LOG (info, KTCP_COMPONENT, name ().c_str (), "closed",
                             log::log_bytes_t ());
      }
  }

private:
  void
  fail_open (std::error_code const &error, char const *what)
  {
    asio::error_code ignored;
    socket_.close (ignored);
    LUMEX_TRANSPORT_LOG (error, KTCP_COMPONENT, name ().c_str (),
                         what + error.message (), log::log_bytes_t ());
    complete (open_.take (), error);
  }

  // A cancel or close during the open: the connect (and a connect the
  // resolve has not started yet) ends with the socket.
  void
  abandon_open ()
  {
    resolver_.cancel ();
    asio::error_code ignored;
    socket_.close (ignored);
    complete (open_.take (), transport_errc::cancelled);
  }

  tcp_options_t options_;
  tcp::resolver resolver_;
  tcp::socket socket_;
  pending_open open_;
};

void
tcp_resolved::operator() (asio::error_code const &error,
                          tcp::resolver::results_type results)
{
  impl->finish_resolve (generation, error, results);
}

void
tcp_connected::operator() (asio::error_code const &error,
                           tcp::endpoint const &)
{
  impl->finish_connect (generation, error);
}

void
tcp_io_finished::operator() (asio::error_code const &error, std::size_t bytes)
{
  impl->finish_io (std::move (handler), data, is_read, error, bytes);
}

// --- UDP
// ---------------------------------------------------------------------

class udp_socket_impl;
typedef std::shared_ptr<udp_socket_impl> udp_impl_ptr;

struct udp_resolved
{
  udp_impl_ptr impl;
  unsigned generation;
  void operator() (asio::error_code const &error,
                   udp::resolver::results_type results);
};

struct udp_received
{
  udp_impl_ptr impl;
  datagram::message_handler_t handler;
  std::vector<std::uint8_t> buffer;
  void operator() (asio::error_code const &error, std::size_t bytes);
};

struct udp_sent
{
  udp_impl_ptr impl;
  status_handler_t handler;
  datagram::message_t message;
  void operator() (asio::error_code const &error, std::size_t bytes);
};

class udp_socket_impl final : public datagram::message_impl
{
public:
  udp_socket_impl (executor_type const &executor, udp_options_t const &options)
      : message_impl (executor, endpoint_name (options.host, options.port)),
        options_ (options), resolver_ (strand ()), socket_ (strand ())
  {
  }

  ~udp_socket_impl () override
  {
    asio::error_code ignored;
    socket_.close (ignored);
  }

  void
  finish_resolve (unsigned generation, asio::error_code const &error,
                  udp::resolver::results_type const &results)
  {
    if (!open_.is_current (generation))
      return;
    std::error_code failure = detail::from_asio (error);
    if (!failure && results.empty ())
      failure = transport_errc::invalid_endpoint;
    if (!failure)
      failure = connect (results.begin ()->endpoint ());
    if (failure)
      {
        asio::error_code ignored;
        socket_.close (ignored);
        LUMEX_TRANSPORT_LOG (error, KUDP_COMPONENT, name ().c_str (),
                             "open failed: " + failure.message (),
                             log::log_bytes_t ());
        complete (open_.take (), failure);
        return;
      }
    set_open (true);
    LUMEX_TRANSPORT_LOG (info, KUDP_COMPONENT, name ().c_str (), "opened",
                         log::log_bytes_t ());
    complete (open_.take (), std::error_code ());
  }

  void
  finish_receive (datagram::message_handler_t handler,
                  std::vector<std::uint8_t> buffer,
                  asio::error_code const &asio_error, std::size_t bytes)
  {
    // The receive buffer holds one byte more than max_datagram_size, so a
    // datagram that is too long shows on every platform: POSIX cuts it
    // silently to the buffer (one byte too many tells), Windows reports
    // message_size after filling it. Both return the datagram cut to
    // max_datagram_size and log it.
    std::size_t const limit = options_.max_datagram_size;
    bool cut = asio_error == asio::error::message_size;
    std::error_code error;
    if (!cut)
      error = detail::from_asio (asio_error);
    if (!error && bytes > limit)
      {
        cut = true;
        bytes = limit;
      }
    if (cut && !error)
      LUMEX_TRANSPORT_LOG (warning, KUDP_COMPONENT, name ().c_str (),
                           "datagram cut to " + std::to_string (limit) + " B",
                           log::log_bytes_t ());
    datagram::message_t message;
    if (!error)
      {
        buffer.resize (bytes);
        LUMEX_TRANSPORT_LOG (
            trace, KUDP_COMPONENT, name ().c_str (),
            "received " + std::to_string (bytes) + " B",
            log::log_bytes_t (buffer.data (), buffer.size ()));
        message.payload = std::move (buffer);
      }
    complete (std::move (handler), error, std::move (message));
  }

  void
  finish_send (status_handler_t handler, datagram::message_t const &message,
               asio::error_code const &asio_error)
  {
    std::error_code const error = detail::from_asio (asio_error);
    if (!error)
      LUMEX_TRANSPORT_LOG (
          trace, KUDP_COMPONENT, name ().c_str (),
          "sent " + std::to_string (message.payload.size ()) + " B",
          log::log_bytes_t (message.payload.data (), message.payload.size ()));
    complete (std::move (handler), error);
  }

  // The io_context is being destroyed: the open in progress dies uncalled (a
  // late resolve or connect finds no current open).
  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    status_handler_t const dropped (open_.take ());
  }

protected:
  void
  do_open (status_handler_t handler) override
  {
    if (is_open ())
      {
        complete (std::move (handler), std::error_code ());
        return;
      }
    if (open_.active ())
      {
        complete (std::move (handler), transport_errc::cancelled);
        return;
      }
    unsigned const generation = open_.begin (std::move (handler));
    udp_resolved next
        = { std::static_pointer_cast<udp_socket_impl> (shared_from_this ()),
            generation };
    resolver_.async_resolve (options_.host, std::to_string (options_.port),
                             udp::resolver::numeric_service, std::move (next));
  }

  void
  do_receive (datagram::message_handler_t handler) override
  {
    // The buffer becomes the payload: no copy of the datagram. One byte more
    // than max_datagram_size, to see a datagram that is too long (see
    // finish_receive).
    udp_received done
        = { std::static_pointer_cast<udp_socket_impl> (shared_from_this ()),
            std::move (handler),
            std::vector<std::uint8_t> (options_.max_datagram_size + 1) };
    asio::mutable_buffer const target
        = asio::buffer (done.buffer.data (), done.buffer.size ());
    socket_.async_receive (target, std::move (done));
  }

  void
  do_send (datagram::message_t message, status_handler_t handler) override
  {
    if (message.report_id != 0)
      {
        complete (std::move (handler),
                  std::make_error_code (std::errc::invalid_argument));
        return;
      }
    udp_sent done
        = { std::static_pointer_cast<udp_socket_impl> (shared_from_this ()),
            std::move (handler), std::move (message) };
    asio::const_buffer const source = asio::buffer (
        done.message.payload.data (), done.message.payload.size ());
    socket_.async_send (source, std::move (done));
  }

  void
  do_cancel () override
  {
    if (open_.active ())
      {
        abandon_open ();
        return;
      }
    asio::error_code ignored;
    socket_.cancel (ignored);
  }

  void
  do_close () override
  {
    if (open_.active ())
      abandon_open ();
    if (socket_.is_open ())
      {
        asio::error_code ignored;
        socket_.close (ignored);
        LUMEX_TRANSPORT_LOG (info, KUDP_COMPONENT, name ().c_str (), "closed",
                             log::log_bytes_t ());
      }
  }

private:
  // Opens the socket for the peer's protocol, binds the local port and
  // connects (for UDP only a filter: nothing is sent).
  std::error_code
  connect (udp::endpoint const &peer)
  {
    asio::error_code error;
    socket_.open (peer.protocol (), error);
    if (!error && options_.local_port != 0)
      socket_.bind (udp::endpoint (peer.protocol (), options_.local_port),
                    error);
    if (!error)
      socket_.connect (peer, error);
    return detail::from_asio (error);
  }

  void
  abandon_open ()
  {
    resolver_.cancel ();
    complete (open_.take (), transport_errc::cancelled);
  }

  udp_options_t options_;
  udp::resolver resolver_;
  udp::socket socket_;
  pending_open open_;
};

void
udp_resolved::operator() (asio::error_code const &error,
                          udp::resolver::results_type results)
{
  impl->finish_resolve (generation, error, results);
}

void
udp_received::operator() (asio::error_code const &error, std::size_t bytes)
{
  impl->finish_receive (std::move (handler), std::move (buffer), error, bytes);
}

void
udp_sent::operator() (asio::error_code const &error, std::size_t)
{
  impl->finish_send (std::move (handler), message, error);
}
} // namespace

stream::byte_stream
make_tcp_stream (executor_type const &executor, tcp_options_t const &options)
{
  return stream::byte_stream (
      std::make_shared<tcp_stream_impl> (executor, options));
}

datagram::message_transport
make_udp_socket (executor_type const &executor, udp_options_t const &options)
{
  return datagram::message_transport (
      std::make_shared<udp_socket_impl> (executor, options));
}
} // namespace net
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
