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
 * retries, the late replies and the counters
 * (`LumexDialogueImpl.cpp`); the streams, the keepalive, the watchdog, the
 * rate limits, the state, the edges and the restart of the reading
 * (`LumexDialogueImplFeatures.cpp`); the sequences of steps, scenarios and the
 * enabling of streams (`LumexDialogueImplRuns.cpp`).
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
 *
 * The timers of the features are lazy: a wait is armed for the nearest time
 * known when it is armed, and what its completion does is decided by the clock
 * and the state at that moment, so a frame or a request that moved the time
 * meanwhile needs no re-arm and a due wait racing it does no harm. A wait of a
 * step of a run carries a number, as the timeout of an attempt does.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DETAIL_DIALOGUE_IMPL_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DETAIL_DIALOGUE_IMPL_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolScenario.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
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
  std::atomic<std::uint64_t> state_refusals;
  std::atomic<std::uint64_t> edge_events;
  std::atomic<std::uint64_t> keepalives;
  std::atomic<std::uint64_t> watchdog_expiries;
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
  /// The policy was made from the row: `extra_retries` are those of the
  /// defaults of the dialogue.
  bool row_policy = false;
  /// The user's handler, erased once.
  exchange_handler_t handler;
  /// False for a row without a reply: the write ends the exchange.
  bool expects_reply = true;
  /// The attempts made, and allowed so far (1 + retries + extra ones).
  std::uint64_t attempts = 0;
  std::uint64_t allowed = 1;
  /// The number of the current attempt: the write of an earlier one changes
  /// nothing.
  std::uint64_t epoch = 0;
  /// The number of the current wait of the timer: a wait of the past (an
  /// earlier attempt or phase) changes nothing.
  std::uint64_t timer_epoch = 0;
  /// The reader of the reply kind (null: the frame as it is).
  reply_reader_t reader = nullptr;
  /// The first phase of a two-phase reply came: no retry any more.
  bool acknowledged = false;
  /// The current attempt was already counted as delayed by a rate limit.
  bool rate_counted = false;
  /// The next attempt waits for a rate limit.
  bool waiting_rate = false;
  /// An attempt waits for its reply.
  bool awaiting = false;
  /// The next attempt waits for the late-reply window to end.
  bool waiting_window = false;
  /// The timeout of the current attempt.
  timing::timer timer;
};

/// @brief The values a table gives the dialogue and a change of layer may
/// replace.
struct layer_values_t
{
  std::size_t max_in_flight = 1;
  std::uint32_t timeout_ms = 0;
  std::uint32_t retries = 0;
  std::uint32_t late_window_ms = 0;
};

/// @brief What `table` gives; a table with no default timeout gives the
/// timeout the dialogue was made with, `base_timeout_ms`.
inline layer_values_t
layer_values_of (protocol_features_t const &table,
                 std::uint32_t base_timeout_ms)
{
  layer_values_t made;
  made.max_in_flight = table.max_in_flight == 0 ? 1 : table.max_in_flight;
  made.timeout_ms = table.default_timeout_ms != 0 ? table.default_timeout_ms
                                                  : base_timeout_ms;
  made.retries = table.default_retries;
  made.late_window_ms = table.late_reply_window_ms;
  return made;
}

/// @brief What the table of `options` gave: the values of its features, and
/// of `options` themselves when it has none (no table was in use, so nothing
/// the application set differs from one).
inline layer_values_t
layer_values_at_start (dialogue_options_t const &options)
{
  if (options.features != nullptr)
    return layer_values_of (*options.features, options.defaults.timeout_ms);
  layer_values_t made;
  made.max_in_flight = options.max_in_flight == 0 ? 1 : options.max_in_flight;
  made.timeout_ms = options.defaults.timeout_ms;
  made.retries = options.defaults.retries;
  made.late_window_ms = options.late_reply_window_ms;
  return made;
}

