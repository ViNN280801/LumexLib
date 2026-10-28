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
 * @file LumexBasicSession.hpp
 * @brief `basic_session<Transport>`: what `stream_session` and
 * `message_session` share: start, open, close, cancel, the state, its
 * notifications, the device presence and the counters.
 * @details A session owns a factory and the transport it made last. Every
 * part of it runs on its own strand of the given executor; each member may be
 * called from any thread. `start ()` (or `async_open`, `open`) begins a cycle:
 * the factory makes a transport and the session opens it; a failed attempt
 * (the factory threw, the open failed or took longer than `open_timeout`)
 * is retried per `reconnect_policy_t`, and `max_attempts` failed attempts in
 * a row end in `faulted`. While open, an I/O error of the session that
 * `is_link_lost` accepts, or `set_device_present (false)`, drops the
 * transport and starts `reconnecting`: at once when the link was
 * established, after the delay of its attempt when it went right after its
 * open (see `reconnect_policy_t`). The link is seen through the session's
 * own I/O: a session nobody reads from notices a silent loss only through
 * the device presence.
 *
 * `set_device_present (false)` (a `device_watcher` calls it through
 * `presence_sink ()`) holds the attempts until the device is back:
 * `set_device_present (true)` makes the next attempt at once.
 *
 * `close ()` and `cancel ()` end connecting and reconnecting at once: no
 * attempt starts and no transport is opened after they return (a factory
 * already running finishes, and its transport is dropped unopened).
 * `close ()` also closes the transport and sets `closed`; `cancel ()` of an
 * open session cancels its pending I/O and keeps it open (a link loss seen
 * meanwhile still starts reconnecting). Both end the pending
 * `async_wait_state_change` and `async_open` with `cancelled`.
 *
 * The posted steps and timers of a session keep its implementation alive
 * (the handlers it gives its transport hold it weakly), so a session may be
 * destroyed at any time, also inside its state callback or a completion
 * handler: the destructor closes (asynchronously) and returns, and the state
 * callback is not called after it (a call already running on another thread
 * may still finish). Destroy a session before the `io_context` of its
 * executor; one destroyed after the `io_context` stopped is released with
 * it.
 *
 * Per-operation cancellation, as for the transports: a
 * `cancellation_type::terminal` emitted on the cancellation slot of the
 * handler of `async_open` or `async_wait_state_change` while it is pending
 * does what `cancel ()` does (connecting and reconnecting end in `closed`;
 * an open session keeps its link and cancels its I/O; every pending wait
 * ends with `cancelled`). One emitted on the slot of an I/O operation
 * reaches the current transport, which cancels its pending operations as
 * its `cancel ()` does; the session keeps its link. `partial` and `total`
 * alone change nothing. A signal emitted after the result was handed on
 * has no effect, also while the handler still waits on its executor (for
 * the I/O the transport decides, as for its own operations). One
 * emitted right after the call, before the session reached its
 * transport, still cancels (the session takes the slot at the initiation,
 * as the transports and Asio's own operations do). The initiations report the
 * session strand, so the timer of `asio::cancel_after` runs there; bind a
 * handler that would run on the bare executor of a runner with several threads
 * to a strand.
 *
 * The session keeps no outstanding work on a handler's associated executor
 * (as the transports): a handler that keeps its session alive, kept by a
 * pending operation, would keep that work forever and the destructor of its
 * `io_context` would never return. An `io_context` that only runs the
 * handlers (`asio::bind_executor (other, ...)`) may therefore run out of
 * work and return from `run ()` before the handler: keep an
 * `executor_work_guard` on it while an operation is pending. That executor
 * must stay valid until the handler ran or was destroyed.
 *
 * A moved-from session is empty: its members do nothing, `state ()` is
 * `closed`, `open` and the asynchronous operations end with `not_open` (on
 * the handler's associated executor, else the system executor).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_BASIC_SESSION_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_BASIC_SESSION_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexSyncWait.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionCore.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionImpl.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace session
{
/// @brief The part every session shares (see the file description).
template <typename Transport> class basic_session
{
protected:
  typedef detail::session_impl<Transport> impl_type;

  // The initiating functions (declared first: the return types of the
  // async_* members name them). Each reports the session strand, which
  // tokens such as asio::cancel_after ask for: their timer then runs on the
  // strand, as for the transports' initiations.
  struct initiate_base
  {
    typedef ::lumex::applied::transport::executor_type executor_type;

    explicit initiate_base (executor_type const &executor)
        : executor_ (executor)
    {
    }

    executor_type
    get_executor () const LUMEX_NOEXCEPT
    {
      return executor_;
    }

  private:
    executor_type executor_;
  };

  struct initiate_open : initiate_base
  {
    explicit initiate_open (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<impl_type> impl) const
    {
      if (!impl)
        {
          detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open));
          return;
        }
      impl->start_open (0, std::forward<Handler> (handler));
    }
  };

  struct initiate_wait_state_change : initiate_base
  {
    explicit initiate_wait_state_change (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<impl_type> impl,
                channel_state seen) const
    {
      if (!impl)
        {
          detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open),
              channel_state::closed);
          return;
        }
      impl->start_wait_state_change (seen, std::forward<Handler> (handler));
    }
  };

  struct initiate_wait_snapshot_change : initiate_base
  {
    explicit initiate_wait_snapshot_change (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<impl_type> impl,
                state_snapshot_t const &seen) const
    {
      if (!impl)
        {
          detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open),
              state_snapshot_t ());
          return;
        }
      impl->start_wait_snapshot_change (seen, std::forward<Handler> (handler));
    }
  };

