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
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionCore.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionImpl.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionStream.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"

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
namespace session
{
namespace detail
{
namespace
{
using error::transport_errc;

// The event of a connection, on the strand (never inside the state change).
struct connection_step_t
{
  std::shared_ptr<connection_handler_t const> handler;
  connection_event event;
  std::error_code reason;

  void
  operator() ()
  {
    (*handler) (event, reason);
  }
};

// The end of an open: the session completes it, the view hands it on.
template <typename View> struct view_status_done_t
{
  using executor_type = session_core::strand_type;

  std::weak_ptr<View> view;
  executor_type strand;
  status_handler_t handler;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void
  operator() (std::error_code error)
  {
    std::shared_ptr<View> const alive = view.lock ();
    if (alive)
      alive->finish_status (std::move (handler), error);
    else
      asio::post (strand, asio::append (std::move (handler), error));
  }
};

/**
 * @brief What both views do: a `Base` (`stream_impl`, `message_impl`) that is
 * the session over `Transport`.
 */
template <typename Base, typename Transport>
class session_view_t : public Base, public session_link_listener
{
public:
  using session_type = session_impl<Transport>;

  explicit session_view_t (std::shared_ptr<session_type> const &session)
      : Base (session->executor (), session->strand (),
              session->name ().empty () ? std::string ("session")
                                        : session->name ()),
        session_ (session)
  {
    this->set_open (session->state () == channel_state::open);
  }

  bool
  reconnects () const LUMEX_NOEXCEPT override
  {
    return true;
  }

  void
  watch_connection (connection_handler_t handler) override
  {
    // Before the lock: the handler that goes dies after it, and its destructor
    // may call back.
    std::shared_ptr<connection_handler_t const> made
        = handler ? std::make_shared<connection_handler_t const> (
                        std::move (handler))
                  : std::shared_ptr<connection_handler_t const> ();
    std::lock_guard<std::mutex> const lock (watcher_mutex_);
    watcher_.swap (made);
  }

  void
  drop_connection () override
  {
    session_->request_drop_link (
        make_error_code (transport_errc::disconnected), false);
  }

  void
  reject_connection () override
  {
    session_->request_drop_link (
        make_error_code (transport_errc::disconnected), true);
  }

  void
  link_changed (channel_state previous, channel_state current,
                std::error_code const &reason) LUMEX_NOEXCEPT override
  {
    if (previous == channel_state::open && current != channel_state::open)
      this->note_connection_over ();
    this->set_open (current == channel_state::open);
    if (previous == current)
      return;
    connection_event event = connection_event::opened;
    if (current == channel_state::reconnecting)
      event = connection_event::lost;
    else if (current == channel_state::closed
             || current == channel_state::faulted)
      event = connection_event::gave_up;
    else if (current != channel_state::open)
      return;
    try
      {
        std::shared_ptr<connection_handler_t const> handler;
        {
          std::lock_guard<std::mutex> const lock (watcher_mutex_);
          handler = watcher_;
        }
        if (!handler)
          return;
        connection_step_t step = { handler, event, reason };
        asio::post (this->strand (), std::move (step));
      }
    catch (...)
      {
        // Out of memory: nobody hears this change.
      }
  }

  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    std::shared_ptr<connection_handler_t const> gone;
    std::lock_guard<std::mutex> const lock (watcher_mutex_);
    watcher_.swap (gone);
  }

  void
  finish_status (status_handler_t handler, std::error_code const &error)
  {
    this->complete (std::move (handler), error);
  }

protected:
  template <typename View>
  void
  open_as (std::shared_ptr<View> const &self, status_handler_t handler)
  {
    view_status_done_t<View> done
        = { self, session_->strand (), std::move (handler) };
    session_->start_open (0, std::move (done));
  }

  void
  do_cancel () override
  {
    // The I/O only: a cancel from above never ends the reconnecting.
    session_->cancel_io ();
  }

  void
  do_close () override
  {
    session_->request_close ();
  }

  std::shared_ptr<session_type> session_;

private:
  std::mutex watcher_mutex_;
  std::shared_ptr<connection_handler_t const> watcher_;
};

class session_stream_t;

// The end of a read: bytes of a connection that is over are not handed on, so
// that no adapter glues them to the next connection.
struct stream_read_done_t
{
  using executor_type = session_core::strand_type;

  std::weak_ptr<session_stream_t> view;
  executor_type strand;
  io_handler_t handler;
  std::size_t epoch;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void operator() (std::error_code error, std::size_t bytes);
};

struct stream_write_done_t
{
  using executor_type = session_core::strand_type;

