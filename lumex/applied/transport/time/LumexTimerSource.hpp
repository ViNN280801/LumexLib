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

/**
 * @file LumexTimerSource.hpp
 * @brief `timer_source` and `timer`: the seam through which the sessions and
 * the `device_watcher` read the time and wait for it, so that a test can run
 * them on a virtual clock (`testing::virtual_clock`) instead of real time.
 * @details A `timer_source` is a small handle (a shared pointer) to an
 * implementation that tells the time (`now ()`) and makes timers. The default
 * one, `steady_timer_source ()`, reads `std::chrono::steady_clock` and waits
 * with `asio::steady_timer`; the virtual clock of `testing/` keeps its own
 * time, which moves only when the test advances it. Code that takes a
 * `timer_source` in its options (`session::session_options_t::timers`,
 * `session::device_watcher_options_t::timers`) behaves the same on both.
 *
 * A `timer` is bound to the executor it was made for (a strand, as a rule):
 * a wait completes on the associated executor of its handler, by default that
 * executor, never inside the call that starts it, inside `cancel ()` or
 * inside the advance of a virtual clock. It is one-shot and re-armable: a new
 * wait ends the pending one with `transport_errc::cancelled`, as a new expiry
 * of an `asio::steady_timer` does. Like an Asio timer, one `timer` object is
 * not safe for concurrent use: start, cancel and destroy it on its strand (or
 * from one thread at a time).
 *
 * A wait ends with an empty error when its time came and with
 * `transport_errc::cancelled` when `cancel ()`, a new wait or the destruction
 * of the timer ended it (never with Asio's `operation_aborted`: see
 * `detail/LumexAsioErrors.hpp`). Per-operation cancellation works as on an
 * `asio::steady_timer`: a `terminal`, `partial` or `total` cancellation
 * emitted on the handler's cancellation slot (`asio::cancel_after`,
 * `asio::bind_cancellation_slot`, a coroutine) while the wait is pending ends
 * that wait with `cancelled`; emit it on the timer's strand, as Asio requires.
 *
 * Destroying a timer with a pending wait ends the wait with `cancelled`
 * (posted, the destructor never blocks). When the `io_context` is destroyed
 * first, the pending handler is destroyed without being called, as Asio
 * destroys the handlers it queues. The steady timer follows the lifetime
 * rule of `asio::steady_timer` (destroy it before its `io_context`); the
 * virtual timer may also outlive it (see `testing/LumexVirtualClock.hpp`).
 *
 * The timers keep no outstanding work on the executor associated with a
 * handler. A pending steady wait keeps its own `io_context` running, as an
 * Asio timer does; a pending virtual wait keeps no work at all.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TIME_TIMER_SOURCE_HPP
#define LUMEX_APPLIED_TRANSPORT_TIME_TIMER_SOURCE_HPP

#include <chrono>
#include <cstddef>
#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexErasedHandlers.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace time
{
/// @brief The clock whose time points every timer source uses (a virtual
/// clock keeps its own epoch in the same type).
using clock_type = std::chrono::steady_clock;

/// @brief A point in time of a timer source.
using time_point_t = clock_type::time_point;

/// @brief The completion of a wait: empty when the time came,
/// `transport_errc::cancelled` when the wait was ended before.
using wait_signature_t = void (std::error_code);

/// @brief The handler of a wait as an implementation receives it (erased
/// once).
using wait_handler_t = asio::any_completion_handler<wait_signature_t>;

/// @brief `from + delay`, kept within the range of `time_point_t` (a delay
/// that would pass its end gives `time_point_t::max ()`, a negative one
/// `from` itself).
time_point_t saturating_add (time_point_t from,
                             std::chrono::milliseconds delay) LUMEX_NOEXCEPT;

/**
 * @brief The implementation of one timer, made by a `timer_source_impl`.
 * @details `start_wait` and `cancel` are called on the timer's strand (see
 * the file description); an implementation must stay safe against its own
 * clock moving on another thread.
 */
class timer_impl
{
public:
  timer_impl () = default;
  virtual ~timer_impl ();

  timer_impl (timer_impl const &) = delete;
  timer_impl &operator= (timer_impl const &) = delete;

  /// @brief The executor the timer was made for; empty once a virtual
  /// timer's `io_context` was destroyed.
  virtual executor_type get_executor () const = 0;

  /// @brief The time of the timer's source now.
  virtual time_point_t now () const = 0;

  /**
   * @brief Waits until `at` (a time already past ends the wait soon after:
   * at once for the steady timer, at the next advance of a virtual clock).
   * @details Ends the pending wait, if any, with `cancelled`. `handler` is
   * never called inside this call; it runs on its associated executor, by
   * default the timer's.
   */
  virtual void start_wait (time_point_t at, wait_handler_t handler) = 0;

  /// @brief Ends the pending wait with `cancelled`; returns how many waits
  /// it ended (0 or 1).
  virtual std::size_t cancel () LUMEX_NOEXCEPT = 0;
};

/// @brief The implementation behind a `timer_source`.
class timer_source_impl
{
public:
  timer_source_impl () = default;
  virtual ~timer_source_impl ();

  timer_source_impl (timer_source_impl const &) = delete;
  timer_source_impl &operator= (timer_source_impl const &) = delete;

  /// @brief The time now (any thread).
  virtual time_point_t now () const = 0;

  /// @brief A new timer bound to `executor` (any thread).
  virtual std::shared_ptr<timer_impl>
  make_timer (executor_type const &executor) const = 0;
};

/**
 * @brief A one-shot, re-armable timer of a `timer_source` (see the file
 * description).
 * @details Moved from (or default constructed) it is empty: a wait ends
 * with `cancelled` on the handler's associated executor (the system executor
 * when it has none) and `cancel ()` returns 0; do not start such a wait with
 * `asio::cancel_after`, which needs the timer's executor.
 */