/// @brief What the engine keeps of a row with a feature: a stream, a rate
/// limit, edges, the state.
struct row_watch_t
{
  /// The row, in the index of the dialogue.
  ::lumex::core::protocol::index::row_t const *row = nullptr;
  /// What the table says of it (null without features).
  row_features_t const *features = nullptr;
  /// `stream (interval_ms)`, 0 for no stream.
  std::uint32_t stream_interval_ms = 0;
  /// The stream is watched: a frame came, or the row that enables it answered.
  bool watching = false;
  /// When its last frame came (or the watch began).
  timing::time_point_t last_frame;
  /// This silence was counted.
  bool gap_counted = false;
  /// The time of its last request, when one was sent.
  bool sent = false;
  timing::time_point_t last_sent;
  /// The reply fields read from each frame: the fields of the edges in their
  /// order, then the field of the state.
  std::vector<std::size_t> fields;
  /// Their values in the last frame that decoded.
  std::vector<std::int64_t> levels;
  /// `levels` holds a frame.
  bool levels_known = false;
  /// Whether the last of `fields` is the field of the state.
  bool reads_state = false;
};

/// @brief One sequence of steps the engine runs (a scenario, the enabling of
/// the streams).
struct sequence_run_t
{
  /// What a run waits for.
  enum class waiting_t : std::uint8_t
  {
    nothing,
    exchange,
    frame,
    state
  };

  /// Its number.
  std::uint64_t id = 0;
  /// The steps.
  ::lumex::core::protocol::dsl::scenario_t scenario;
  /// What the table of the scenario says of its rows.
  protocol_features_t const *features = nullptr;
  /// The user's handler, erased once.
  sequence_handler_t handler;
  /// The current step.
  std::size_t step = 0;
  /// What it waits for.
  waiting_t waiting = waiting_t::nothing;
  /// The exchange of a `send` under way.
  std::uint64_t exchange_id = 0;
  /// The row of an `expect` under way.
  ::lumex::core::protocol::index::row_t const *expected = nullptr;
  /// The row the next step expects, kept while this one runs: its frame may
  /// come right before the reply that ends this step.
  ::lumex::core::protocol::index::row_t const *held_row = nullptr;
  bool has_held = false;
  /// The number of the wait of the current step: a wait of the past changes
  /// nothing.
  std::uint64_t timer_epoch = 0;
  /// The time of an `expect` or a `wait_state`.
  timing::timer timer;
  /// The last frame of each row seen in this run, for a branch.
  std::vector<
      std::pair<::lumex::core::protocol::index::row_t const *, link_frame_t>>
      replies;
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
    return index_now ();
  }

  /// @brief Posts the change to another layer of the table.
  void start_select_layer (
      ::lumex::core::protocol::index::protocol_index_t const &index,
      protocol_features_t const *features);

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

  /// @brief Calls `handler` with every frame of `row` that answers no
  /// exchange; an id of 0 when the index has no such row or the handler is
  /// empty.
  subscription_t subscribe (::lumex::core::protocol::index::row_t const *row,
                            frame_handler_t handler);

  /// @brief Ends a subscription; false when there is none with its id.
  bool unsubscribe (subscription_t subscription);

  /// @brief Calls `handler` with every frame whose key matches no row (an
  /// empty one removes it).
  void set_unknown_handler (frame_handler_t handler);

  /// @brief Calls `handler` with the edges of `row`.
  subscription_t
  subscribe_edge (::lumex::core::protocol::index::row_t const *row,
                  edge_handler_t handler);

  /// @brief Calls `handler` when the state of the link changes.
  void set_link_handler (link_state_handler_t handler);

  /// @brief Whether nothing says the link is gone.
  bool
  link_up () const LUMEX_NOEXCEPT
  {
    return link_up_.load (std::memory_order_acquire);
  }

  /// @brief Takes the generator of the keepalive requests; false when the
  /// table names no keepalive or the dialogue is closed.
  bool set_keepalive (keepalive_generator_t generator);

  /// @brief Sets the last known state (it counts at once; the runs that wait
  /// for it hear it on the strand).
  void set_state (std::int64_t state);

  /// @brief Forgets the last known state.
  void clear_state () LUMEX_NOEXCEPT;

  /// @brief The last known state, 0 when none.
  std::int64_t last_state () const LUMEX_NOEXCEPT;

  /// @brief Whether a state is known.
  bool state_known () const LUMEX_NOEXCEPT;

  /// @brief Whether a state is known, and which (0 when none): one snapshot.
  bool read_state (std::int64_t &state) const LUMEX_NOEXCEPT;

  /// @brief Starts a sequence: the handler arrives erased, with the mark of
  /// its cancellation slot (none without a slot).
  void start_scenario (
      ::lumex::core::protocol::dsl::scenario_t scenario,
      protocol_features_t const *features, sequence_handler_t handler,
      ::lumex::applied::transport::detail::operation_mark_ptr const &mark);

  /// @brief Sends the rows that enable streams, in table order.
  void start_enable_streams (
      sequence_handler_t handler,
      ::lumex::applied::transport::detail::operation_mark_ptr const &mark);

  /// @brief Posts the cancellation a signal on the slot of run `id` requested.
  void request_run_cancel (
      std::uint64_t id,
      std::weak_ptr<::lumex::applied::transport::detail::operation_mark> mark);

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
  void rate_done (std::error_code error);
  void gap_done (std::error_code error);
  void watchdog_done (std::error_code error);
  void keepalive_done (std::error_code error);
  void keepalive_ended (std::error_code error);
  void read_restart_done (std::error_code error);
  void run_keepalive_start ();
  void run_select_layer (
      ::lumex::core::protocol::index::protocol_index_t const *index,
      protocol_features_t const *features);
  void run_state_set (std::int64_t state);
  void run_begin_sequence (std::uint64_t id,
                           ::lumex::core::protocol::dsl::scenario_t &scenario,
                           protocol_features_t const *features,
                           sequence_handler_t &handler);
  void run_enable_streams (std::uint64_t id, sequence_handler_t &handler);
  void run_cancel_sequence (std::uint64_t id);
  void sequence_advance (std::uint64_t id);
  void sequence_exchange_ended (std::uint64_t id, std::uint64_t exchange,
                                std::error_code error, link_frame_t frame);
  void sequence_timer_done (std::uint64_t id, std::uint64_t epoch,
                            std::error_code error);

