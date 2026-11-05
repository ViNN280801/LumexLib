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
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/detail/LumexDialogueImpl.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"

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

// A log step while the state of the engine moves: a message that cannot be
// built (out of memory) must not stop the step, so it is dropped.
#define LUMEX_DIALOGUE_LOG_QUIET(...)                                         \
  do                                                                          \
    {                                                                         \
      try                                                                     \
        {                                                                     \
          LUMEX_TRANSPORT_LOG (__VA_ARGS__);                                  \
        }                                                                     \
      catch (...)                                                             \
        {                                                                     \
        }                                                                     \
    }                                                                         \
  while (false)

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
namespace
{
namespace tdetail = ::lumex::applied::transport::detail;
namespace pindex = ::lumex::core::protocol::index;
using ::lumex::core::protocol::error::protocol_errc;

/// The component of the log records.
char const *const kcomponent = "transport.dialogue";

std::error_code
dialogue_error (protocol_errc value)
{
  return ::lumex::core::protocol::error::make_error_code (value);
}

// The completion of a wait of a feature: the engine asks the clock itself.
struct feature_wait_done_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  void (dialogue_impl::*method) (std::error_code);

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void
  operator() (std::error_code error)
  {
    std::shared_ptr<dialogue_impl> const alive = impl.lock ();
    if (alive)
      ((*alive).*method) (error);
  }
};

// The end of a keepalive exchange.
struct keepalive_end_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void
  operator() (std::error_code error, link_frame_t)
  {
    std::shared_ptr<dialogue_impl> const alive = impl.lock ();
    if (alive)
      alive->keepalive_ended (error);
  }
};

struct keepalive_start_step_t
{
  std::shared_ptr<dialogue_impl> impl;

  void
  operator() ()
  {
    impl->run_keepalive_start ();
  }
};

struct select_layer_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  pindex::protocol_index_t const *index;
  protocol_features_t const *features;

  void
  operator() ()
  {
    impl->run_select_layer (index, features);
  }
};

struct state_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::int64_t state;

  void
  operator() ()
  {
    impl->run_state_set (state);
  }
};

std::string
row_name (pindex::row_t const *row)
{
  return row != nullptr ? std::string (row->entry->specification_name)
                        : std::string ("a request");
}

timing::time_point_t
after (timing::time_point_t from, std::uint64_t milliseconds) LUMEX_NOEXCEPT
{
  return tdetail::saturating_add (
      from, std::chrono::milliseconds (
                static_cast<std::chrono::milliseconds::rep> (milliseconds)));
}
} // namespace

// --- what the table says
// -----------------------------------------------------

void
dialogue_impl::build_watches (
    protocol_features_t const *features,
    std::shared_ptr<keepalive_generator_t const> &dropped)
{
  std::vector<row_watch_t> made;
  ::lumex::core::span::view::span<pindex::row_t const> const rows
      = index_now ().rows ();
  // A watch for a row that needs one, in table order.
  for (std::size_t i = 0; i < rows.size (); ++i)
    {
      pindex::row_t const &row = rows[i];
      row_watch_t watch;
      watch.row = &row;
      if (row.entry->options.has_stream)
        watch.stream_interval_ms = row.entry->options.stream_interval_ms;
      row_features_t const *own = nullptr;
      if (features != nullptr)
        for (std::size_t j = 0; j < features->row_count && own == nullptr; ++j)
          if (std::strcmp (features->rows[j].identifier, row.entry->identifier)
              == 0)
            own = &features->rows[j];
      bool const reads_state
          = own != nullptr && features->state_row != nullptr
            && std::strcmp (features->state_row, row.entry->identifier) == 0;
      if (own != nullptr
          && (own->min_interval_ms != 0 || own->edge_count != 0
              || own->enables != nullptr || reads_state))
        watch.features = own;
      if (reads_state)
        watch.reads_state = true;
      if (watch.features == nullptr && watch.stream_interval_ms == 0)
        continue;
      if (own != nullptr)
        for (std::size_t j = 0; j < own->edge_count; ++j)
          watch.fields.push_back (own->edges[j].field);
      if (reads_state)
        watch.fields.push_back (features->state_field);
      watch.levels.assign (watch.fields.size (), 0);
      made.push_back (std::move (watch));
    }
  watches_.swap (made);
  watch_index_.clear ();
  for (std::size_t i = 0; i < watches_.size (); ++i)
    watch_index_[watches_[i].row] = i;
  row_features_t const *keepalive = nullptr;
  if (features != nullptr && features->keepalive_row != nullptr)
    for (std::size_t j = 0; j < features->row_count; ++j)
      if (std::strcmp (features->rows[j].identifier, features->keepalive_row)
          == 0)
        keepalive = &features->rows[j];
  // set_keepalive reads it from any thread, and sets the generator under the
  // same lock: the generator of the row that was goes in the same section, or
  // one set for the new row in between would go with it.
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  char const *const old_identifier
      = keepalive_row_ != nullptr ? keepalive_row_->identifier : nullptr;
  keepalive_row_ = keepalive;
  if (keepalive == nullptr || old_identifier == nullptr
      || std::strcmp (keepalive->identifier, old_identifier) != 0)
    dropped.swap (keepalive_generator_);
}

