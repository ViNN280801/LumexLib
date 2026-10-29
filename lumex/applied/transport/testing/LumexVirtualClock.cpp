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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/applied/transport/testing/LumexVirtualClock.hpp"
#include "lumex/applied/transport/time/LumexTimerSource.hpp"

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
namespace
{
using time::time_point_t;
using time::wait_handler_t;

class virtual_timer_impl;

// The place of a wait in the queue: its time, then the order of the starts.
struct queue_key
{
  time_point_t at;
  std::uint64_t sequence;

  bool
  operator< (queue_key const &other) const LUMEX_NOEXCEPT
  {
    if (at != other.at)
      return at < other.at;
    return sequence < other.sequence;
  }
};

// A wait taken out of the queue, to be completed outside the lock.
struct taken_wait
{
  executor_type executor;
  wait_handler_t handler;
};

// Posts the result of each wait to its timer's executor, in order. A wait
// without an executor (its io_context is gone) or whose post fails (out of
// memory) dies uncalled with the vector.
void
complete_all (std::vector<taken_wait> &waits,
              std::error_code const &error) LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < waits.size (); ++i)
    {
      if (!waits[i].handler || !waits[i].executor)
        continue;
      try
        {
          asio::post (waits[i].executor,
                      asio::append (std::move (waits[i].handler), error));
        }
      catch (...)
        {
        }
    }
}

std::error_code
cancelled_code () LUMEX_NOEXCEPT
{
  return make_error_code (error::transport_errc::cancelled);
}
} // namespace

/// The shared state of a clock: its time and the queue of the pending waits
/// of every timer made from it. The mutex also guards the wait of each
/// timer, so that the queue and the timers never disagree. Timers and
/// sources hold it, so it outlives the `virtual_clock` object.
class virtual_clock_impl
{
public:
  explicit virtual_clock_impl (time_point_t start)
      : now_ (start), next_sequence_ (0), alive_ (true)
  {
  }

  std::mutex mutex;
  time_point_t now_;
  std::uint64_t next_sequence_;
  // False once the virtual_clock object was destroyed.
  bool alive_;
  std::map<queue_key, virtual_timer_impl *> queue_;

  // Moves the time to `target` (never back) and takes out every wait due by
  // then, in order; with `to_next` the target is the earliest wait (or the
  // time now when it is due already).
  std::size_t fire (time_point_t target, bool to_next);

  // The clock is destroyed: every pending wait is taken out.
  void close () LUMEX_NOEXCEPT;
};

namespace
{
/**
 * One timer of a virtual clock. Its wait (handler, executor, place in the
 * queue) is guarded by the clock's mutex. It is a handler keeper: the
 * shutdown of the io_context of its executor destroys the pending handler
 * uncalled and forgets the executor (a strand must not outlive its
 * context), after which every wait is dropped the same way.
 */
class virtual_timer_impl final
    : public time::timer_impl,
      public ::lumex::applied::transport::detail::handler_keeper,
      public std::enable_shared_from_this<virtual_timer_impl>
{
public:
  virtual_timer_impl (std::shared_ptr<virtual_clock_impl> clock,
                      executor_type const &executor)
      : clock_ (std::move (clock)), executor_ (executor), enlisted_ (false),
        next_wait_id_ (0), pending_ (false), shut_down_ (false),
        key_ (queue_key ()), wait_id_ (0)
  {
  }

  ~virtual_timer_impl () override
  {
    // Only a wait started through a copy of the implementation (a deferred
    // wait launched after the timer was destroyed) can still be pending.
    std::vector<taken_wait> taken (1);
    {
      std::lock_guard<std::mutex> const lock (clock_->mutex);
      if (pending_)
        take_locked (taken[0]);
    }
    complete_all (taken, cancelled_code ());
  }

  executor_type
  get_executor () const override
  {
    std::lock_guard<std::mutex> const lock (clock_->mutex);
    return executor_;
  }

  time_point_t
  now () const override
  {
    std::lock_guard<std::mutex> const lock (clock_->mutex);
    return clock_->now_;
  }

  void start_wait (time_point_t at, wait_handler_t handler) override;

  std::size_t
  cancel () LUMEX_NOEXCEPT override
  {
    return cancel_wait (0);
  }

  /// Ends the pending wait if `id` names it (0: whatever is pending).
  std::size_t
  cancel_wait (std::uint64_t id) LUMEX_NOEXCEPT
  {
    try
      {
        std::vector<taken_wait> taken (1);
        {
          std::lock_guard<std::mutex> const lock (clock_->mutex);
          if (!pending_ || (id != 0 && id != wait_id_))
            return 0;
          take_locked (taken[0]);
        }
        complete_all (taken, cancelled_code ());
        return 1;
      }
    catch (...)
      {
        return 0;
      }
  }

  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    // Destroyed after the lock is released: the handler may own the owner
    // of this timer.
    taken_wait dropped;
    try
      {
        std::lock_guard<std::mutex> const lock (clock_->mutex);
        shut_down_ = true;
        if (pending_)
          take_locked (dropped);
        dropped.executor = std::move (executor_);
        executor_ = executor_type ();
      }
    catch (...)
      {
      }
  }

