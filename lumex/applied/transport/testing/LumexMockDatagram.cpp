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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/detail/LumexScheduledDeliveries.hpp"
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
/// by mutex_. Observers are called outside the lock.
class mock_datagram_impl final : public datagram::message_impl
{
public:
  mock_datagram_impl (executor_type const &executor,
                      mock_options_t const &options)
      : message_impl (executor, options.name), timers_ (options.timers),
        script_enlisted_ (false)
  {
  }

  void
  deliver (message_t const &message)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    incoming_.push_back (message);
    serve_receives ();
  }

  void
  fail_open (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    open_error_ = error;
  }

  void
  hold_next_open ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    hold_open_ = true;
  }

  // The oldest pending receive ends with `error`; without one the next
  // receive does.
  void
  fail_next_receive (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (!receives_.empty ())
      {
        datagram::message_handler_t handler = std::move (receives_.front ());
        receives_.pop_front ();
        complete (std::move (handler), error, message_t ());
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
  fail_send_after (std::size_t sends, std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    sends_before_failure_ = sends;
    delayed_send_error_ = error;
  }

  void
  disconnect ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    disconnected_ = true;
    serve_receives ();
    end_pending_sends (transport_errc::disconnected);
  }

  std::vector<message_t>
  sent () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return sent_;
  }

  void
  on_send (send_observer_t observer)
  {
    // An observer may own a control of this mock (a cycle): the shutdown of
    // the context drops it, which needs the mock to be enlisted.
    observer_ptr_t fresh;
    if (observer)
      {
        detail::enlist_keeper (script_enlisted_, executor (),
                               std::static_pointer_cast<mock_datagram_impl> (
                                   shared_from_this ()));
        fresh = std::make_shared<send_observer_t const> (std::move (observer));
      }
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      observer_.swap (fresh);
    }
    // The replaced observer dies here, outside the lock.
  }

  void
  deliver_after (std::chrono::milliseconds delay, message_t const &message)
  {
    scripted_step_t step;
    step.kind = scripted_step_t::deliver_message;
    step.message = message;
    schedule (delay, std::move (step));
  }

  void
  disconnect_after (std::chrono::milliseconds delay)
  {
    scripted_step_t step;
    step.kind = scripted_step_t::hang_up;
    schedule (delay, std::move (step));
  }

  void
  fail_opens_for (std::chrono::milliseconds duration, std::error_code error)
  {
    timing::time_point_t const until
        = detail::saturating_add (timers_.now (), duration);
    std::lock_guard<std::mutex> const lock (mutex_);
    open_fail_until_ = until;
    open_fail_error_ = error;
  }

  void
  hold_sends ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    hold_sends_ = true;
  }

  // Held sends go on the strand, in order, one by one (an observer may hold
  // again between two of them).
  void
  release_sends ()
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      hold_sends_ = false;
      if (sends_.empty ())
        return;
    }
    release_step_t step = { std::static_pointer_cast<mock_datagram_impl> (
        shared_from_this ()) };
    asio::post (strand (), std::move (step));
  }

  // A scheduled step came due (on the strand): its message becomes
  // receivable, or the link goes down.
  void
  deliver_scheduled (std::uint64_t id)
  {
    scripted_step_t step;
    timing::timer expired;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (!scheduled_.take (id, step, expired))
        return;
      switch (step.kind)
        {
        case scripted_step_t::deliver_message:
          incoming_.push_back (std::move (step.message));
          break;
        case scripted_step_t::hang_up:
          disconnected_ = true;
          end_pending_sends (transport_errc::disconnected);
          break;
        }
      serve_receives ();
    }
  }

  // The io_context is being destroyed: the pending receives and open and the
  // observer die uncalled, after the lock is released (their destructors may
  // destroy the last owner of this transport).
  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    std::deque<datagram::message_handler_t> receives;
    status_handler_t open;
    std::deque<pending_send_t> sends;
    observer_ptr_t observer;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      receives.swap (receives_);
      sends.swap (sends_);
      open = std::move (pending_open_);
      observer.swap (observer_);
      has_pending_open_ = false;
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
    if (open_fail_error_ && timers_.now () < open_fail_until_)
      {
        complete (std::move (handler), open_fail_error_);
        return;
      }
    if (hold_open_)
      {
        hold_open_ = false;
        pending_open_ = std::move (handler);
        has_pending_open_ = true;
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
    receives_.push_back (std::move (handler));
    serve_receives ();
  }

  void
  do_send (message_t message, status_handler_t handler) override
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (hold_sends_ || !sends_.empty ())
        {
          // Behind the held sends, so that they keep their order.
          pending_send_t held = { std::move (message), std::move (handler) };
          sends_.push_back (std::move (held));
          return;
        }
    }
    accept_send (std::move (message), std::move (handler));
  }

  void
  do_cancel () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_open ();
    end_pending_receives (transport_errc::cancelled);
    end_pending_sends (transport_errc::cancelled);
  }

  void
  do_close () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    end_pending_open ();
    end_pending_receives (transport_errc::cancelled);
    end_pending_sends (transport_errc::cancelled);
  }