public:
  /// @brief Makes a closed transport on the executor it is given (the
  /// session's executor); a factory that throws is a failed attempt.
  typedef typename impl_type::factory_t factory_t;

  /// @brief A closed session; `start ()` or `open` begins.
  basic_session (executor_type const &executor, factory_t factory,
                 session_options_t options = session_options_t ())
      : impl_ (std::make_shared<impl_type> (executor, std::move (factory),
                                            std::move (options)))
  {
  }

  /// @brief Closes (asynchronously) and returns; no state callback runs
  /// after it; pending handlers still run (with `cancelled`).
  ~basic_session () { release (); }

  basic_session (basic_session const &) = delete;
  basic_session &operator= (basic_session const &) = delete;

  basic_session (basic_session &&other) LUMEX_NOEXCEPT
      : impl_ (std::move (other.impl_))
  {
  }

  basic_session &
  operator= (basic_session &&other) LUMEX_NOEXCEPT
  {
    if (this != &other)
      {
        release ();
        impl_ = std::move (other.impl_);
      }
    return *this;
  }

  /// @brief The executor the session was given (an empty one when moved
  /// from).
  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return impl_ ? impl_->executor () : executor_type ();
  }

  /// @brief The name for logs (`session_options_t::name`).
  std::string const &
  name () const LUMEX_NOEXCEPT
  {
    return impl_ ? impl_->name () : detail::empty_session_name ();
  }

  /// @brief The state now.
  channel_state
  state () const LUMEX_NOEXCEPT
  {
    return impl_ ? impl_->state () : channel_state::closed;
  }

  /// @brief The state and the number of its changes so far, read together:
  /// the start of `async_wait_state_change` from a snapshot.
  state_snapshot_t
  state_snapshot () const LUMEX_NOEXCEPT
  {
    return impl_ ? impl_->state_snapshot () : state_snapshot_t ();
  }

  /// @brief True while the state is `open`.
  bool
  is_open () const LUMEX_NOEXCEPT
  {
    return impl_ && impl_->state () == channel_state::open;
  }

  /// @brief The error of the last failed attempt or lost link.
  std::error_code
  last_error () const
  {
    return impl_ ? impl_->last_error () : std::error_code ();
  }

  /// @brief The text that goes with `last_error ()`. A factory that threw
  /// shows in `last_error ()` as the exception's own code (a
  /// `std::system_error`) or `invalid_endpoint`; this is the `what ()` of the
  /// exception (the factory's reason: "no device with this serial number").
  /// For every other failure it is the message of the code.
  std::string
  last_error_text () const
  {
    return impl_ ? impl_->last_error_text () : std::string ();
  }

  /// @brief The counters of the transport of the last attempt (zero before
  /// the first).
  statistics::statistics_t
  statistics () const
  {
    return impl_ ? impl_->transport_statistics ()
                 : statistics::statistics_t ();
  }

  /// @brief Attempts, failed attempts and reconnects.
  session_statistics_t
  session_statistics () const LUMEX_NOEXCEPT
  {
    return impl_ ? impl_->session_statistics () : session_statistics_t ();
  }

  /// @brief Sets the transport's counters and the session's to zero.
  void
  reset_statistics ()
  {
    if (!impl_)
      return;
    impl_->reset_transport_statistics ();
    impl_->reset_session_statistics ();
  }

  /// @brief Begins a cycle (no effect while one runs or the session is
  /// open).
  void
  start ()
  {
    if (impl_)
      impl_->request_start ();
  }

  /**
   * @brief Starts and waits until the session is open, within the deadline.
   * @return Success when open; the last error when it faulted; `cancelled`
   * when it was closed or cancelled; `timeout` at the deadline (the session
   * goes on connecting); `would_deadlock` on a thread of its executor.
   */
  status_result_t
  open (deadline_t deadline)
  {
    if (!impl_)
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (error::transport_errc::not_open));
    if (::lumex::applied::transport::detail::runs_on (impl_->executor ()))
      return status_result_t (
          lumex::core::expected::result::unexpect,
          make_error_code (error::transport_errc::would_deadlock));
    std::shared_ptr<::lumex::applied::transport::detail::sync_state> const
        state
        = std::make_shared<::lumex::applied::transport::detail::sync_state> ();
    std::uint64_t const id = impl_->next_waiter_id ();
    ::lumex::applied::transport::detail::sync_status_handler handler
        = { state };
    impl_->start_open (id, std::move (handler));
    std::unique_lock<std::mutex> lock (state->mutex);
    if (!state->done_signal.wait_for (lock, deadline,
                                      [&state] { return state->done; }))
      {
        lock.unlock ();
        impl_->forget_open_wait (id);
        return status_result_t (
            lumex::core::expected::result::unexpect,
            make_error_code (error::transport_errc::timeout));
      }
    if (state->error)
      return status_result_t (lumex::core::expected::result::unexpect,
                              state->error);
    return status_result_t ();
  }

  /// @brief Starts; the token gets `void (std::error_code)` once the session
  /// is open (success), faulted (the last error) or closed (`cancelled`).
  // The return types name the arguments as the bodies pass them (lvalues):
  // asio::deferred keeps their types.
  template <typename Token>
  auto
  async_open (Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_open> (), token,
          std::declval<std::shared_ptr<impl_type> &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_open (initiation_executor ()), token, impl_);
  }

  /**
   * @brief Waits for a state other than `seen`, or for any change after
   * this call (also one that returns to `seen`); the token gets
   * `void (std::error_code, channel_state)` with the state, or `cancelled` on
   * `close ()` / `cancel ()`. A change between the `state ()` that returned
   * `seen` and this call that returns to `seen` (open, reconnecting, open) is
   * not reported: wait from a `state_snapshot ()` to miss none.
   */
  template <typename Token>
  auto
  async_wait_state_change (channel_state seen, Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code,
                                                     channel_state)> (
          std::declval<initiate_wait_state_change> (), token,
          std::declval<std::shared_ptr<impl_type> &> (), seen))
  {
    return asio::async_initiate<Token, void (std::error_code, channel_state)> (
        initiate_wait_state_change (initiation_executor ()), token, impl_,
        seen);
  }

  /**
   * @brief Waits for a change after the snapshot `seen` (at once when one
   * happened since): no change is missed. The token gets
   * `void (std::error_code, state_snapshot_t)` with the snapshot now (the
   * start of the next wait), or `cancelled` on `close ()` / `cancel ()`.
   * Several changes between two waits are reported once, with the sequence
   * grown by their number.
   */
  template <typename Token>
  auto
  async_wait_state_change (state_snapshot_t const &seen, Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code,
                                                     state_snapshot_t)> (
          std::declval<initiate_wait_snapshot_change> (), token,
          std::declval<std::shared_ptr<impl_type> &> (), seen))
  {
    return asio::async_initiate<Token,
                                void (std::error_code, state_snapshot_t)> (
        initiate_wait_snapshot_change (initiation_executor ()), token, impl_,
        seen);
  }

  /// @brief Ends connecting and reconnecting at once and closes (any
  /// thread, idempotent).
  void
  close () LUMEX_NOEXCEPT
  {
    if (!impl_)
      return;
    try
      {
        impl_->request_close ();
      }
    catch (...)
      {
      }
  }

  /// @brief Ends connecting and reconnecting at once (`closed`), or cancels
  /// the pending I/O of an open session (any thread).
  void
  cancel () LUMEX_NOEXCEPT
  {
    if (!impl_)
      return;
    try
      {
        impl_->request_cancel ();
      }
    catch (...)
      {
      }
  }

  /// @brief The device is present (true) or gone (false).
  void
  set_device_present (bool present)
  {
    if (impl_)
      impl_->set_device_present (present);
  }

  /// @brief A function that calls `set_device_present` of this session while
  /// it lives (it holds no strong reference), for `device_watcher`.
  presence_sink_t
  presence_sink () const
  {
    std::weak_ptr<detail::session_core> const weak = impl_;
    return [weak] (bool present)
      {
        std::shared_ptr<detail::session_core> const core = weak.lock ();
        if (core)
          core->set_device_present (present);
      };
  }

protected:
  /// @brief What the initiations report: the session strand (the system
  /// executor for a moved-from session).
  executor_type
  initiation_executor () const
  {
    if (!impl_)
      return executor_type (asio::system_executor ());
    return executor_type (impl_->strand ());
  }

  std::shared_ptr<impl_type> impl_;

private:
  void
  release () LUMEX_NOEXCEPT
  {
    if (!impl_)
      return;
    impl_->detach ();
    try
      {
        impl_->request_close ();
      }
    catch (...)
      {
      }
  }
};
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_BASIC_SESSION_HPP