// --- the change of layer
// -------------------------------------------------------

void
dialogue_impl::start_select_layer (pindex::protocol_index_t const &index,
                                   protocol_features_t const *features)
{
  select_layer_step_t step = { shared_from_this (), &index, features };
  asio::post (strand_, std::move (step));
}

pindex::row_t const *
dialogue_impl::row_again (pindex::row_t const *row) const
{
  return row != nullptr ? index_now ().find_name (row->entry->identifier)
                        : nullptr;
}

// What was learned of a row stays with the row of the same identifier.
void
dialogue_impl::take_old_watches (std::vector<row_watch_t> const &old)
{
  for (std::size_t i = 0; i < old.size (); ++i)
    {
      row_watch_t *const now = watch_of (row_again (old[i].row));
      if (now == nullptr)
        continue;
      now->sent = old[i].sent;
      now->last_sent = old[i].last_sent;
      if (now->stream_interval_ms != 0 && old[i].watching)
        {
          now->watching = true;
          now->last_frame = old[i].last_frame;
          now->gap_counted = old[i].gap_counted;
          if (!now->gap_counted)
            arm_gap_timer (after (
                now->last_frame,
                2 * static_cast<std::uint64_t> (now->stream_interval_ms)));
        }
      if (now->fields == old[i].fields)
        {
          now->levels = old[i].levels;
          now->levels_known = old[i].levels_known;
        }
    }
}

// What is under way follows by the identifier of its row.
void
dialogue_impl::follow_layer ()
{
  std::deque<record_ptr> queued;
  queued.swap (queue_);
  for (std::deque<record_ptr>::iterator it = queued.begin ();
       it != queued.end (); ++it)
    {
      record_ptr record (std::move (*it));
      pindex::row_t const *const row = row_again (record->row);
      pindex::row_t const *const reply = row_again (record->reply_row);
      if ((record->row != nullptr && row == nullptr)
          || (record->reply_row != nullptr && reply == nullptr))
        {
          finish (std::move (record),
                  dialogue_error (protocol_errc::not_supported),
                  link_frame_t ());
          continue;
        }
      record->row = row;
      record->reply_row = reply;
      if (record->row_policy && record->attempts == 0)
        {
          // Not started: it starts in this layer, with the timeout and the
          // retries of this layer; one that started keeps what it has.
          try
            {
              record->policy = row_policy (row);
              record->allowed
                  = 1 + static_cast<std::uint64_t> (record->policy->retries);
            }
          catch (...)
            {
              // Out of memory: the policy of the layer it was queued in.
            }
        }
      queue_.push_back (std::move (record));
    }
  for (std::size_t at = flight_.size (); at != 0; --at)
    {
      exchange_record_t &record = *flight_[at - 1];
      pindex::row_t const *const row = row_again (record.row);
      pindex::row_t const *const reply = row_again (record.reply_row);
      if ((record.row != nullptr && row == nullptr)
          || (record.reply_row != nullptr && reply == nullptr))
        {
          finish_in_flight (at - 1,
                            dialogue_error (protocol_errc::not_supported),
                            link_frame_t ());
          continue;
        }
      record.row = row;
      record.reply_row = reply;
    }
  std::deque<pindex::row_t const *> late;
  for (std::size_t i = 0; i < late_rows_.size (); ++i)
    {
      pindex::row_t const *const row = row_again (late_rows_[i]);
      if (row != nullptr)
        late.push_back (row);
    }
  late_rows_.swap (late);
  for (std::size_t i = 0; i < runs_.size (); ++i)
    {
      sequence_run_t &run = *runs_[i];
      run.expected = row_again (run.expected);
      run.held_row = row_again (run.held_row);
      run.has_held = run.has_held && run.held_row != nullptr;
      std::vector<std::pair<pindex::row_t const *, link_frame_t>> replies;
      for (std::size_t r = 0; r < run.replies.size (); ++r)
        {
          pindex::row_t const *const row = row_again (run.replies[r].first);
          if (row != nullptr)
            replies.push_back (std::make_pair (row, run.replies[r].second));
        }
      run.replies.swap (replies);
    }
}