  std::weak_ptr<session_stream_t> view;
  executor_type strand;
  io_handler_t handler;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void operator() (std::error_code error, std::size_t bytes);
};

/// @brief A session as a stream.
class session_stream_t final
    : public session_view_t<stream::stream_impl, stream::byte_stream>
{
public:
  explicit session_stream_t (std::shared_ptr<session_type> const &session)
      : session_view_t<stream::stream_impl, stream::byte_stream> (session)
  {
  }

  void
  finish_io (io_handler_t handler, std::error_code const &error,
             std::size_t bytes)
  {
    complete (std::move (handler), error, bytes);
  }

protected:
  void
  do_open (status_handler_t handler) override
  {
    open_as (self (), std::move (handler));
  }

  void
  do_read_some (mutable_bytes_t buffer, io_handler_t handler) override
  {
    stream_read_done_t done = { self (), session_->strand (),
                                std::move (handler), connection_epoch () };
    read_some_operation operation = { buffer };
    session_->run_io (
        operation, std::move (done),
        std::shared_ptr<asio::cancellation_signal> (),
        ::lumex::applied::transport::detail::operation_mark_ptr ());
  }

  void
  do_write_some (const_bytes_t data, io_handler_t handler) override
  {
    stream_write_done_t done
        = { self (), session_->strand (), std::move (handler) };
    write_some_operation operation = { data };
    session_->run_io (
        operation, std::move (done),
        std::shared_ptr<asio::cancellation_signal> (),
        ::lumex::applied::transport::detail::operation_mark_ptr ());
  }

private:
  std::shared_ptr<session_stream_t>
  self ()
  {
    return std::static_pointer_cast<session_stream_t> (shared_from_this ());
  }
};

void
stream_read_done_t::operator() (std::error_code error, std::size_t bytes)
{
  std::shared_ptr<session_stream_t> const alive = view.lock ();
  if (!alive)
    {
      asio::post (strand, asio::append (std::move (handler), error, bytes));
      return;
    }
  if (alive->connection_epoch () != epoch)
    {
      error = make_error_code (transport_errc::cancelled);
      bytes = 0;
    }
  alive->finish_io (std::move (handler), error, bytes);
}

void
stream_write_done_t::operator() (std::error_code error, std::size_t bytes)
{
  std::shared_ptr<session_stream_t> const alive = view.lock ();
  if (alive)
    alive->finish_io (std::move (handler), error, bytes);
  else
    asio::post (strand, asio::append (std::move (handler), error, bytes));
}

class session_message_t;

// The end of a receive: a message of a connection that is over is not handed
// on.
struct message_receive_done_t
{
  using executor_type = session_core::strand_type;

  std::weak_ptr<session_message_t> view;
  executor_type strand;
  datagram::message_handler_t handler;
  std::size_t epoch;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void operator() (std::error_code error, datagram::message_t message);
};

/// @brief A session as a message transport.
class session_message_t final
    : public session_view_t<datagram::message_impl,
                            datagram::message_transport>
{
public:
  explicit session_message_t (std::shared_ptr<session_type> const &session)
      : session_view_t<datagram::message_impl, datagram::message_transport> (
            session)
  {
  }

  void
  finish_message (datagram::message_handler_t handler,
                  std::error_code const &error, datagram::message_t message)
  {
    complete (std::move (handler), error, std::move (message));
  }

protected:
  void
  do_open (status_handler_t handler) override
  {
    open_as (self (), std::move (handler));
  }

  void
  do_receive (datagram::message_handler_t handler) override
  {
    message_receive_done_t done = { self (), session_->strand (),
                                    std::move (handler), connection_epoch () };
    receive_operation operation;
    session_->run_io (
        operation, std::move (done),
        std::shared_ptr<asio::cancellation_signal> (),
        ::lumex::applied::transport::detail::operation_mark_ptr ());
  }

  void
  do_send (datagram::message_t message, status_handler_t handler) override
  {
    view_status_done_t<session_message_t> done
        = { self (), session_->strand (), std::move (handler) };
    send_operation operation;
    operation.message = std::move (message);
    session_->run_io (
        operation, std::move (done),
        std::shared_ptr<asio::cancellation_signal> (),
        ::lumex::applied::transport::detail::operation_mark_ptr ());
  }

private:
  std::shared_ptr<session_message_t>
  self ()
  {
    return std::static_pointer_cast<session_message_t> (shared_from_this ());
  }
};

void
message_receive_done_t::operator() (std::error_code error,
                                    datagram::message_t message)
{
  std::shared_ptr<session_message_t> const alive = view.lock ();
  if (!alive)
    {
      asio::post (strand, asio::append (std::move (handler), error,
                                        std::move (message)));
      return;
    }
  if (alive->connection_epoch () != epoch)
    {
      error = make_error_code (transport_errc::cancelled);
      message = datagram::message_t ();
    }
  alive->finish_message (std::move (handler), error, std::move (message));
}

template <typename View, typename Session>
std::shared_ptr<View>
make_view (std::shared_ptr<Session> const &session)
{
  if (!session)
    return std::shared_ptr<View> ();
  std::shared_ptr<View> const made = std::make_shared<View> (session);
  session->add_link_listener (std::weak_ptr<session_link_listener> (
      std::shared_ptr<session_link_listener> (made)));
  return made;
}
} // namespace

std::shared_ptr<stream::stream_impl>
make_session_stream (
    std::shared_ptr<session_impl<stream::byte_stream>> const &session)
{
  return make_view<session_stream_t> (session);
}

std::shared_ptr<datagram::message_impl>
make_session_message (
    std::shared_ptr<session_impl<datagram::message_transport>> const &session)
{
  return make_view<session_message_t> (session);
}
} // namespace detail
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
