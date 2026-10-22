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

#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexSyncWait.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"

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
namespace datagram
{
using error::transport_errc;
typedef std::shared_ptr<message_impl> impl_ptr;

struct message_impl_access
{
  static void
  open (message_impl &impl, status_handler_t handler)
  {
    impl.do_open (std::move (handler));
  }
  static void
  receive (message_impl &impl, message_handler_t handler)
  {
    if (!impl.is_open ())
      {
        impl.complete (std::move (handler), transport_errc::not_open,
                       message_t ());
        return;
      }
    impl.do_receive (std::move (handler));
  }
  static void
  send (message_impl &impl, message_t message, status_handler_t handler)
  {
    if (!impl.is_open ())
      {
        impl.complete (std::move (handler), transport_errc::not_open);
        return;
      }
    impl.do_send (std::move (message), std::move (handler));
  }
  static void
  cancel (message_impl &impl)
  {
    impl.do_cancel ();
  }
  static void
  close (message_impl &impl)
  {
    impl.do_close ();
    impl.set_open (false);
  }
};

namespace
{
struct post_open
{
  impl_ptr impl;
  status_handler_t handler;
  void
  operator() ()
  {
    message_impl_access::open (*impl, std::move (handler));
  }
};

struct post_receive
{
  impl_ptr impl;
  message_handler_t handler;
  void
  operator() ()
  {
    message_impl_access::receive (*impl, std::move (handler));
  }
};

struct post_send
{
  impl_ptr impl;
  message_t message;
  status_handler_t handler;
  void
  operator() ()
  {
    message_impl_access::send (*impl, std::move (message),
                               std::move (handler));
  }
};

struct post_cancel
{
  impl_ptr impl;
  void
  operator() ()
  {
    message_impl_access::cancel (*impl);
  }
};

struct post_close
{
  impl_ptr impl;
  void
  operator() ()
  {
    message_impl_access::close (*impl);
  }
};

// The cancellation of one operation, on the strand: nothing when the
// operation completed since the signal (its mark is gone).
struct post_cancel_operation
{
  impl_ptr impl;
  std::weak_ptr<detail::operation_mark> operation;
  void
  operator() ()
  {
    if (!operation.expired ())
      message_impl_access::cancel (*impl);
  }
};

// The slot handler of the transport (see stream::stream_impl): a terminal
// cancellation cancels the transport; partial and total are not supported.
class terminal_cancellation
{
public:
  terminal_cancellation (impl_ptr const &impl,
                         detail::operation_mark_ptr const &operation)
      : impl_ (impl), operation_ (operation)
  {
  }