// The waits whose time the layer changes start again.
void
dialogue_impl::apply_layer_times (std::uint32_t watchdog_ms,
                                  std::uint32_t keepalive_ms,
                                  row_features_t const *old_keepalive)
{
  if (watchdog_ms != watchdog_ms_)
    {
      watchdog_timer_.cancel ();
      watchdog_armed_ = false;
      watchdog_ms_ = watchdog_ms;
      if (link_up ())
        arm_watchdog (after (timers_.now (), watchdog_ms_));
    }
  if (keepalive_ms != keepalive_ms_ || old_keepalive != keepalive_row_)
    {
      keepalive_timer_.cancel ();
      keepalive_armed_ = false;
      keepalive_ms_ = keepalive_ms;
      arm_keepalive ();
    }
}

void
dialogue_impl::run_select_layer (pindex::protocol_index_t const *index,
                                 protocol_features_t const *features)
{
  if (closed_ || (index == &index_now () && features == features_))
    return;
  // Another layer found on a new connection is another device: what waited
  // for the old one does not follow it.
  if (restoring_)
    fail_waiting (dialogue_error (protocol_errc::link_lost));
  // The handlers that go die after the lock is released: they may call the
  // engine.
  std::shared_ptr<keepalive_generator_t const> dropped;
  {
    std::lock_guard<std::mutex> const lock (handlers_mutex_);
    index_.store (index, std::memory_order_release);
    for (std::size_t i = 0; i < subscribers_.size (); ++i)
      subscribers_[i].row = index->find_name (subscribers_[i].identifier);
    for (std::size_t i = 0; i < edge_subscribers_.size (); ++i)
      edge_subscribers_[i].row
          = index->find_name (edge_subscribers_[i].identifier);
  }
  std::vector<row_watch_t> old;
  old.swap (watches_);
  row_features_t const *const old_keepalive = keepalive_row_;
  features_ = features;
  build_watches (features, dropped);
  // Rows of a table that keeps them: the old pointer still names the row until
  // follow_layer moves it.
  take_old_watches (old);
  min_interval_ms_ = features != nullptr ? features->min_interval_ms : 0;
  if (features != nullptr)
    {
      // A value that is not what the layer in use gives was set by the
      // application and stays; one that is takes what the new layer gives.
      layer_values_t const gives = layer_values_of (*features);
      retry_policy_t made;
      if (defaults_)
        made = *defaults_;
      if (made.timeout_ms == table_.timeout_ms)
        made.timeout_ms = gives.timeout_ms;
      if (made.retries == table_.retries)
        made.retries = gives.retries;
      defaults_ = std::make_shared<retry_policy_t const> (std::move (made));
      if (max_in_flight_ == table_.max_in_flight)
        max_in_flight_ = gives.max_in_flight;
      if (late_window_ms_ == table_.late_window_ms)
        late_window_ms_ = gives.late_window_ms;
      table_ = gives;
    }
  apply_layer_times (features != nullptr ? features->watchdog_ms : 0,
                     features != nullptr ? features->keepalive_period_ms : 0,
                     old_keepalive);
  follow_layer ();
  pump ();
}

row_watch_t *
dialogue_impl::watch_of (pindex::row_t const *row) LUMEX_NOEXCEPT
{
  return const_cast<row_watch_t *> (
      static_cast<dialogue_impl const *> (this)->watch_of (row));
}

row_watch_t const *
dialogue_impl::watch_of (pindex::row_t const *row) const LUMEX_NOEXCEPT
{
  if (row == nullptr)
    return nullptr;
  std::unordered_map<pindex::row_t const *, std::size_t>::const_iterator const
      found
      = watch_index_.find (row);
  return found != watch_index_.end () ? &watches_[found->second] : nullptr;
}

std::shared_ptr<retry_policy_t const>
dialogue_impl::row_policy (pindex::row_t const *row) const
{
  // The extra retries stay with the defaults (next_attempt): a copy here
  // would lose what a predicate with state counted.
  retry_policy_t made;
  if (defaults_)
    {
      made.timeout_ms = defaults_->timeout_ms;
      made.retries = defaults_->retries;
    }
  if (row != nullptr)
    {
      if (row->entry->options.has_timeout)
        made.timeout_ms = row->entry->options.timeout_ms;
      if (row->entry->options.has_retries)
        made.retries = row->entry->options.retries;
    }
  return std::make_shared<retry_policy_t const> (std::move (made));
}