  /// Under the clock's lock: takes the pending wait out of the queue.
  void
  take_locked (taken_wait &out) LUMEX_NOEXCEPT
  {
    clock_->queue_.erase (key_);
    out.executor = executor_;
    out.handler = std::move (handler_);
    pending_ = false;
  }

private:
  std::shared_ptr<virtual_clock_impl> clock_;
  executor_type executor_;
  std::atomic<bool> enlisted_;
  std::atomic<std::uint64_t> next_wait_id_;
  // Under the clock's lock.
  bool pending_;
  bool shut_down_;
  queue_key key_;
  std::uint64_t wait_id_;
  wait_handler_t handler_;
};

/**
 * The slot handler of a wait: any cancellation type emitted while the wait
 * is pending ends it with `cancelled` (as on an asio::steady_timer). It
 * holds the timer weakly and names its wait, so a signal emitted after the
 * wait ended (the slot stays where Asio's own operations leave it) reaches
 * nothing.
 */
struct wait_canceller
{
  wait_canceller (std::weak_ptr<virtual_timer_impl> owner,
                  std::uint64_t wait) LUMEX_NOEXCEPT
      : timer (std::move (owner)),
        id (wait)
  {
  }

  void
  operator() (asio::cancellation_type_t type) LUMEX_NOEXCEPT
  {
    if ((type
         & (asio::cancellation_type::terminal
            | asio::cancellation_type::partial
            | asio::cancellation_type::total))
        == asio::cancellation_type::none)
      return;
    if (std::shared_ptr<virtual_timer_impl> const alive = timer.lock ())
      alive->cancel_wait (id);
  }

  std::weak_ptr<virtual_timer_impl> timer;
  std::uint64_t id;
};

void
virtual_timer_impl::start_wait (time_point_t at, wait_handler_t handler)
{
  executor_type executor;
  {
    std::lock_guard<std::mutex> const lock (clock_->mutex);
    executor = executor_;
  }
  // The io_context is gone: the wait is dropped, as Asio drops what is
  // started on a context that shut down.
  if (!executor)
    return;
  ::lumex::applied::transport::detail::enlist_keeper (
      enlisted_, executor,
      std::static_pointer_cast<
          ::lumex::applied::transport::detail::handler_keeper> (
          shared_from_this ()));
  std::uint64_t const id
      = next_wait_id_.fetch_add (1, std::memory_order_relaxed) + 1;
  asio::cancellation_slot slot = handler.get_cancellation_slot ();
  if (slot.is_connected ())
    slot.template emplace<wait_canceller> (
        std::weak_ptr<virtual_timer_impl> (shared_from_this ()), id);

  // [0]: the wait this one replaces; [1]: this one when the clock is gone.
  std::vector<taken_wait> taken (2);
  {
    std::lock_guard<std::mutex> const lock (clock_->mutex);
    if (shut_down_)
      {
        taken[1].handler = std::move (handler);
        taken[1].executor = executor_type ();
      }
    else
      {
        if (pending_)
          take_locked (taken[0]);
        if (!clock_->alive_)
          {
            taken[1].executor = executor_;
            taken[1].handler = std::move (handler);
          }
        else
          {
            queue_key const key = { at, clock_->next_sequence_ };
            clock_->queue_.insert (std::make_pair (key, this));
            ++clock_->next_sequence_;
            key_ = key;
            wait_id_ = id;
            handler_ = std::move (handler);
            pending_ = true;
          }
      }
  }
  complete_all (taken, cancelled_code ());
}

