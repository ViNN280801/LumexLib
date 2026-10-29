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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexAsioErrors.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"

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
namespace timing
{
namespace
{
// The steady clock's time on the seam's clock, and back. The end of the
// range maps onto the end of the range.
std::chrono::steady_clock::time_point
to_steady (time_point_t at) LUMEX_NOEXCEPT
{
  if (at == time_point_t::max ())
    return std::chrono::steady_clock::time_point::max ();
  return std::chrono::steady_clock::time_point (
      std::chrono::duration_cast<std::chrono::steady_clock::duration> (
          at.time_since_epoch ()));
}

/**
 * The completion of an `asio::steady_timer` wait: translates Asio's code and
 * hands the result to the erased handler on that handler's executor. Neither
 * the executor of the handler (Asio would keep work on it while the wait is
 * pending) nor its allocator (it may be the view of the erased handler) nor
 * its cancellation slot is forwarded: the timer serves the slot itself (see
 * `steady_canceller`).
 */
struct steady_completion
{
  executor_type executor;
  wait_handler_t handler;

  void
  operator() (std::error_code const &error)
  {
    // On the timer's executor already: a handler without an executor of its
    // own runs here at once, one bound elsewhere is handed over.
    asio::dispatch (
        executor,
        asio::append (std::move (handler),
                      ::lumex::applied::transport::detail::from_asio (error)));
  }
};

class steady_timer_impl;

/**
 * The slot handler of a steady wait: any cancellation type emitted while the
 * wait is pending ends it with `cancelled`. It holds the timer weakly and
 * names its wait: Asio's own slot handler would point into the destroyed
 * timer when a signal comes after the timer was destroyed (before or after
 * the completion ran), this one reaches nothing then.
 */
struct steady_canceller
{
  steady_canceller (std::weak_ptr<steady_timer_impl> owner,
                    std::uint64_t wait) LUMEX_NOEXCEPT
      : timer (std::move (owner)),
        id (wait)
  {
  }

  void operator() (asio::cancellation_type_t type) LUMEX_NOEXCEPT;

  std::weak_ptr<steady_timer_impl> timer;
  std::uint64_t id;
};

class steady_timer_impl final
    : public timer_impl,
      public std::enable_shared_from_this<steady_timer_impl>
{
public:
  explicit steady_timer_impl (executor_type const &executor)
      : executor_ (executor), timer_ (executor), wait_id_ (0)
  {
  }

  executor_type
  get_executor () const override
  {
    return executor_;
  }

  time_point_t
  now () const override
  {
    return timer_clock::now ();
  }

  void
  start_wait (time_point_t at, wait_handler_t handler) override
  {
    std::uint64_t const id = ++wait_id_;
    asio::cancellation_slot slot = handler.get_cancellation_slot ();
    if (slot.is_connected ())
      slot.template emplace<steady_canceller> (
          std::weak_ptr<steady_timer_impl> (shared_from_this ()), id);
    // A new expiry ends the pending wait with operation_aborted (cancelled).
    timer_.expires_at (to_steady (at));
    steady_completion completion = { executor_, std::move (handler) };
    timer_.async_wait (std::move (completion));
  }

  std::size_t
  cancel () LUMEX_NOEXCEPT override
  {
    try
      {
        return timer_.cancel ();
      }
    catch (...)
      {
        return 0;
      }
  }

  /// The slot of the wait `id` was signalled (on the timer's strand): it
  /// ends that wait, a later one is not touched. A wait that already ended
  /// leaves nothing to cancel.
  void
  cancel_wait (std::uint64_t id) LUMEX_NOEXCEPT
  {
    if (id == wait_id_)
      cancel ();
  }

private:
  executor_type executor_;
  asio::steady_timer timer_;
  // The strand only: the number of the latest wait.
  std::uint64_t wait_id_;
};

void
steady_canceller::operator() (asio::cancellation_type_t type) LUMEX_NOEXCEPT
{
  if ((type
       & (asio::cancellation_type::terminal | asio::cancellation_type::partial
          | asio::cancellation_type::total))
      == asio::cancellation_type::none)
    return;
  if (std::shared_ptr<steady_timer_impl> const alive = timer.lock ())
    alive->cancel_wait (id);
}

class steady_source_impl final : public timer_source_impl
{
public:
  time_point_t
  now () const override
  {
    return timer_clock::now ();
  }

  std::shared_ptr<timer_impl>
  make_timer (executor_type const &executor) const override
  {
    return std::make_shared<steady_timer_impl> (executor);
  }
};

std::shared_ptr<timer_source_impl> const &
steady_impl ()
{
  static std::shared_ptr<timer_source_impl> const impl
      = std::make_shared<steady_source_impl> ();
  return impl;
}
} // namespace

timer_clock::time_point
timer_clock::now () LUMEX_NOEXCEPT
{
  return time_point (std::chrono::duration_cast<duration> (
      std::chrono::steady_clock::now ().time_since_epoch ()));
}

timer_impl::~timer_impl () {}

timer_source_impl::~timer_source_impl () {}

timer::timer () LUMEX_NOEXCEPT {}

timer::timer (std::shared_ptr<timer_impl> impl) LUMEX_NOEXCEPT
    : impl_ (std::move (impl))
{
}

timer::~timer () { cancel (); }

timer::timer (timer &&other) LUMEX_NOEXCEPT : impl_ (std::move (other.impl_))
{
}

timer &
timer::operator= (timer &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      cancel ();
      impl_ = std::move (other.impl_);
    }
  return *this;
}

timer::executor_type
timer::get_executor () const
{
  return impl_ ? impl_->get_executor () : executor_type ();
}

time_point_t
timer::now () const
{
  return impl_ ? impl_->now () : timer_clock::now ();
}

std::size_t
timer::cancel () LUMEX_NOEXCEPT
{
  return impl_ ? impl_->cancel () : 0;
}

timer_source::timer_source () : impl_ (steady_impl ()) {}

timer_source::timer_source (std::shared_ptr<timer_source_impl> impl)
    : impl_ (impl ? std::move (impl) : steady_impl ())
{
}

time_point_t
timer_source::now () const
{
  return impl_->now ();
}

timer
timer_source::make_timer (executor_type const &executor) const
{
  return timer (impl_->make_timer (executor));
}

timer_source
steady_timer_source ()
{
  return timer_source ();
}
} // namespace timing

namespace detail
{
timing::time_point_t
saturating_add (timing::time_point_t from,
                std::chrono::milliseconds delay) LUMEX_NOEXCEPT
{
  using timing::time_point_t;
  if (delay.count () <= 0)
    return from;
  // The longest delay the duration of the clock holds: a longer one passes
  // the end of the range from any point.
  std::chrono::milliseconds const longest
      = std::chrono::duration_cast<std::chrono::milliseconds> (
          time_point_t::duration::max ());
  if (delay >= longest)
    return time_point_t::max ();
  time_point_t::duration const step
      = std::chrono::duration_cast<time_point_t::duration> (delay);
  // Before the epoch the sum cannot pass the end (step fits the duration);
  // from the epoch on, the room left is computed without overflow.
  if (from.time_since_epoch ().count () >= 0
      && step >= time_point_t::max () - from)
    return time_point_t::max ();
  return from + step;
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
