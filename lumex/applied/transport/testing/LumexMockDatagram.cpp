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

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"

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
namespace testing
{
using datagram::message_t;
using error::transport_errc;

/// The implementation of a mock message transport; every member is guarded
/// by mutex_.
class mock_datagram_impl final : public datagram::message_impl
{
public:
  mock_datagram_impl (executor_type const &executor, std::string const &name)
      : message_impl (executor, name)
  {
  }

  void
  deliver (message_t const &message)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    incoming_.push_back (message);
    serve_pending_receive ();
  }

  void
  fail_open (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    open_error_ = error;
  }

  void
  fail_next_receive (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (has_pending_receive_)
      {
        has_pending_receive_ = false;
        complete (std::move (pending_handler_), error, message_t ());
        return;
      }
    receive_error_ = error;
  }

  void
  fail_next_send (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    send_error_ = error;
  }

  void
  disconnect ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    disconnected_ = true;
    serve_pending_receive ();
  }

  std::vector<message_t>
  sent () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return sent_;
  }

  // The io_context is being destroyed: the pending receive dies uncalled,
  // after the lock is released (its destructor may destroy the last owner of
  // this transport).
  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    datagram::message_handler_t receive;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      receive = std::move (pending_handler_);
      has_pending_receive_ = false;
    }
  }

protected:
  void
  do_open (status_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (open_error_)
      {
        std::error_code const error = open_error_;
        open_error_ = std::error_code ();
        complete (std::move (handler), error);
        return;
      }
    disconnected_ = false;
    set_open (true);
    complete (std::move (handler), std::error_code ());
  }

  void
  do_receive (datagram::message_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (receive_error_)
      {
        std::error_code const error = receive_error_;
        receive_error_ = std::error_code ();
        complete (std::move (handler), error, message_t ());
        return;
      }
    pending_handler_ = std::move (handler);
    has_pending_receive_ = true;
    serve_pending_receive ();
  }

  void
  do_send (message_t message, status_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (send_error_)
      {
        std::error_code const error = send_error_;
        send_error_ = std::error_code ();
        complete (std::move (handler), error);
        return;
      }
    sent_.push_back (std::move (message));
    complete (std::move (handler), std::error_code ());
  }

  void
  do_cancel () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_receive (transport_errc::cancelled);
  }

  void
  do_close () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_receive (transport_errc::cancelled);
  }

private:
  void
  serve_pending_receive ()
  {
    if (!has_pending_receive_)
      return;
    if (!incoming_.empty ())
      {
        message_t message = std::move (incoming_.front ());
        incoming_.pop_front ();
        has_pending_receive_ = false;
        complete (std::move (pending_handler_), std::error_code (),
                  std::move (message));
        return;
      }
    if (disconnected_)
      end_pending_receive (transport_errc::disconnected);
  }

  void
  end_pending_receive (transport_errc reason)
  {
    if (!has_pending_receive_)
      return;
    has_pending_receive_ = false;
    complete (std::move (pending_handler_), make_error_code (reason),
              message_t ());
  }

  mutable std::mutex mutex_;
  std::deque<message_t> incoming_;
  std::vector<message_t> sent_;
  datagram::message_handler_t pending_handler_;
  bool has_pending_receive_ = false;
  bool disconnected_ = false;
  std::error_code open_error_;
  std::error_code receive_error_;
  std::error_code send_error_;
};

mock_datagram_control::mock_datagram_control (
    std::shared_ptr<mock_datagram_impl> impl)
    : impl_ (std::move (impl))
{
}

void
mock_datagram_control::deliver (message_t const &message)
{
  impl_->deliver (message);
}

void
mock_datagram_control::fail_open (std::error_code error)
{
  impl_->fail_open (error);
}

void
mock_datagram_control::fail_next_receive (std::error_code error)
{
  impl_->fail_next_receive (error);
}

void
mock_datagram_control::fail_next_send (std::error_code error)
{
  impl_->fail_next_send (error);
}

void
mock_datagram_control::disconnect ()
{
  impl_->disconnect ();
}

std::vector<message_t>
mock_datagram_control::sent () const
{
  return impl_->sent ();
}

mock_datagram
make_mock_datagram (executor_type const &executor, std::string const &name)
{
  std::shared_ptr<mock_datagram_impl> const impl
      = std::make_shared<mock_datagram_impl> (executor, name);
  mock_datagram result
      = { datagram::message_transport (impl), mock_datagram_control (impl) };
  return result;
}
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