void
dialogue_impl::fill_exchange (exchange_t &request,
                              row_features_t const *features,
                              pindex::row_t const *row) const
{
  request.row = row;
  request.row_policy = true;
  if (features == nullptr)
    return;
  request.how = features->how;
  request.matcher = features->matcher;
  request.reader = features->reader;
  if (features->reply_row != nullptr)
    request.reply_row = index_now ().find_name (features->reply_row);
}

// --- the rate limits
// ---------------------------------------------------------

timing::time_point_t
dialogue_impl::rate_due (exchange_record_t const &record) const LUMEX_NOEXCEPT
{
  timing::time_point_t due = (timing::time_point_t::min)();
  if (min_interval_ms_ > 0 && any_sent_)
    due = after (last_sent_, min_interval_ms_);
  row_watch_t const *const watch = watch_of (record.row);
  if (watch != nullptr && watch->sent && watch->features != nullptr
      && watch->features->min_interval_ms > 0)
    {
      timing::time_point_t const own
          = after (watch->last_sent, watch->features->min_interval_ms);
      if (own > due)
        due = own;
    }
  return due;
}

void
dialogue_impl::note_sent (exchange_record_t const &record) LUMEX_NOEXCEPT
{
  timing::time_point_t const now = timers_.now ();
  any_sent_ = true;
  last_sent_ = now;
  row_watch_t *const watch = watch_of (record.row);
  if (watch != nullptr)
    {
      watch->sent = true;
      watch->last_sent = now;
    }
}

void
dialogue_impl::note_rate_limited (exchange_record_t &record)
{
  if (record.rate_counted)
    return;
  record.rate_counted = true;
  dialogue_counters_t::bump (counters_.rate_limited);
  // A warning at most once a second of the timer source, as for the frames of
  // no row; it carries the number of the requests that went to debug since.
  timing::time_point_t const now = timers_.now ();
  bool const warn = !rate_warned_ || now < rate_warned_at_
                    || now - rate_warned_at_ >= std::chrono::seconds (1);
  if (!warn)
    {
      ++rate_skipped_;
      LUMEX_DIALOGUE_LOG_QUIET (debug, kcomponent, name_.c_str (),
                                "a rate limit delays " + row_name (record.row),
                                log::log_bytes_t ());
      return;
    }
  std::uint64_t const skipped = rate_skipped_;
  rate_warned_ = true;
  rate_warned_at_ = now;
  rate_skipped_ = 0;
  LUMEX_DIALOGUE_LOG_QUIET (
      warning, kcomponent, name_.c_str (),
      "a rate limit delays " + row_name (record.row)
          + (skipped == 0 ? std::string ()
                          : " (" + std::to_string (skipped)
                                + " more since the last warning, logged at "
                                  "debug)"),
      log::log_bytes_t ());
}

// A wait that comes sooner than this one stays: its completion looks at the
// clock and arms the next one.
void
dialogue_impl::arm_rate_timer (timing::time_point_t at)
{
  if (rate_armed_ && rate_armed_for_ <= at)
    return;
  try
    {
      feature_wait_done_t done
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
              &dialogue_impl::rate_done };
      rate_timer_.async_wait_until (at, std::move (done));
      rate_armed_ = true;
      rate_armed_for_ = at;
    }
  catch (...)
    {
      // Out of memory: nothing would ever start the requests that wait.
      rate_armed_ = false;
      fail_rate_waiters ();
    }
}

void
dialogue_impl::fail_rate_waiters ()
{
  std::error_code const failed
      = make_error_code (std::errc::not_enough_memory);
  timing::time_point_t const now = timers_.now ();
  for (std::size_t i = 0; i < queue_.size ();)
    if (rate_due (*queue_[i]) > now)
      {
        record_ptr record (std::move (queue_[i]));
        queue_.erase (queue_.begin () + static_cast<std::ptrdiff_t> (i));
        finish (std::move (record), failed, link_frame_t ());
      }
    else
      ++i;
  for (std::size_t i = 0; i < flight_.size ();)
    if (flight_[i]->waiting_rate)
      finish_in_flight (i, failed, link_frame_t ());
    else
      ++i;
  pump ();
}

