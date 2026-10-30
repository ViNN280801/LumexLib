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
 * @file LumexDialogueImpl.hpp
 * @brief `dialogue_impl`: the engine behind a `dialogue_engine`: the queue,
 * the exchanges in flight, the correlation of the frames, the timeouts, the
 * retries, the late replies and the counters.
 * @details Internal to the transport library. Everything runs on the strand
 * of the link (`frame_link::get_strand ()`): the reads and writes of the
 * link complete there, the timers are made for it, and the calls of the
 * front post a step to it. A user's handler is never called inside the
 * engine: its result is posted through the strand to the handler's
 * associated executor, after the engine's state is final. The handlers the
 * engine gives the link and the timers hold it weakly; the front holds it
 * strongly, and so does every posted step until it ran. The engine keeps
 * the handlers of the queued and the pending exchanges in its members: it is
 * a `handler_keeper`, so the `io_context` shutdown destroys them uncalled.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DETAIL_DIALOGUE_IMPL_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DETAIL_DIALOGUE_IMPL_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/index/LumexProtocolSession.hpp"
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
namespace protocol
{
namespace detail
{
/// @brief The counters of a dialogue (any thread).
struct dialogue_counters_t
{
  std::atomic<std::uint64_t> requests;
  std::atomic<std::uint64_t> replies;
  std::atomic<std::uint64_t> retries;
  std::atomic<std::uint64_t> timeouts;
  std::atomic<std::uint64_t> retries_exhausted;
  std::atomic<std::uint64_t> naks;
  std::atomic<std::uint64_t> unexpected_replies;
  std::atomic<std::uint64_t> late_replies;
  std::atomic<std::uint64_t> unknown_frames;
  std::atomic<std::uint64_t> events;
  std::atomic<std::uint64_t> stream_gaps;
  std::atomic<std::uint64_t> rate_limited;
  std::atomic<std::uint64_t> reconnects;
  std::atomic<std::uint64_t> queue_refusals;
  std::atomic<std::size_t> max_queued;

  dialogue_counters_t () LUMEX_NOEXCEPT;
  dialogue_counters_t (dialogue_counters_t const &) = delete;
  dialogue_counters_t &operator= (dialogue_counters_t const &) = delete;

