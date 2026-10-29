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
 * @file LumexSessionCore.hpp
 * @brief `session_core`: the state machine every session shares (states,
 * attempts, retry timer, open timeout, waiters, presence), independent of the
 * transport type.
 * @details Internal to the transport library. Everything of the core runs on
 * its own strand; the `request_*` functions and `set_device_present` may be
 * called from any thread and post to it. Only the posted steps and the
 * timers of the core hold a `shared_ptr` to it (they end once the strand
 * ran them, or with the `io_context`), so the front may be destroyed at any
 * time, also inside a state callback or a completion handler. The handlers
 * the core gives its transport (the open, the I/O) hold it weakly: the
 * transport is owned by the core, and a strong reference from a handler it
 * keeps while its operation is pending would be a cycle that only the posted
 * close breaks (it leaked when the `io_context` stopped first, REVIEW13 L1).
 *
 * `close ()` and `cancel ()` end reconnecting at once: they bump an atomic
 * stop epoch before they post, and every step of a cycle (the retry timer,
 * the completion of an open, the next attempt) checks it first, so no step
 * that runs after the call starts another attempt.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_CORE_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_CORE_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionHandlers.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
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
namespace session
{
namespace detail
{
struct session_core_access;
struct terminal_canceller;

/// @brief An empty name (the name of a moved-from session).
std::string const &empty_session_name () LUMEX_NOEXCEPT;

/**
 * @brief The transport-independent part of a session. A derived class owns
 * the transport and implements the four `*_transport` functions, which always
 * run on the strand.
 */
class session_core
    : public ::lumex::applied::transport::detail::handler_keeper,
      public std::enable_shared_from_this<session_core>
{
public:
  using strand_type = asio::strand<executor_type>;

  session_core (executor_type const &executor, session_options_t options);
  ~session_core () override;

  session_core (session_core const &) = delete;
  session_core &operator= (session_core const &) = delete;

  /// @brief The executor the session was given (its transports get it too).
  executor_type const &
  executor () const LUMEX_NOEXCEPT
  {
    return executor_;
  }

  /// @brief The strand everything of the session runs on.
  strand_type const &
  strand () const LUMEX_NOEXCEPT
  {
    return strand_;
  }

  /// @brief The name for logs.
  std::string const &
  name () const LUMEX_NOEXCEPT
  {
    return options_.name;
  }

  /// @brief The state now (any thread).
  channel_state state () const LUMEX_NOEXCEPT;

  /// @brief The state and the number of its changes, read together (any
  /// thread).
  state_snapshot_t state_snapshot () const LUMEX_NOEXCEPT;

  /// @brief The error of the last failed attempt or lost link (any thread).
  std::error_code last_error () const;

  /// @brief The text that goes with `last_error ()` (any thread): the
  /// message of the exception when the factory threw, else the message of
  /// the error code.
  std::string last_error_text () const;

  /// @brief The session's own counters (any thread).
  session_statistics_t session_statistics () const LUMEX_NOEXCEPT;

  /// @brief Sets the session's own counters to zero (any thread).
  void reset_session_statistics () LUMEX_NOEXCEPT;

  // --- any thread: each posts to the strand
  // -----------------------------------------

  /// @brief Begins a cycle unless one runs or the session is open.
  void request_start ();
  /// @brief Ends reconnecting at once, closes the transport, `closed`.
  void request_close ();
  /// @brief Ends reconnecting at once (`closed`), or cancels the pending
  /// I/O of the open transport; ends the waiters with `cancelled`.
  void request_cancel ();
  /// @brief The device came (true) or went (false); see the session docs.
  void set_device_present (bool present);
  /// @brief The front is gone: no state callback runs from now on.
  void detach () LUMEX_NOEXCEPT;
  /// @brief A new id for `start_open`.
  std::uint64_t next_waiter_id () LUMEX_NOEXCEPT;
  /// @brief Drops the open waiter `id` without calling it (its sync caller
  /// gave up).
  void forget_open_wait (std::uint64_t id);
  /// @brief The `io_context` is being destroyed: the waiters die uncalled
  /// (a waiter whose handler owns the session would otherwise keep it for
  /// ever).
  void drop_kept_handlers () LUMEX_NOEXCEPT override;

  /// @brief Completes with the state once it differs from `seen` or changed
  /// after this call (posted at once when it already did), or `cancelled` on
  /// a close or a cancel. The handler is type-erased here, once (see
  /// `erase_once`).
  template <typename Handler>
  void
  start_wait_state_change (channel_state seen, Handler &&handler)
  {
    std::uint64_t const sequence = state_snapshot ().sequence;
    ::lumex::applied::transport::detail::operation_mark_ptr const mark
        = ::lumex::applied::transport::detail::cancellation_mark (handler);
    state_handler_t erased
        = erase_once<state_handler_t> (std::forward<Handler> (handler), mark);
    watch_terminal_cancellation (erased, mark);
    enqueue_state_wait (seen, sequence, std::move (erased));
  }

  /// @brief Completes with the snapshot once the sequence differs from
  /// `seen.sequence` (at once when it already does), or `cancelled`.
  template <typename Handler>
  void
  start_wait_snapshot_change (state_snapshot_t const &seen, Handler &&handler)
  {
    ::lumex::applied::transport::detail::operation_mark_ptr const mark
        = ::lumex::applied::transport::detail::cancellation_mark (handler);
    snapshot_handler_t erased = erase_once<snapshot_handler_t> (
        std::forward<Handler> (handler), mark);
    watch_terminal_cancellation (erased, mark);
    enqueue_snapshot_wait (seen.sequence, std::move (erased));
  }

  /// @brief Starts (as `request_start`) and completes when the session is
  /// open (success), faulted (the last error) or closed (`cancelled`).
  template <typename Handler>
  void
  start_open (std::uint64_t id, Handler &&handler)
  {
    ::lumex::applied::transport::detail::operation_mark_ptr const mark
        = ::lumex::applied::transport::detail::cancellation_mark (handler);
    status_handler_t erased
        = erase_once<status_handler_t> (std::forward<Handler> (handler), mark);
    watch_terminal_cancellation (erased, mark);
    enqueue_open_wait (id, std::move (erased));
  }

  // --- on the strand
  // ---------------------------------------------------------

  /// @brief True when I/O may start: open, not stopped; `generation` names
  /// the current transport.
  bool usable (unsigned &generation) const LUMEX_NOEXCEPT;

  /// @brief The result of an I/O operation on the transport `generation`:
  /// `carried` when it moved data (see `reconnect_policy_t`); an error that
  /// `is_link_lost` accepts starts reconnecting.
  void note_io_result (unsigned generation, std::error_code const &error,
                       bool carried);

  /// @brief The completion of the open of the transport `generation`.
  void on_opened (unsigned generation, std::error_code const &error);

protected:
  /// @brief Makes the next transport through the factory (replacing the
  /// current one); throws when the factory throws.
  virtual void make_transport () = 0;
  /// @brief Starts the open of the current transport; its completion calls
  /// `on_opened (generation, error)` on the strand.
  virtual void open_transport (unsigned generation) = 0;
  /// @brief Closes and releases the current transport (if any).
  virtual void drop_transport () LUMEX_NOEXCEPT = 0;
  /// @brief Cancels the pending operations of the current transport.
  virtual void cancel_transport () LUMEX_NOEXCEPT = 0;

private:
  friend struct session_core_access;

  // A terminal cancellation emitted on the cancellation slot of a waiter's
  // handler while the waiter is pending (its mark lives and is not settled)
  // does what request_cancel () does (as a terminal cancellation of a
  // transport's operation does what its cancel () does). Installed on the
  // slot of the erased handler: the erasure puts its own cancellation state
  // in the user's slot, which passes every cancellation type on (the
  // canceller keeps only terminal). The mark is remembered by that slot and
  // settled when the waiter's result is handed on.
  template <typename Erased>
  void
  watch_terminal_cancellation (
      Erased &erased,
      ::lumex::applied::transport::detail::operation_mark_ptr const &mark)
  {
    if (!mark)
      return;
    asio::cancellation_slot slot = erased.get_cancellation_slot ();
    if (!slot.is_connected ())
      return;
    slot.template emplace<terminal_canceller> (
        std::weak_ptr<session_core> (shared_from_this ()),
        std::weak_ptr<::lumex::applied::transport::detail::operation_mark> (
            mark));
    marks_.watch (slot, mark);
  }

  struct open_waiter
  {
    std::uint64_t id;
    status_handler_t handler;
  };

  void enlist ();
  void enqueue_state_wait (channel_state seen, std::uint64_t sequence,
                           state_handler_t handler);
  void enqueue_snapshot_wait (std::uint64_t sequence,
                              snapshot_handler_t handler);
  void enqueue_open_wait (std::uint64_t id, status_handler_t handler);

  // On the strand (session_core_access calls them from the posted steps).
  bool running () const LUMEX_NOEXCEPT;
  bool do_start (unsigned epoch);
  void do_stop (unsigned epoch, bool close_link);
  void do_presence (bool present);
  void do_state_wait (channel_state seen, std::uint64_t sequence,
                      state_handler_t handler);
  void do_snapshot_wait (std::uint64_t sequence, snapshot_handler_t handler);
  void do_open_wait (unsigned epoch, std::uint64_t id,
                     status_handler_t handler);
  void do_forget_open_wait (std::uint64_t id);
  void on_retry_timer (unsigned wait_id);
  void on_open_timer (unsigned generation, std::error_code const &error);

  void begin_cycle (channel_state state, std::error_code const &reason);
  void attempt ();
  void fail_attempt (std::error_code const &error,
                     std::string const &text = std::string ());
  void schedule_retry (std::uint32_t failed_attempt);
  void cancel_retry ();
  void lose_link (std::error_code const &error);
  bool established () const;
  void drop_current () LUMEX_NOEXCEPT;
  void set_state (channel_state state, std::error_code const &reason);
  void set_last_error (std::error_code const &error,
                       std::string const &text = std::string ());
  void cancel_waiters ();
  double draw_random ();

  executor_type executor_;
  strand_type strand_;
  session_options_t options_;
  // Both from options_.timers, bound to the strand.
  timing::timer retry_timer_;
  timing::timer open_timer_;

  // Any thread. The state (low 8 bits) and the number of its changes (the
  // rest) in one word: a snapshot reads both at once.
  std::atomic<std::uint64_t> state_word_;
  std::atomic<unsigned> stop_epoch_;
  std::atomic<bool> detached_;
  // Set when the core was enlisted for the io_context shutdown.
  std::atomic<bool> enlisted_;
  std::atomic<std::uint64_t> next_waiter_id_;
  std::atomic<std::uint64_t> attempts_;
  std::atomic<std::uint64_t> failed_attempts_;
  std::atomic<std::uint64_t> reconnects_;
  mutable std::mutex error_mutex_;
  std::error_code last_error_;
  // What goes with last_error_ when its code says too little (the what () of
  // a factory that threw); empty: the message of the code.
  std::string last_error_text_;

  // The strand only.
  bool running_;
  unsigned run_epoch_;
  unsigned generation_;
  unsigned wait_id_;
  std::uint32_t attempt_;
  bool opening_;
  bool waiting_retry_;
  bool parked_;
  bool device_absent_;
  // The current link moved data (an I/O with bytes, a message).
  bool carried_data_;
  // A lost link (or a removal of the device) seen while a stop was on its
  // way: cancel () keeps an open link, so its step replays the loss of the
  // transport `lost_generation_`.
  std::error_code lost_while_stopping_;
  unsigned lost_generation_;
  // On the clock of options_.timers.
  timing::time_point_t opened_at_;
  std::vector<state_handler_t> state_waiters_;
  std::vector<snapshot_handler_t> snapshot_waiters_;
  std::vector<open_waiter> open_waiters_;
  // The marks of the waiters with a cancellation slot, settled as their
  // result is handed on: a signal emitted after that ends nothing.
  ::lumex::applied::transport::detail::mark_registry marks_;
};
/// @brief The step of a waiter's terminal cancellation, on the session
/// strand: nothing when the waiter's handler ran since the signal (its mark
/// is gone); a signal emitted before the waiter settled acts.
struct terminal_cancel_step
{
  std::shared_ptr<session_core> core;
  std::weak_ptr<::lumex::applied::transport::detail::operation_mark> pending;