private:
  ::lumex::core::protocol::index::protocol_index_t const &
  index_now () const LUMEX_NOEXCEPT
  {
    return *index_.load (std::memory_order_acquire);
  }

  void enlist ();
  void start_reading ();
  bool can_start (exchange_record_t const &record) const;
  void pump ();
  // False when the attempt could not start (it ended the exchange).
  bool start_attempt (exchange_record_t &record);
  // The next attempt of an exchange in flight, now or after a rate limit;
  // false when it could not start (it ended the exchange).
  bool begin_attempt (exchange_record_t &record);
  static bool exact (exchange_record_t const &record) LUMEX_NOEXCEPT;
  bool late_holds (::lumex::core::protocol::index::row_t const *row) const
      LUMEX_NOEXCEPT;
  void attempt_timed_out (std::size_t at);
  void next_attempt (std::size_t at,
                     ::lumex::core::protocol::error::protocol_errc why);
  void arm_timer (exchange_record_t &record);
  // `known`: what the reader already said of the frame, or null.
  void apply_reply (std::size_t at, link_frame_t &frame,
                    reply_reading_t const *known);
  std::string
  describe (link_frame_t const &frame,
            ::lumex::core::protocol::index::frame_direction direction) const;
  using handler_ptr = std::shared_ptr<frame_handler_t const>;
  std::vector<handler_ptr>
  subscribers_of (::lumex::core::protocol::index::row_t const *row) const;
  std::vector<handler_ptr> unknown_handlers () const;
  void note_abandoned (exchange_record_t const &record);
  void log_unknown (link_frame_t const &frame);
  void handle_frame (link_frame_t &frame);
  // Whether the exchange takes the frame. The reader of a by_order exchange
  // decides, and what it said is in `reading` (with `read` true), so that the
  // frame is not read twice.
  bool accepts (exchange_record_t const &record,
                ::lumex::core::protocol::index::row_t const *row,
                link_frame_t const &frame, std::exception_ptr &thrown,
                reply_reading_t &reading, bool &read) const;
  reply_reading_t read_reply (exchange_record_t const &record,
                              link_frame_t const &frame) const;
  bool take_late (::lumex::core::protocol::index::row_t const *row);
  std::size_t find_in_flight (std::uint64_t id) const;
  void finish (record_ptr record, std::error_code error, link_frame_t frame);
  void finish_in_flight (std::size_t at, std::error_code error,
                         link_frame_t frame);
  void complete (exchange_handler_t handler, std::error_code error,
                 link_frame_t frame);
  void end_all (std::error_code error, bool abandoned);
  void link_lost (std::error_code error);

  // --- the features (LumexDialogueImplFeatures.cpp) ----------------------

  void build_watches (protocol_features_t const *features);
  // The rows of what is under way, found again in the index of a new layer.
  ::lumex::core::protocol::index::row_t const *
  row_again (::lumex::core::protocol::index::row_t const *row) const;
  void follow_layer ();
  void take_old_watches (std::vector<row_watch_t> const &old);
  void apply_layer_times (std::uint32_t watchdog_ms,
                          std::uint32_t keepalive_ms,
                          row_features_t const *old_keepalive);
  row_watch_t *
  watch_of (::lumex::core::protocol::index::row_t const *row) LUMEX_NOEXCEPT;
  row_watch_t const *watch_of (
      ::lumex::core::protocol::index::row_t const *row) const LUMEX_NOEXCEPT;
  std::shared_ptr<retry_policy_t const>
  row_policy (::lumex::core::protocol::index::row_t const *row) const;
  void fill_exchange (exchange_t &request, row_features_t const *features,
                      ::lumex::core::protocol::index::row_t const *row) const;
  // The rate limits.
  timing::time_point_t
  rate_due (exchange_record_t const &record) const LUMEX_NOEXCEPT;
  void note_sent (exchange_record_t const &record) LUMEX_NOEXCEPT;
  void note_rate_limited (exchange_record_t &record);
  void arm_rate_timer (timing::time_point_t at);
  void fail_rate_waiters ();
  // The state.
  // Whether the state in `state` does not accept the request.
  bool refused_by_state (exchange_record_t const &record,
                         std::int64_t &state) const LUMEX_NOEXCEPT;
  void refuse_by_state (record_ptr record, std::int64_t state);
  void store_state (std::int64_t state) LUMEX_NOEXCEPT;
  // The frames: the stream of a row, its levels.
  void note_stream_frame (row_watch_t &watch);
  void count_gap (row_watch_t &watch);
  void start_watching (row_watch_t &watch, timing::time_point_t now);
  void arm_gap_timer (timing::time_point_t at);
  void note_success (exchange_record_t const &record);
  void read_levels (row_watch_t &watch, link_frame_t const &frame,
                    std::vector<edge_t> &edges, bool &state_changed,
                    std::int64_t &state);
  std::vector<std::shared_ptr<edge_handler_t const>>
  edge_handlers_of (::lumex::core::protocol::index::row_t const *row) const;
  // The link.
  void frame_seen (std::exception_ptr &thrown);
  void notify_link_state (link_state state, std::error_code const &reason);
  void forget_watches () LUMEX_NOEXCEPT;
  void schedule_read_restart ();
  void arm_watchdog (timing::time_point_t at);
  // The keepalive.
  void arm_keepalive ();
  void cancel_feature_timers () LUMEX_NOEXCEPT;
  struct feature_handlers_t;
  void take_feature_handlers (feature_handlers_t &out) LUMEX_NOEXCEPT;

  // --- the runs (LumexDialogueImplRuns.cpp) ------------------------------

  sequence_run_t *find_run (std::uint64_t id) const LUMEX_NOEXCEPT;
  void end_run (std::uint64_t id, std::error_code error);
  void end_all_runs (std::error_code error);
  void begin_sequence (std::uint64_t id,
                       ::lumex::core::protocol::dsl::scenario_t scenario,
                       protocol_features_t const *features,
                       sequence_handler_t handler);
  void complete_sequence (sequence_handler_t handler, std::error_code error,
                          std::size_t step);
  void remember_reply (sequence_run_t &run,
                       ::lumex::core::protocol::index::row_t const *row,
                       link_frame_t const &frame);
  void note_held_row (sequence_run_t &run);
  bool
  run_send_step (sequence_run_t &run,
                 ::lumex::core::protocol::dsl::scenario_step_t const &step);
  bool
  run_wait_timer (sequence_run_t &run,
                  ::lumex::core::protocol::dsl::scenario_step_t const &step);
  // Offers a frame that answered no exchange to the runs: true when one took
  // it; the runs to go on are added to `ready`.
  bool offer_to_runs (::lumex::core::protocol::index::row_t const *row,
                      link_frame_t const &frame,
                      std::vector<std::uint64_t> &ready);
  // The runs that wait for this state go on.
  void wake_state_waiters (std::int64_t state);

  frame_link link_;
  strand_type strand_;
  // Changed on the strand under handlers_mutex_ (select_layer); read anywhere.
  std::atomic<::lumex::core::protocol::index::protocol_index_t const *> index_;
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
  std::uint64_t timer_sequence_;
  // Unknown frames are warned about at most once a second (the time of the
  // timer source): when the last warning was, and how many frames of no row
  // went to debug since.
  bool unknown_warned_;
  timing::time_point_t unknown_warned_at_;
  std::uint64_t unknown_skipped_;
  std::atomic<std::uint64_t> next_id_;
  std::atomic<std::uint64_t> next_subscription_;
  // What the table says (null without features), and the rows with a feature.
  protocol_features_t const *features_;
  // What the table in use gave the four values of a change of layer, and the
  // timeout the dialogue was made with: a value that is not what the table
  // gave was set by the application and stays.
  layer_values_t table_;
  std::uint32_t base_timeout_ms_;
  std::vector<row_watch_t> watches_;
  std::unordered_map<::lumex::core::protocol::index::row_t const *,
                     std::size_t>
      watch_index_;
  // The rate limits: the time of the last request, and the wait.
  std::uint32_t min_interval_ms_;
  bool any_sent_;
  timing::time_point_t last_sent_;
  timing::timer rate_timer_;
  bool rate_armed_;
  timing::time_point_t rate_armed_for_;
  // Delayed requests are warned about at most once a second (the time of the
  // timer source): when the last warning was, and how many went to debug
  // since.
  bool rate_warned_;
  timing::time_point_t rate_warned_at_;
  std::uint64_t rate_skipped_;
  // The gaps of the streams.
  timing::timer gap_timer_;
  bool gap_armed_;
  timing::time_point_t gap_armed_for_;
  // The link state, the restart of the reading.
  std::atomic<bool> link_up_;
  std::uint32_t read_restart_ms_;
  std::uint32_t read_restart_max_ms_;
  timing::timer read_restart_timer_;
  bool read_restart_armed_;
  std::uint32_t read_failures_;
  bool read_restarted_at_once_;
  // The watchdog.
  std::uint32_t watchdog_ms_;
  timing::timer watchdog_timer_;
  bool watchdog_armed_;
  timing::time_point_t last_frame_at_;
  // The keepalive.
  std::uint32_t keepalive_ms_;
  row_features_t const *keepalive_row_;
  timing::timer keepalive_timer_;
  bool keepalive_armed_;
  bool keepalive_pending_;
  // An arm threw: a frame or the end of a keepalive tries again.
  bool keepalive_arm_failed_;
  // The last known state: any thread, read as one snapshot of the two.
  mutable std::mutex state_mutex_;
  bool state_known_;
  std::int64_t state_value_;
  // The sequences under way.
  std::vector<std::unique_ptr<sequence_run_t>> runs_;
  // The subscribers and the handlers of unknown frames, of edges and of the
  // link state, the keepalive generator: changed from any thread, read on the
  // strand (copied out, called outside the lock).
  struct subscriber_t
  {
    std::uint64_t id = 0;
    // Null while the layer in use has not the row of `identifier`.
    ::lumex::core::protocol::index::row_t const *row = nullptr;
    char const *identifier = nullptr;
    handler_ptr handler;
  };
  struct edge_subscriber_t
  {
    std::uint64_t id = 0;
    // Null while the layer in use has not the row of `identifier`.
    ::lumex::core::protocol::index::row_t const *row = nullptr;
    char const *identifier = nullptr;
    std::shared_ptr<edge_handler_t const> handler;
  };
  mutable std::mutex handlers_mutex_;
  std::vector<subscriber_t> subscribers_;
  handler_ptr unknown_;
  std::vector<edge_subscriber_t> edge_subscribers_;
  std::shared_ptr<link_state_handler_t const> link_handler_;
  std::shared_ptr<keepalive_generator_t const> keepalive_generator_;
  // What close () and the shutdown take out of the members under the lock.
  struct feature_handlers_t
  {
    std::vector<edge_subscriber_t> edges;
    std::shared_ptr<link_state_handler_t const> link;
    std::shared_ptr<keepalive_generator_t const> generator;
  };
  // Set by close (), under the mutex: no handler is kept any more.
  bool handlers_closed_;
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