void
dialogue_impl::rate_done (std::error_code error)
{
  if (error)
    return;
  rate_armed_ = false;
  if (closed_)
    return;
  // By number: an attempt that fails at once leaves the flight.
  std::vector<std::uint64_t> waiting;
  for (std::size_t i = 0; i < flight_.size (); ++i)
    if (flight_[i]->waiting_rate)
      waiting.push_back (flight_[i]->id);
  timing::time_point_t const now = timers_.now ();
  for (std::size_t i = 0; i < waiting.size (); ++i)
    {
      std::size_t const at = find_in_flight (waiting[i]);
      if (at != flight_.size () && rate_due (*flight_[at]) <= now)
        start_attempt (*flight_[at]);
    }
  pump ();
}

// --- the state
// ---------------------------------------------------------------

bool
dialogue_impl::refused_by_state (exchange_record_t const &record,
                                 std::int64_t &state) const LUMEX_NOEXCEPT
{
  if (record.row == nullptr)
    return false;
  ::lumex::core::protocol::dsl::states_t const states
      = record.row->entry->options.accepted_states;
  if (states.empty () || !read_state (state))
    return false;
  for (std::size_t i = 0; i < states.size (); ++i)
    if (states[i] == state)
      return false;
  return true;
}

void
dialogue_impl::refuse_by_state (record_ptr record, std::int64_t state)
{
  dialogue_counters_t::bump (counters_.state_refusals);
  LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                            "refused " + row_name (record->row)
                                + " before sending: not accepted in state "
                                + std::to_string (state),
                            log::log_bytes_t ());
  finish (std::move (record),
          dialogue_error (protocol_errc::not_accepted_in_state),
          link_frame_t ());
}

void
dialogue_impl::store_state (std::int64_t state) LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> const lock (state_mutex_);
  state_value_ = state;
  state_known_ = true;
}

void
dialogue_impl::set_state (std::int64_t state)
{
  store_state (state);
  state_step_t step = { shared_from_this (), state };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::clear_state () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> const lock (state_mutex_);
  state_known_ = false;
  state_value_ = 0;
}

bool
dialogue_impl::read_state (std::int64_t &state) const LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> const lock (state_mutex_);
  state = state_value_;
  return state_known_;
}

std::int64_t
dialogue_impl::last_state () const LUMEX_NOEXCEPT
{
  std::int64_t state = 0;
  read_state (state);
  return state;
}

bool
dialogue_impl::state_known () const LUMEX_NOEXCEPT
{
  std::int64_t state = 0;
  return read_state (state);
}

void
dialogue_impl::run_state_set (std::int64_t state)
{
  wake_state_waiters (state);
}

// --- the streams
// -------------------------------------------------------------

void
dialogue_impl::count_gap (row_watch_t &watch)
{
  watch.gap_counted = true;
  dialogue_counters_t::bump (counters_.stream_gaps);
  LUMEX_DIALOGUE_LOG_QUIET (
      warning, kcomponent, name_.c_str (),
      "a gap in the stream " + row_name (watch.row) + ": two intervals of "
          + std::to_string (watch.stream_interval_ms) + " ms without a frame",
      log::log_bytes_t ());
}

void
dialogue_impl::start_watching (row_watch_t &watch, timing::time_point_t now)
{
  watch.watching = true;
  watch.gap_counted = false;
  watch.last_frame = now;
  arm_gap_timer (
      after (now, 2 * static_cast<std::uint64_t> (watch.stream_interval_ms)));
}

void
dialogue_impl::note_stream_frame (row_watch_t &watch)
{
  if (watch.stream_interval_ms == 0)
    return;
  timing::time_point_t const now = timers_.now ();
  // The silence ended with this frame; the wait that would have said so may
  // be on its way.
  if (watch.watching && !watch.gap_counted
      && now >= after (
             watch.last_frame,
             2 * static_cast<std::uint64_t> (watch.stream_interval_ms)))
    count_gap (watch);
  start_watching (watch, now);
}

void
dialogue_impl::arm_gap_timer (timing::time_point_t at)
{
  if (gap_armed_ && gap_armed_for_ <= at)
    return;
  try
    {
      feature_wait_done_t done
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
              &dialogue_impl::gap_done };
      gap_timer_.async_wait_until (at, std::move (done));
      gap_armed_ = true;
      gap_armed_for_ = at;
    }
  catch (...)
    {
      // Out of memory: the next frame of a stream arms it again.
      gap_armed_ = false;
    }
}

void
dialogue_impl::gap_done (std::error_code error)
{
  if (error)
    return;
  gap_armed_ = false;
  if (closed_)
    return;
  timing::time_point_t const now = timers_.now ();
  bool any = false;
  timing::time_point_t earliest = now;
  for (std::size_t i = 0; i < watches_.size (); ++i)
    {
      row_watch_t &watch = watches_[i];
      if (!watch.watching || watch.gap_counted)
        continue;
      timing::time_point_t const due
          = after (watch.last_frame,
                   2 * static_cast<std::uint64_t> (watch.stream_interval_ms));
      if (due <= now)
        {
          count_gap (watch);
          continue;
        }
      if (!any || due < earliest)
        earliest = due;
      any = true;
    }
  if (any)
    arm_gap_timer (earliest);
}