  void
  operator() (asio::cancellation_type_t type)
  {
    if ((type & asio::cancellation_type::terminal)
            == asio::cancellation_type::none
        || operation_.expired ())
      return;
    impl_ptr const impl = impl_.lock ();
    if (!impl)
      return;
    try
      {
        post_cancel_operation step = { impl, operation_ };
        asio::post (impl->strand (), std::move (step));
      }
    catch (...)
      {
        // Out of memory: the operation is not cancelled.
      }
  }

private:
  std::weak_ptr<message_impl> impl_;
  std::weak_ptr<detail::operation_mark> operation_;
};

template <typename Handler>
void
take_cancellation (impl_ptr const &impl, Handler &handler,
                   detail::operation_mark_ptr const &mark)
{
  if (!mark)
    return;
  asio::cancellation_slot slot
      = asio::get_associated_cancellation_slot (handler);
  if (slot.is_connected ())
    slot.template emplace<terminal_cancellation> (impl, mark);
}

// The state of a synchronous receive: the shared wait state and the message.
struct message_sync_state : detail::sync_state
{
  message_t message;
};

struct sync_message_handler
{
  std::shared_ptr<message_sync_state> state;
  void
  operator() (std::error_code error, message_t message)
  {
    std::lock_guard<std::mutex> const lock (state->mutex);
    state->error = error;
    state->message = std::move (message);
    state->done = true;
    state->done_signal.notify_all ();
  }
};
} // namespace

message_impl::message_impl (executor_type const &executor, std::string name)
    : executor_ (executor), strand_ (asio::make_strand (executor)),
      name_ (std::move (name)), open_ (false),
      counters_ (std::make_shared<statistics::statistics_counters> ())
{
}

message_impl::~message_impl () {}

// The handlers arrive already wrapped in the counting handler (see the
// start_* templates of the header); the slot handler goes in before the post.
void
message_impl::enqueue_open (status_handler_t handler,
                            detail::operation_mark_ptr const &mark)
{
  impl_ptr const self = shared_from_this ();
  take_cancellation (self, handler, mark);
  post_open step = { self, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
message_impl::enqueue_receive (message_handler_t handler,
                               detail::operation_mark_ptr const &mark)
{
  impl_ptr const self = shared_from_this ();
  take_cancellation (self, handler, mark);
  post_receive step = { self, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
message_impl::enqueue_send (message_t message, status_handler_t handler,
                            detail::operation_mark_ptr const &mark)
{
  impl_ptr const self = shared_from_this ();
  take_cancellation (self, handler, mark);
  post_send step = { self, std::move (message), std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
message_impl::start_cancel ()
{
  post_cancel step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

void
message_impl::start_close ()
{
  post_close step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

void
message_impl::complete (status_handler_t handler, std::error_code error)
{
  asio::post (strand_, asio::append (std::move (handler), error));
}

void
message_impl::complete (message_handler_t handler, std::error_code error,
                        message_t message)
{
  asio::post (strand_,
              asio::append (std::move (handler), error, std::move (message)));
}

message_transport::message_transport (std::shared_ptr<message_impl> impl)
    : impl_ (std::move (impl))
{
}

message_transport::~message_transport ()
{
  if (impl_)
    impl_->start_close ();
}

message_transport::message_transport (message_transport &&other) LUMEX_NOEXCEPT
    : impl_ (std::move (other.impl_))
{
}

message_transport &
message_transport::operator= (message_transport &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      if (impl_)
        impl_->start_close ();
      impl_ = std::move (other.impl_);
    }
  return *this;
}

executor_type
message_transport::get_executor () const LUMEX_NOEXCEPT
{
  return impl_->executor ();
}

std::string const &
message_transport::name () const LUMEX_NOEXCEPT
{
  return impl_->name ();
}

bool
message_transport::is_open () const LUMEX_NOEXCEPT
{
  return impl_ && impl_->is_open ();
}

statistics::statistics_t
message_transport::statistics () const LUMEX_NOEXCEPT
{
  if (!impl_)
    return statistics::statistics_t ();
  return impl_->counters ().snapshot ();
}

void
message_transport::reset_statistics () LUMEX_NOEXCEPT
{
  if (impl_)
    impl_->counters ().reset ();
}

void
message_transport::cancel () LUMEX_NOEXCEPT
{
  try
    {
      impl_->start_cancel ();
    }
  catch (...)
    {
    }
}

void
message_transport::close () LUMEX_NOEXCEPT
{
  try
    {
      impl_->start_close ();
    }
  catch (...)
    {
    }
}

status_result_t
message_transport::open (deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    {
      impl_->counters ().add_error (
          make_error_code (transport_errc::would_deadlock));
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (transport_errc::would_deadlock));
    }
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_open (detail::sync_status_handler{ state });
  std::error_code const error
      = detail::wait (*state, deadline, *this, impl_->counters ()).error;
  if (error)
    return status_result_t (lumex::core::expected::result::unexpect, error);
  return status_result_t ();
}

message_result_t
message_transport::receive (deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    {
      impl_->counters ().add_error (
          make_error_code (transport_errc::would_deadlock));
      return message_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (transport_errc::would_deadlock));
    }
  std::shared_ptr<message_sync_state> const state
      = std::make_shared<message_sync_state> ();
  impl_->start_receive (sync_message_handler{ state });
  std::error_code const error
      = detail::wait (*state, deadline, *this, impl_->counters ()).error;
  if (error)
    return message_result_t (lumex::core::expected::result::unexpect, error);
  std::lock_guard<std::mutex> const lock (state->mutex);
  return message_result_t (std::move (state->message));
}

status_result_t
message_transport::send (message_t const &message, deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    {
      impl_->counters ().add_error (
          make_error_code (transport_errc::would_deadlock));
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (transport_errc::would_deadlock));
    }
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_send (message, detail::sync_status_handler{ state });
  std::error_code const error
      = detail::wait (*state, deadline, *this, impl_->counters ()).error;
  if (error)
    return status_result_t (lumex::core::expected::result::unexpect, error);
  return status_result_t ();
}
} // namespace datagram
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