class timer
{
  // The initiating functions (declared first: the return types of the
  // async_* members name them). Each carries the timer's executor, which
  // tokens such as asio::cancel_after ask for: their own timer then runs on
  // the strand, never beside the wait.
  struct initiate_base
  {
    using executor_type = transport::executor_type;

    explicit initiate_base (executor_type const &executor)
        : executor_ (executor)
    {
    }

    executor_type
    get_executor () const LUMEX_NOEXCEPT
    {
      return executor_;
    }

  protected:
    // The wait of `impl` until `at`, or `cancelled` without one.
    template <typename Handler>
    static void
    start (Handler &&handler, std::shared_ptr<timer_impl> const &impl,
           time_point_t at)
    {
      if (!impl)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::cancelled));
          return;
        }
      impl->start_wait (
          at, ::lumex::applied::transport::detail::erase_once<wait_handler_t> (
                  std::forward<Handler> (handler),
                  ::lumex::applied::transport::detail::operation_mark_ptr ()));
    }

  private:
    executor_type executor_;
  };

  struct initiate_wait_until : initiate_base
  {
    explicit initiate_wait_until (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<timer_impl> const &impl,
                time_point_t at) const
    {
      start (std::forward<Handler> (handler), impl, at);
    }
  };

  // The deadline is taken when the wait starts (a deferred wait launched
  // later waits the whole delay from then).
  struct initiate_wait_for : initiate_base
  {
    explicit initiate_wait_for (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<timer_impl> const &impl,
                std::chrono::milliseconds delay) const
    {
      time_point_t const at
          = impl ? saturating_add (impl->now (), delay) : time_point_t ();
      start (std::forward<Handler> (handler), impl, at);
    }
  };

public:
  /// @brief The executor type the initiating functions report.
  using executor_type = transport::executor_type;

  /// @brief An empty timer (see the class description).
  timer () LUMEX_NOEXCEPT;

  /// @brief A timer over `impl` (null: empty).
  explicit timer (std::shared_ptr<timer_impl> impl) LUMEX_NOEXCEPT;

  /// @brief Ends the pending wait with `cancelled` and returns.
  ~timer ();

  timer (timer const &) = delete;
  timer &operator= (timer const &) = delete;
  timer (timer &&other) LUMEX_NOEXCEPT;
  /// @brief Ends the pending wait of this timer with `cancelled`, then takes
  /// `other`'s.
  timer &operator= (timer &&other) LUMEX_NOEXCEPT;

  /// @brief The executor the timer was made for (empty for an empty timer).
  executor_type get_executor () const;

  /// @brief The time of the timer's source (`clock_type::now ()` for an
  /// empty timer).
  time_point_t now () const;

  /// @brief Waits until `at`; the token gets `void (std::error_code)`.
  template <typename Token>
  auto
  async_wait_until (time_point_t at, Token &&token)
      -> decltype (asio::async_initiate<Token, wait_signature_t> (
          std::declval<initiate_wait_until> (), token,
          std::declval<std::shared_ptr<timer_impl> &> (), at))
  {
    return asio::async_initiate<Token, wait_signature_t> (
        initiate_wait_until (get_executor ()), token, impl_, at);
  }

  /// @brief Waits `delay` from the start of the wait (a negative delay as
  /// 0); the token gets `void (std::error_code)`.
  template <typename Token>
  auto
  async_wait_for (std::chrono::milliseconds delay, Token &&token)
      -> decltype (asio::async_initiate<Token, wait_signature_t> (
          std::declval<initiate_wait_for> (), token,
          std::declval<std::shared_ptr<timer_impl> &> (), delay))
  {
    return asio::async_initiate<Token, wait_signature_t> (
        initiate_wait_for (get_executor ()), token, impl_, delay);
  }

  /// @brief Ends the pending wait with `cancelled`; returns how many waits
  /// it ended (0 or 1).
  std::size_t cancel () LUMEX_NOEXCEPT;

  /// @brief The implementation, for the library's own adapters.
  std::shared_ptr<timer_impl> const &
  impl () const LUMEX_NOEXCEPT
  {
    return impl_;
  }

private:
  std::shared_ptr<timer_impl> impl_;
};

/**
 * @brief Tells the time and makes timers (see the file description).
 * @details A handle: copies share one implementation and compare equal.
 * Never empty: the default and a null implementation give the steady source.
 */
class timer_source
{
public:
  /// @brief The steady source (`steady_timer_source ()`).
  timer_source ();

  /// @brief A source over `impl` (null: the steady source).
  explicit timer_source (std::shared_ptr<timer_source_impl> impl);

  /// @brief The time now (any thread).
  time_point_t now () const;

  /// @brief A new timer bound to `executor` (any thread; pass the strand the
  /// timer is used on).
  timer make_timer (executor_type const &executor) const;

  /// @brief The implementation.
  std::shared_ptr<timer_source_impl> const &
  impl () const LUMEX_NOEXCEPT
  {
    return impl_;
  }

  /// @brief True when both use the same implementation.
  friend bool
  operator== (timer_source const &left,
              timer_source const &right) LUMEX_NOEXCEPT
  {
    return left.impl_ == right.impl_;
  }

  /// @brief True when they use different implementations.
  friend bool
  operator!= (timer_source const &left,
              timer_source const &right) LUMEX_NOEXCEPT
  {
    return !(left == right);
  }

private:
  std::shared_ptr<timer_source_impl> impl_;
};

/// @brief The real-time source: `clock_type::now ()` and
/// `asio::steady_timer` (one implementation shared by every copy).
timer_source steady_timer_source ();
} // namespace time
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TIME_TIMER_SOURCE_HPP