void
dialogue_impl::note_success (exchange_record_t const &record)
{
  row_watch_t const *const watch = watch_of (record.row);
  if (watch == nullptr || watch->features == nullptr
      || watch->features->enables == nullptr)
    return;
  note_enabled_stream (record);
  // Nothing may throw here: the exchange is about to end. A watch that does
  // not start only misses a gap until the first frame of the stream.
  try
    {
      row_watch_t *const stream
          = watch_of (index_now ().find_name (watch->features->enables));
      if (stream != nullptr && stream->stream_interval_ms != 0)
        start_watching (*stream, timers_.now ());
    }
  catch (...)
    {
    }
}

// --- the levels of a frame
// ---------------------------------------------------

void
dialogue_impl::read_levels (row_watch_t &watch, link_frame_t const &frame,
                            std::vector<edge_t> &edges, bool &state_changed,
                            std::int64_t &state)
{
  if (watch.features == nullptr || watch.features->read_levels == nullptr
      || watch.fields.empty ())
    return;
  try
    {
      std::vector<std::int64_t> values (watch.fields.size (), 0);
      ::lumex::core::protocol::field::decode_context_t const context
          = session_.decode_context ();
      if (!watch.features->read_levels (
              ::lumex::core::protocol::layout::bytes_view_t (frame.data (),
                                                             frame.size ()),
              &context, watch.fields.data (), watch.fields.size (),
              values.data ()))
        return;
      if (watch.levels_known)
        for (std::size_t i = 0; i < watch.features->edge_count; ++i)
          {
            edge_spec_t const &spec = watch.features->edges[i];
            if (watch.levels[i] != spec.from || values[i] != spec.to)
              continue;
            edge_t edge;
            edge.row = watch.row;
            edge.field = spec.field;
            edge.from = spec.from;
            edge.to = spec.to;
            edges.push_back (edge);
            dialogue_counters_t::bump (counters_.edge_events);
            LUMEX_DIALOGUE_LOG_QUIET (
                debug, kcomponent, name_.c_str (),
                "an edge of " + row_name (watch.row) + ": field "
                    + std::to_string (spec.field) + " from "
                    + std::to_string (spec.from) + " to "
                    + std::to_string (spec.to),
                log::log_bytes_t ());
          }
      if (watch.reads_state)
        {
          state = values.back ();
          store_state (state);
          state_changed = true;
        }
      watch.levels.swap (values);
      watch.levels_known = true;
    }
  catch (...)
    {
      // Out of memory: this frame says nothing of its levels.
    }
}

std::vector<std::shared_ptr<edge_handler_t const>>
dialogue_impl::edge_handlers_of (pindex::row_t const *row) const
{
  std::vector<std::shared_ptr<edge_handler_t const>> found;
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  for (std::size_t i = 0; i < edge_subscribers_.size (); ++i)
    if (edge_subscribers_[i].row == row)
      found.push_back (edge_subscribers_[i].handler);
  return found;
}

subscription_t
dialogue_impl::subscribe_edge (pindex::row_t const *row,
                               edge_handler_t handler)
{
  subscription_t made;
  if (row == nullptr || !handler)
    return made;
  edge_subscriber_t added;
  added.id = next_subscription_.fetch_add (1, std::memory_order_relaxed) + 1;
  added.handler = std::make_shared<edge_handler_t const> (std::move (handler));
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  added.row = index_now ().find_name (row->entry->identifier);
  if (handlers_closed_ || added.row == nullptr)
    return made;
  added.identifier = added.row->entry->identifier;
  edge_subscribers_.push_back (added);
  made.id = added.id;
  return made;
}

// --- the link
// ----------------------------------------------------------------

void
dialogue_impl::set_link_handler (link_state_handler_t handler)
{
  std::shared_ptr<link_state_handler_t const> const made
      = handler ? std::make_shared<link_state_handler_t const> (
                      std::move (handler))
                : std::shared_ptr<link_state_handler_t const> ();
  std::shared_ptr<link_state_handler_t const> old = made;
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  if (handlers_closed_)
    return;
  link_handler_.swap (old);
}