/// The source of one clock.
class virtual_source_impl final : public time::timer_source_impl
{
public:
  explicit virtual_source_impl (std::shared_ptr<virtual_clock_impl> clock)
      : clock_ (std::move (clock))
  {
  }

  time_point_t
  now () const override
  {
    std::lock_guard<std::mutex> const lock (clock_->mutex);
    return clock_->now_;
  }

  std::shared_ptr<time::timer_impl>
  make_timer (executor_type const &executor) const override
  {
    return std::make_shared<virtual_timer_impl> (clock_, executor);
  }

private:
  std::shared_ptr<virtual_clock_impl> clock_;
};

// Runs every ready handler of `context`, restarting it when it ran out of
// work.
void
drain (asio::io_context &context)
{
  for (;;)
    {
      if (context.stopped ())
        context.restart ();
      if (context.poll () == 0)
        return;
    }
}
} // namespace

std::size_t
virtual_clock_impl::fire (time_point_t target, bool to_next)
{
  std::vector<taken_wait> due;
  {
    std::lock_guard<std::mutex> const lock (mutex);
    if (to_next)
      {
        if (queue_.empty ())
          return 0;
        target = std::max (now_, queue_.begin ()->first.at);
      }
    while (!queue_.empty () && !(target < queue_.begin ()->first.at))
      {
        due.push_back (taken_wait ());
        queue_.begin ()->second->take_locked (due.back ());
      }
    if (now_ < target)
      now_ = target;
  }
  complete_all (due, std::error_code ());
  return due.size ();
}

void
virtual_clock_impl::close () LUMEX_NOEXCEPT
{
  try
    {
      std::vector<taken_wait> taken;
      {
        std::lock_guard<std::mutex> const lock (mutex);
        alive_ = false;
        while (!queue_.empty ())
          {
            taken.push_back (taken_wait ());
            queue_.begin ()->second->take_locked (taken.back ());
          }
      }
      complete_all (taken, cancelled_code ());
    }
  catch (...)
    {
    }
}

virtual_clock::virtual_clock (time::time_point_t start)
    : impl_ (std::make_shared<virtual_clock_impl> (start)),
      source_ (std::make_shared<virtual_source_impl> (impl_))
{
}

virtual_clock::~virtual_clock () { impl_->close (); }

time::timer_source
virtual_clock::source () const
{
  return source_;
}

time::time_point_t
virtual_clock::now () const
{
  std::lock_guard<std::mutex> const lock (impl_->mutex);
  return impl_->now_;
}

std::size_t
virtual_clock::advance (std::chrono::milliseconds by)
{
  return impl_->fire (time::saturating_add (now (), by), false);
}

std::size_t
virtual_clock::advance_to_next ()
{
  return impl_->fire (time_point_t (), true);
}

std::size_t
virtual_clock::pending () const
{
  std::lock_guard<std::mutex> const lock (impl_->mutex);
  return impl_->queue_.size ();
}

time::time_point_t
virtual_clock::next_expiry () const
{
  std::lock_guard<std::mutex> const lock (impl_->mutex);
  return impl_->queue_.empty () ? time_point_t::max ()
                                : impl_->queue_.begin ()->first.at;
}

bool
run_until (asio::io_context &context, virtual_clock &clock,
           std::function<bool ()> const &done,
           std::chrono::milliseconds budget)
{
  time_point_t const end = time::saturating_add (clock.now (), budget);
  for (;;)
    {
      drain (context);
      if (done ())
        return true;
      // The waits due now first (started for the time now or before it).
      if (clock.impl_->fire (clock.now (), false) > 0)
        continue;
      // Nothing pending, or the next wait lies beyond the budget: the rest
      // of the budget passes.
      time_point_t const next = clock.next_expiry ();
      if (next == time_point_t::max () || end < next)
        {
          clock.impl_->fire (end, false);
          drain (context);
          return done ();
        }
      clock.impl_->fire (time_point_t (), true);
    }
}
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