  void
  operator() ()
  {
    if (::lumex::applied::transport::detail::take_cancellation_request (
            pending))
      core->request_cancel ();
  }
};

/**
 * @brief The cancellation slot handler of a session's waiter: a terminal
 * cancellation while the waiter is pending does what `request_cancel ()`
 * does (posted to the strand, checked there again); `partial` and `total`
 * alone change nothing, and a signal emitted after the waiter's result was
 * handed on (settled) reaches nothing, also before its handler ran.
 */
struct terminal_canceller
{
  terminal_canceller (
      std::weak_ptr<session_core> owner,
      std::weak_ptr<::lumex::applied::transport::detail::operation_mark> mark)
      : core (std::move (owner)), pending (std::move (mark))
  {
  }

  void
  operator() (asio::cancellation_type type) LUMEX_NOEXCEPT
  {
    if ((type & asio::cancellation_type::terminal)
        == asio::cancellation_type::none)
      return;
    std::shared_ptr<session_core> const alive = core.lock ();
    if (!alive
        || !::lumex::applied::transport::detail::request_cancellation (
            pending))
      return;
    try
      {
        terminal_cancel_step step = { alive, pending };
        asio::post (alive->strand (), std::move (step));
      }
    catch (...)
      {
        // Out of memory: the waiter is not cancelled; a later signal may try
        // again.
        ::lumex::applied::transport::detail::take_cancellation_request (
            pending);
      }
  }

  std::weak_ptr<session_core> core;
  std::weak_ptr<::lumex::applied::transport::detail::operation_mark> pending;
};
} // namespace detail
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_CORE_HPP