void
dialogue_impl::notify_link_state (link_state state,
                                  std::error_code const &reason)
{
  std::shared_ptr<link_state_handler_t const> handler;
  {
    std::lock_guard<std::mutex> const lock (handlers_mutex_);
    handler = link_handler_;
  }
  if (handler)
    (*handler) (state, reason);
}

void
dialogue_impl::frame_seen (std::exception_ptr &thrown)
{
  read_failures_ = 0;
  read_restarted_at_once_ = false;
  timing::time_point_t const now = timers_.now ();
  last_frame_at_ = now;
  // Armed already, as a rule; a re-arm that threw comes back with a frame.
  if (watchdog_ms_ > 0)
    arm_watchdog (after (now, watchdog_ms_));
  if (keepalive_arm_failed_)
    arm_keepalive ();
  if (link_up_.exchange (true, std::memory_order_acq_rel))
    return;
  try
    {
      notify_link_state (link_state::up, std::error_code ());
    }
  catch (...)
    {
      thrown = std::current_exception ();
    }
}

void
dialogue_impl::forget_watches () LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < watches_.size (); ++i)
    {
      watches_[i].watching = false;
      watches_[i].gap_counted = false;
      watches_[i].levels_known = false;
    }
}

void
dialogue_impl::schedule_read_restart ()
{
  if (closed_ || read_restart_ms_ == 0 || read_restart_armed_)
    return;
  std::uint64_t delay = read_restart_ms_;
  for (std::uint32_t i = 0; i < read_failures_ && delay < read_restart_max_ms_;
       ++i)
    delay *= 2;
  if (delay > read_restart_max_ms_)
    delay = read_restart_max_ms_;
  ++read_failures_;
  try
    {
      feature_wait_done_t done
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
              &dialogue_impl::read_restart_done };
      read_restart_timer_.async_wait_for (
          std::chrono::milliseconds (
              static_cast<std::chrono::milliseconds::rep> (delay)),
          std::move (done));
      read_restart_armed_ = true;
    }
  catch (...)
    {
      // Out of memory: the next request starts the reading.
      read_restart_armed_ = false;
    }
}

void
dialogue_impl::read_restart_done (std::error_code error)
{
  if (error)
    return;
  read_restart_armed_ = false;
  if (closed_)
    return;
  try
    {
      start_reading ();
    }
  catch (...)
    {
      schedule_read_restart ();
    }
}

// --- the watchdog
// ------------------------------------------------------------

void
dialogue_impl::arm_watchdog (timing::time_point_t at)
{
  if (closed_ || watchdog_ms_ == 0 || watchdog_armed_)
    return;
  try
    {
      feature_wait_done_t done
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
              &dialogue_impl::watchdog_done };
      watchdog_timer_.async_wait_until (at, std::move (done));
      watchdog_armed_ = true;
    }
  catch (...)
    {
      // Out of memory: a frame after a lost link arms it again.
      watchdog_armed_ = false;
      LUMEX_DIALOGUE_LOG_QUIET (error, kcomponent, name_.c_str (),
                                "the watchdog could not be armed",
                                log::log_bytes_t ());
    }
}

void
dialogue_impl::watchdog_done (std::error_code error)
{
  if (error)
    return;
  watchdog_armed_ = false;
  // watchdog_ms_ of 0: a wait that fired just as a layer without a watchdog
  // was selected.
  if (closed_ || !link_up () || watchdog_ms_ == 0)
    return;
  timing::time_point_t const deadline = after (last_frame_at_, watchdog_ms_);
  if (timers_.now () < deadline)
    {
      arm_watchdog (deadline);
      return;
    }
  dialogue_counters_t::bump (counters_.watchdog_expiries);
  LUMEX_DIALOGUE_LOG_QUIET (error, kcomponent, name_.c_str (),
                            "the watchdog ended the link: no frame for "
                                + std::to_string (watchdog_ms_) + " ms",
                            log::log_bytes_t ());
  // The transport first: a queued request that starts now fails its write.
  link_.impl ()->drop_connection ();
  if (reconnects_)
    connection_lost (dialogue_error (protocol_errc::link_lost));
  else
    link_lost (dialogue_error (protocol_errc::link_lost));
}

// --- the keepalive
// -----------------------------------------------------------

bool
dialogue_impl::set_keepalive (keepalive_generator_t generator)
{
  std::shared_ptr<keepalive_generator_t const> old
      = generator ? std::make_shared<keepalive_generator_t const> (
                        std::move (generator))
                  : std::shared_ptr<keepalive_generator_t const> ();
  {
    std::lock_guard<std::mutex> const lock (handlers_mutex_);
    if (handlers_closed_ || keepalive_row_ == nullptr)
      return false;
    keepalive_generator_.swap (old);
  }
  keepalive_start_step_t step = { shared_from_this () };
  asio::post (strand_, std::move (step));
  return true;
}