private:
  // What a scripted delivery does when its time comes.
  struct scripted_step_t
  {
    enum kind_t
    {
      deliver_message,
      hang_up
    };

    kind_t kind = deliver_message;
    message_t message;
  };

  using scheduled_t
      = detail::scheduled_deliveries<mock_datagram_impl, scripted_step_t>;
  using observer_ptr_t = std::shared_ptr<send_observer_t const>;

  void
  schedule (std::chrono::milliseconds delay, scripted_step_t step)
  {
    std::shared_ptr<mock_datagram_impl> const self
        = std::static_pointer_cast<mock_datagram_impl> (shared_from_this ());
    std::uint64_t id = 0;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      id = scheduled_.add (std::move (step));
    }
    // The timer is made and the wait started outside the lock.
    timing::timer timer;
    try
      {
        timer = scheduled_t::start (self, timers_, executor_type (strand ()),
                                    delay, id);
      }
    catch (...)
      {
        std::lock_guard<std::mutex> const lock (mutex_);
        scheduled_.remove (id);
        throw;
      }
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      scheduled_.attach (id, timer);
    }
  }

  struct pending_send_t
  {
    message_t message;
    status_handler_t handler;
  };

  // The posted step that lets the held sends go.
  struct release_step_t
  {
    std::shared_ptr<mock_datagram_impl> impl;

    void
    operator() ()
    {
      impl->flush_sends ();
    }
  };

  // Takes one send under the lock, calls the observer outside it. On the
  // strand.
  void
  accept_send (message_t message, status_handler_t handler)
  {
    observer_ptr_t observer;
    message_t seen;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (!take_send (std::move (message), std::move (handler), observer,
                      seen))
        return;
    }
    if (observer)
      (*observer) (seen);
  }

  void
  flush_sends ()
  {
    for (;;)
      {
        observer_ptr_t observer;
        message_t seen;
        {
          std::lock_guard<std::mutex> const lock (mutex_);
          if (hold_sends_ || sends_.empty ())
            return;
          pending_send_t held = std::move (sends_.front ());
          sends_.pop_front ();
          if (!take_send (std::move (held.message), std::move (held.handler),
                          observer, seen))
            continue;
        }
        if (!observer)
          continue;
        try
          {
            (*observer) (seen);
          }
        catch (...)
          {
            // The held sends that are left must not wait for ever: flush
            // them in a new step, then let the exception go.
            repost_flush ();
            throw;
          }
      }
  }

  void
  repost_flush ()
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (hold_sends_ || sends_.empty ())
        return;
    }
    release_step_t step = { std::static_pointer_cast<mock_datagram_impl> (
        shared_from_this ()) };
    asio::post (strand (), std::move (step));
  }

  // Expects mutex_ held. Fails, or records the message and completes; true
  // when the send was accepted (the observer, if any, is returned with a
  // copy of the message to be called outside the lock).
  bool
  take_send (message_t message, status_handler_t handler,
             observer_ptr_t &observer, message_t &seen)
  {
    if (send_error_)
      {
        std::error_code const error = send_error_;
        send_error_ = std::error_code ();
        complete (std::move (handler), error);
        return false;
      }
    if (delayed_send_error_)
      {
        if (sends_before_failure_ == 0)
          {
            std::error_code const error = delayed_send_error_;
            delayed_send_error_ = std::error_code ();
            complete (std::move (handler), error);
            return false;
          }
        --sends_before_failure_;
      }
    if (observer_)
      {
        observer = observer_;
        seen = message;
      }
    sent_.push_back (std::move (message));
    complete (std::move (handler), std::error_code ());
    return true;
  }

  void
  end_pending_sends (transport_errc reason)
  {
    while (!sends_.empty ())
      {
        status_handler_t handler = std::move (sends_.front ().handler);
        sends_.pop_front ();
        complete (std::move (handler), make_error_code (reason));
      }
  }

  // Expects mutex_ held: serves the pending receives in the order they
  // started while there is something to give them.
  void
  serve_receives ()
  {
    while (!receives_.empty ())
      {
        if (!incoming_.empty ())
          {
            message_t message = std::move (incoming_.front ());
            incoming_.pop_front ();
            datagram::message_handler_t handler
                = std::move (receives_.front ());
            receives_.pop_front ();
            complete (std::move (handler), std::error_code (),
                      std::move (message));
            continue;
          }
        if (disconnected_)
          end_pending_receives (transport_errc::disconnected);
        return;
      }
  }

  void
  end_pending_open ()
  {
    if (!has_pending_open_)
      return;
    has_pending_open_ = false;
    complete (std::move (pending_open_),
              make_error_code (transport_errc::cancelled));
  }

  void
  end_pending_receives (transport_errc reason)
  {
    while (!receives_.empty ())
      {
        datagram::message_handler_t handler = std::move (receives_.front ());
        receives_.pop_front ();
        complete (std::move (handler), make_error_code (reason), message_t ());
      }
  }

  mutable std::mutex mutex_;
  timing::timer_source timers_;
  std::atomic<bool> script_enlisted_;
  std::deque<message_t> incoming_;
  std::vector<message_t> sent_;
  std::deque<datagram::message_handler_t> receives_;
  scheduled_t scheduled_;
  observer_ptr_t observer_;
  std::deque<pending_send_t> sends_;
  bool hold_sends_ = false;
  status_handler_t pending_open_;
  bool has_pending_open_ = false;
  bool hold_open_ = false;
  bool disconnected_ = false;
  std::error_code open_error_;
  std::error_code open_fail_error_;
  timing::time_point_t open_fail_until_;
  std::error_code receive_error_;
  std::error_code send_error_;
  std::error_code delayed_send_error_;
  std::size_t sends_before_failure_ = 0;
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

