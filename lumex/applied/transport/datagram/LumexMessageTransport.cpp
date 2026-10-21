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
      name_ (std::move (name)), open_ (false)
{
}

message_impl::~message_impl () {}

void
message_impl::start_open (status_handler_t handler)
{
  post_open step = { shared_from_this (), std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
message_impl::start_receive (message_handler_t handler)
{
  post_receive step = { shared_from_this (), std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
message_impl::start_send (message_t message, status_handler_t handler)
{
  post_send step
      = { shared_from_this (), std::move (message), std::move (handler) };
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
    return status_result_t (lumex::core::expected::result::unexpect,
                            make_error_code (transport_errc::would_deadlock));
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_open (status_handler_t (detail::sync_status_handler{ state }));
  std::error_code const error = detail::wait (*state, deadline, *this).error;
  if (error)
    return status_result_t (lumex::core::expected::result::unexpect, error);
  return status_result_t ();
}

message_result_t
message_transport::receive (deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    return message_result_t (lumex::core::expected::result::unexpect,
                             make_error_code (transport_errc::would_deadlock));
  std::shared_ptr<message_sync_state> const state
      = std::make_shared<message_sync_state> ();
  impl_->start_receive (message_handler_t (sync_message_handler{ state }));
  std::error_code const error = detail::wait (*state, deadline, *this).error;
  if (error)
    return message_result_t (lumex::core::expected::result::unexpect, error);
  std::lock_guard<std::mutex> const lock (state->mutex);
  return message_result_t (std::move (state->message));
}

status_result_t
message_transport::send (message_t const &message, deadline_t deadline)
{
  if (detail::runs_on (impl_->executor ()))
    return status_result_t (lumex::core::expected::result::unexpect,
                            make_error_code (transport_errc::would_deadlock));
  std::shared_ptr<detail::sync_state> const state
      = std::make_shared<detail::sync_state> ();
  impl_->start_send (message,
                     status_handler_t (detail::sync_status_handler{ state }));
  std::error_code const error = detail::wait (*state, deadline, *this).error;
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