void
dialogue_impl::run_keepalive_start ()
{
  arm_keepalive ();
}

void
dialogue_impl::arm_keepalive ()
{
  keepalive_arm_failed_ = false;
  if (closed_ || keepalive_row_ == nullptr || keepalive_ms_ == 0
      || keepalive_armed_)
    return;
  if (keepalive_row_->empty_request == nullptr)
    {
      std::lock_guard<std::mutex> const lock (handlers_mutex_);
      if (!keepalive_generator_)
        return;
    }
  try
    {
      feature_wait_done_t done
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
              &dialogue_impl::keepalive_done };
      keepalive_timer_.async_wait_for (
          std::chrono::milliseconds (keepalive_ms_), std::move (done));
      keepalive_armed_ = true;
    }
  catch (...)
    {
      // Out of memory: the next frame, the end of a pending keepalive or
      // set_keepalive arms it again.
      keepalive_armed_ = false;
      keepalive_arm_failed_ = true;
      LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                "the keepalive could not be armed",
                                log::log_bytes_t ());
    }
}

void
dialogue_impl::keepalive_done (std::error_code error)
{
  if (error)
    return;
  keepalive_armed_ = false;
  // No row: a wait that fired just as a layer without a keepalive was
  // selected.
  if (closed_ || keepalive_row_ == nullptr)
    return;
  // The next period first: a generator that throws must not stop it.
  arm_keepalive ();
  if (keepalive_pending_)
    {
      LUMEX_DIALOGUE_LOG_QUIET (debug, kcomponent, name_.c_str (),
                                "the previous keepalive is still pending",
                                log::log_bytes_t ());
      return;
    }
  // A link that is lost and closed would only fail the write; a link that is
  // lost and open may come back by this very request.
  if (!link_up () && !link_.is_open ())
    return;
  // A dialogue that does not know its device sends nothing by itself.
  if (!gate_open ())
    return;
  std::shared_ptr<keepalive_generator_t const> generator;
  {
    std::lock_guard<std::mutex> const lock (handlers_mutex_);
    generator = keepalive_generator_;
  }
  if (!generator && keepalive_row_->empty_request == nullptr)
    return;
  exchange_t request;
  ::lumex::core::protocol::field::decode_context_t const context
      = session_.decode_context ();
  std::error_code const made
      = generator ? (*generator) (request.message, context)
                  : keepalive_row_->empty_request (request.message, context);
  if (made)
    {
      LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                "the keepalive was skipped: "
                                    + made.message (),
                                log::log_bytes_t ());
      return;
    }
  fill_exchange (request, keepalive_row_,
                 index_now ().find_name (keepalive_row_->identifier));
  keepalive_end_t ended
      = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_ };
  exchange_handler_t handler (std::move (ended));
  std::uint64_t const id
      = next_id_.fetch_add (1, std::memory_order_relaxed) + 1;
  keepalive_pending_ = true;
  dialogue_counters_t::bump (counters_.keepalives);
  LUMEX_DIALOGUE_LOG_QUIET (debug, kcomponent, name_.c_str (), "keepalive",
                            log::log_bytes_t ());
  try
    {
      run_enqueue (id, request, handler, true);
    }
  catch (...)
    {
      // Out of memory: the next period tries again.
      keepalive_pending_ = false;
      throw;
    }
}

void
dialogue_impl::keepalive_ended (std::error_code error)
{
  keepalive_pending_ = false;
  if (keepalive_arm_failed_)
    arm_keepalive ();
  if (error)
    LUMEX_DIALOGUE_LOG_QUIET (debug, kcomponent, name_.c_str (),
                              "a keepalive ended: " + error.message (),
                              log::log_bytes_t ());
}

// --- the end
// -----------------------------------------------------------------

void
dialogue_impl::cancel_feature_timers () LUMEX_NOEXCEPT
{
  rate_timer_.cancel ();
  gap_timer_.cancel ();
  read_restart_timer_.cancel ();
  watchdog_timer_.cancel ();
  keepalive_timer_.cancel ();
}

void
dialogue_impl::take_feature_handlers (feature_handlers_t &out) LUMEX_NOEXCEPT
{
  out.edges.swap (edge_subscribers_);
  out.link.swap (link_handler_);
  out.generator.swap (keepalive_generator_);
  out.reconnect.swap (reconnect_handler_);
}
} // namespace detail
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#undef LUMEX_DIALOGUE_LOG_QUIET

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