void
mock_datagram_control::on_send (send_observer_t observer)
{
  impl_->on_send (std::move (observer));
}

void
mock_datagram_control::deliver_after (std::chrono::milliseconds delay,
                                      message_t const &message)
{
  impl_->deliver_after (delay, message);
}

void
mock_datagram_control::disconnect_after (std::chrono::milliseconds delay)
{
  impl_->disconnect_after (delay);
}

void
mock_datagram_control::fail_opens_for (std::chrono::milliseconds duration,
                                       std::error_code error)
{
  impl_->fail_opens_for (duration, error);
}

void
mock_datagram_control::hold_sends ()
{
  impl_->hold_sends ();
}

void
mock_datagram_control::release_sends ()
{
  impl_->release_sends ();
}

void
mock_datagram_control::hold_next_open ()
{
  impl_->hold_next_open ();
}

void
mock_datagram_control::fail_send_after (std::size_t sends,
                                        std::error_code error)
{
  impl_->fail_send_after (sends, error);
}

mock_datagram
make_mock_datagram (executor_type const &executor,
                    mock_options_t const &options)
{
  std::shared_ptr<mock_datagram_impl> const impl
      = std::make_shared<mock_datagram_impl> (executor, options);
  mock_datagram result
      = { datagram::message_transport (impl), mock_datagram_control (impl) };
  return result;
}

mock_datagram
make_mock_datagram (executor_type const &executor, std::string const &name)
{
  mock_options_t options;
  options.name = name;
  return make_mock_datagram (executor, options);
}
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