  /// @brief Adds one to `counter`.
  static void bump (std::atomic<std::uint64_t> &counter) LUMEX_NOEXCEPT;
  /// @brief Raises `max_queued` to `queued`.
  void note_queued (std::size_t queued) LUMEX_NOEXCEPT;
  /// @brief A snapshot.
  dialogue_statistics_t snapshot () const LUMEX_NOEXCEPT;
  /// @brief Every counter to zero.
  void reset () LUMEX_NOEXCEPT;
};

/// @brief One exchange the engine holds (queued or in flight).
struct exchange_record_t
{
  /// Its number, given when the exchange was started.
  std::uint64_t id = 0;
  /// The request message, shared by every attempt.
  std::shared_ptr<link_frame_t const> message;
  /// The rows, found in the index of the dialogue.
  ::lumex::core::protocol::index::row_t const *row = nullptr;
  ::lumex::core::protocol::index::row_t const *reply_row = nullptr;
  dialogue::correlation how = dialogue::correlation::by_key;
  dialogue::reply_matcher_t matcher = nullptr;
  /// The policy (never empty in the engine).
  std::shared_ptr<retry_policy_t const> policy;
  /// The user's handler, erased once.
  exchange_handler_t handler;
  /// False for a row without a reply: the write ends the exchange.
  bool expects_reply = true;
  /// The attempts made, and allowed so far (1 + retries + extra ones).
  std::uint64_t attempts = 0;
  std::uint64_t allowed = 1;
  /// The number of the current attempt: a completion of an earlier one
  /// (a timer, a write) changes nothing.
  std::uint64_t epoch = 0;
  /// An attempt waits for its reply.
  bool awaiting = false;
  /// The next attempt waits for the late-reply window to end.
  bool waiting_window = false;
  /// The timeout of the current attempt.
  timing::timer timer;
};

/// @brief The engine of a dialogue (see the file description).
class dialogue_impl
    : public ::lumex::applied::transport::detail::handler_keeper,
      public std::enable_shared_from_this<dialogue_impl>
{
public:
  using strand_type = frame_link::strand_type;
  using record_ptr = std::unique_ptr<exchange_record_t>;

  /// @brief Makes the engine over `link` (not empty) and starts reading.
  static std::shared_ptr<dialogue_impl>
  make (frame_link link,
        ::lumex::core::protocol::index::protocol_index_t const &index,
        dialogue_options_t const &options);

  dialogue_impl (frame_link link,
                 ::lumex::core::protocol::index::protocol_index_t const &index,
                 dialogue_options_t const &options);
  ~dialogue_impl () override;
  dialogue_impl (dialogue_impl const &) = delete;
  dialogue_impl &operator= (dialogue_impl const &) = delete;

  // --- any thread --------------------------------------------------------

  /// @brief The strand of the engine (the link's).
  strand_type const &
  strand () const LUMEX_NOEXCEPT
  {
    return strand_;
  }

  /// @brief The executor of the transport.
  executor_type get_executor () const;

  /// @brief The name for logs.
  std::string const &
  name () const LUMEX_NOEXCEPT
  {
    return name_;
  }

  /// @brief The index of the protocol layer.
  ::lumex::core::protocol::index::protocol_index_t const &
  index () const LUMEX_NOEXCEPT
  {
    return *index_;
  }

  /// @brief The session of the device.
  ::lumex::core::protocol::index::session_context_t &
  session () LUMEX_NOEXCEPT
  {
    return session_;
  }

  /// @brief The counters.
  dialogue_counters_t &
  counters () LUMEX_NOEXCEPT
  {
    return counters_;
  }

  dialogue_counters_t const &
  counters () const LUMEX_NOEXCEPT
  {
    return counters_;
  }

  /// @brief Starts an exchange: the handler arrives erased, with the mark of
  /// its cancellation slot (none without a slot).
  void enqueue (
      exchange_t request, exchange_handler_t handler,
      ::lumex::applied::transport::detail::operation_mark_ptr const &mark);

  /// @brief Ends every exchange with `cancelled`; the link stays.
  void start_cancel ();

  /// @brief Ends every exchange with `cancelled` and closes the link, for
  /// good.
  void start_close ();

  /// @brief Posts the cancellation a signal on the slot of exchange `id`
  /// requested.
  void request_exchange_cancel (
      std::uint64_t id,
      std::weak_ptr<::lumex::applied::transport::detail::operation_mark> mark,
      asio::cancellation_type_t type);

  /// @brief The `io_context` is being destroyed: the handlers of the queued
  /// and pending exchanges die uncalled.
  void drop_kept_handlers () LUMEX_NOEXCEPT override;

  // --- on the strand (the posted steps and the handlers) ----------------

  void run_start ();
  void run_enqueue (std::uint64_t id, exchange_t &request,
                    exchange_handler_t &handler);
  void run_cancel ();
  void run_close ();
  void run_cancel_exchange (std::uint64_t id, asio::cancellation_type_t type);
  void read_done (std::uint64_t generation, std::error_code error,
                  link_frame_t frame);
  void write_done (std::uint64_t id, std::uint64_t epoch,
                   std::error_code error);
  void timeout_done (std::uint64_t id, std::uint64_t epoch,
                     std::error_code error);
  void window_done (std::uint64_t window, std::error_code error);

private:
  void enlist ();
  void start_reading ();
  bool can_start (exchange_record_t const &record) const;
  void pump ();
  void start_attempt (exchange_record_t &record);
  void attempt_timed_out (std::size_t at);
  void note_abandoned (exchange_record_t const &record);
  void handle_frame (link_frame_t &frame);
  bool accepts (exchange_record_t const &record,
                ::lumex::core::protocol::index::row_t const *row,
                link_frame_t const &frame, std::exception_ptr &thrown) const;
  bool take_late (::lumex::core::protocol::index::row_t const *row);
  std::size_t find_in_flight (std::uint64_t id) const;
  void finish (record_ptr record, std::error_code error, link_frame_t frame);
  void finish_in_flight (std::size_t at, std::error_code error,
                         link_frame_t frame);
  void complete (exchange_handler_t handler, std::error_code error,
                 link_frame_t frame);
  void end_all (std::error_code error, bool abandoned);
  void link_lost (std::error_code error);

  frame_link link_;
  strand_type strand_;
  ::lumex::core::protocol::index::protocol_index_t const *index_;
  std::string name_;
  std::size_t max_in_flight_;
  std::size_t queue_bound_;
  std::uint32_t late_window_ms_;
  timing::timer_source timers_;
  std::shared_ptr<retry_policy_t const> defaults_;
  ::lumex::core::protocol::index::session_context_t session_;
  dialogue_counters_t counters_;
  std::deque<record_ptr> queue_;
  std::vector<record_ptr> flight_;
  // The rows whose reply an abandoned attempt still expects (bounded).
  std::deque<::lumex::core::protocol::index::row_t const *> late_rows_;
  // The late-reply window of a by_order row: sends wait while it is open.
  timing::timer window_timer_;
  bool window_open_;
  std::uint64_t window_sequence_;
  bool reading_;
  std::uint64_t read_generation_;
  bool closed_;
  std::uint64_t attempt_sequence_;
  std::atomic<std::uint64_t> next_id_;
  std::atomic<bool> enlisted_;
  // The marks of the exchanges with a cancellation slot, settled when the
  // result is handed on.
  ::lumex::applied::transport::detail::mark_registry marks_;
};
} // namespace detail
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DETAIL_DIALOGUE_IMPL_HPP
