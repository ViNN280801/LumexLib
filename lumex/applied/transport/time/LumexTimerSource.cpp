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
#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexAsioErrors.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
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
namespace time
{
namespace
{
/**
 * The completion of an `asio::steady_timer` wait: translates Asio's code and
 * hands the result to the erased handler on that handler's executor. Only
 * the cancellation slot of the handler is forwarded (the timer's own
 * per-operation cancellation then serves it); its executor is not, so Asio
 * keeps no work on it while the wait is pending, and its allocator is not
 * (it may be the view of the erased handler).
 */
struct steady_completion
{
  executor_type executor;
  wait_handler_t handler;

  using cancellation_slot_type = asio::cancellation_slot;

  cancellation_slot_type
  get_cancellation_slot () const LUMEX_NOEXCEPT
  {
    return handler.get_cancellation_slot ();
  }

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

class steady_timer_impl final : public timer_impl
{
public:
  explicit steady_timer_impl (executor_type const &executor)
      : executor_ (executor), timer_ (executor)
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
    return clock_type::now ();
  }

  void
  start_wait (time_point_t at, wait_handler_t handler) override
  {
    // A new expiry ends the pending wait with operation_aborted (cancelled).
    timer_.expires_at (at);
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

private:
  executor_type executor_;
  asio::steady_timer timer_;
};

class steady_source_impl final : public timer_source_impl
{
public:
  time_point_t
  now () const override
  {
    return clock_type::now ();
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

time_point_t
saturating_add (time_point_t from,
                std::chrono::milliseconds delay) LUMEX_NOEXCEPT
{
  if (delay.count () <= 0)
    return from;
  // The room left before the end of the range, in whole milliseconds.
  std::chrono::milliseconds const room
      = std::chrono::duration_cast<std::chrono::milliseconds> (
          time_point_t::max () - from);
  if (delay >= room)
    return time_point_t::max ();
  return from + delay;
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
  return impl_ ? impl_->now () : clock_type::now ();
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
} // namespace time
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
