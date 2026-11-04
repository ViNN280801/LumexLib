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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/detail/LumexDialogueImpl.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/contracts/LumexContracts"
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

// A log step between the change of the state of an exchange and its end: a
// message that cannot be built (out of memory) must not leave the exchange
// waiting with no timer, so it is dropped.
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
using error::transport_errc;
using ::lumex::core::protocol::error::protocol_errc;

/// The component of the log records.
char const *const kcomponent = "transport.dialogue";

/// How many rows of abandoned attempts the engine remembers at most.
std::size_t const klate_rows = 32;

std::error_code
dialogue_error (protocol_errc value)
{
  return ::lumex::core::protocol::error::make_error_code (value);
}

// --- the posted steps ------------------------------------------------------

struct start_step_t
{
  std::shared_ptr<dialogue_impl> impl;

  void
  operator() ()
  {
    impl->run_start ();
  }
};

struct enqueue_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::uint64_t id;
  exchange_t request;
  exchange_handler_t handler;

  void
  operator() ()
  {
    impl->run_enqueue (id, request, handler);
  }
};

struct cancel_step_t
{
  std::shared_ptr<dialogue_impl> impl;

  void
  operator() ()
  {
    impl->run_cancel ();
  }
};

struct close_step_t
{
  std::shared_ptr<dialogue_impl> impl;

  void
  operator() ()
  {
    impl->run_close ();
  }
};

// The cancellation of one exchange, on the strand: nothing when its result
// was handed on since the signal (the mark is gone or settled).
struct cancel_exchange_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::uint64_t id;
  std::weak_ptr<tdetail::operation_mark> mark;
  asio::cancellation_type_t type;

  void
  operator() ()
  {
    if (tdetail::take_cancellation_request (mark))
      impl->run_cancel_exchange (id, type);
  }
};

// --- the cancellation slot -------------------------------------------------

// The handler the engine puts in the cancellation slot of an exchange's
// handler. It runs on the thread that emits the signal and only posts to the
// strand, and only while the exchange is pending: not once its result was
// handed on. A terminal or partial cancellation ends the exchange; a total
// one only while it waits in the queue (nothing was written yet).
class exchange_cancellation_t
{
public:
  exchange_cancellation_t (std::shared_ptr<dialogue_impl> const &impl,
                           std::uint64_t id,
                           tdetail::operation_mark_ptr const &mark)
      : impl_ (impl), id_ (id), mark_ (mark)
  {
  }

  void
  operator() (asio::cancellation_type_t type)
  {
    if ((type
         & (asio::cancellation_type::terminal
            | asio::cancellation_type::partial
            | asio::cancellation_type::total))
        == asio::cancellation_type::none)
      return;
    std::shared_ptr<dialogue_impl> const impl = impl_.lock ();
    if (!impl || !tdetail::request_cancellation (mark_))
      return;
    try
      {
        impl->request_exchange_cancel (id_, mark_, type);
      }
    catch (...)
      {
        // Out of memory: the exchange is not cancelled; a later signal may
        // try again.
        tdetail::take_cancellation_request (mark_);
      }
  }

private:
  std::weak_ptr<dialogue_impl> impl_;
  std::uint64_t id_;
  std::weak_ptr<tdetail::operation_mark> mark_;
};

// --- the handlers given to the link and the timers -------------------------

struct read_done_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t generation;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }

  void
  operator() (std::error_code error, link_frame_t frame)
  {
    std::shared_ptr<dialogue_impl> const alive = impl.lock ();
    if (alive)
      alive->read_done (generation, error, std::move (frame));
  }
};

struct write_done_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t id;
  std::uint64_t epoch;

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
      alive->write_done (id, epoch, error);
  }
};

struct timeout_done_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t id;
  std::uint64_t epoch;

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
      alive->timeout_done (id, epoch, error);
  }
};

struct window_done_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t window;

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
      alive->window_done (window, error);
  }
};

// Puts the engine's slot handler in the cancellation slot of the erased
// handler (Asio chains it to the slot of the user's handler), and remembers
// the mark by that slot for the completion.
void
take_cancellation (std::shared_ptr<dialogue_impl> const &impl,
                   exchange_handler_t &handler, std::uint64_t id,
                   tdetail::operation_mark_ptr const &mark,
                   tdetail::mark_registry &marks)
{
  if (!mark)
    return;
  asio::cancellation_slot slot
      = asio::get_associated_cancellation_slot (handler);
  if (!slot.is_connected ())
    return;
  slot.template emplace<exchange_cancellation_t> (impl, id, mark);
  marks.watch (slot, mark);
}

std::string
row_name (pindex::row_t const *row)
{
  return row != nullptr ? std::string (row->entry->specification_name)
                        : std::string ("a request");
}
} // namespace

// --- dialogue_counters_t ---------------------------------------------------

dialogue_counters_t::dialogue_counters_t () LUMEX_NOEXCEPT
    : requests (0),
      replies (0),
      retries (0),
      timeouts (0),
      retries_exhausted (0),
      naks (0),
      unexpected_replies (0),
      late_replies (0),
      unknown_frames (0),
      events (0),
      stream_gaps (0),
      rate_limited (0),
      reconnects (0),
      queue_refusals (0),
      state_refusals (0),
      edge_events (0),
      keepalives (0),
      watchdog_expiries (0),
      max_queued (0)
{
}

void
dialogue_counters_t::bump (std::atomic<std::uint64_t> &counter) LUMEX_NOEXCEPT
{
  counter.fetch_add (1, std::memory_order_relaxed);
}

void
dialogue_counters_t::note_queued (std::size_t queued) LUMEX_NOEXCEPT
{
  std::size_t seen = max_queued.load (std::memory_order_relaxed);
  while (queued > seen
         && !max_queued.compare_exchange_weak (seen, queued,
                                               std::memory_order_relaxed))
    {
    }
}

dialogue_statistics_t
dialogue_counters_t::snapshot () const LUMEX_NOEXCEPT
{
  dialogue_statistics_t out;
  out.requests = requests.load (std::memory_order_relaxed);
  out.replies = replies.load (std::memory_order_relaxed);
  out.retries = retries.load (std::memory_order_relaxed);
  out.timeouts = timeouts.load (std::memory_order_relaxed);
  out.retries_exhausted = retries_exhausted.load (std::memory_order_relaxed);
  out.naks = naks.load (std::memory_order_relaxed);
  out.unexpected_replies = unexpected_replies.load (std::memory_order_relaxed);
  out.late_replies = late_replies.load (std::memory_order_relaxed);
  out.unknown_frames = unknown_frames.load (std::memory_order_relaxed);
  out.events = events.load (std::memory_order_relaxed);
  out.stream_gaps = stream_gaps.load (std::memory_order_relaxed);
  out.rate_limited = rate_limited.load (std::memory_order_relaxed);
  out.reconnects = reconnects.load (std::memory_order_relaxed);
  out.queue_refusals = queue_refusals.load (std::memory_order_relaxed);
  out.state_refusals = state_refusals.load (std::memory_order_relaxed);
  out.edge_events = edge_events.load (std::memory_order_relaxed);
  out.keepalives = keepalives.load (std::memory_order_relaxed);
  out.watchdog_expiries = watchdog_expiries.load (std::memory_order_relaxed);
  out.max_queued = max_queued.load (std::memory_order_relaxed);
  return out;
}

void
dialogue_counters_t::reset () LUMEX_NOEXCEPT
{
  std::atomic<std::uint64_t> *const all[] = { &requests,
                                              &replies,
                                              &retries,
                                              &timeouts,
                                              &retries_exhausted,
                                              &naks,
                                              &unexpected_replies,
                                              &late_replies,
                                              &unknown_frames,
                                              &events,
                                              &stream_gaps,
                                              &rate_limited,
                                              &reconnects,
                                              &queue_refusals,
                                              &state_refusals,
                                              &edge_events,
                                              &keepalives,
                                              &watchdog_expiries };
  for (std::size_t i = 0; i < sizeof (all) / sizeof (all[0]); ++i)
    all[i]->store (0, std::memory_order_relaxed);
  max_queued.store (0, std::memory_order_relaxed);
}

// --- dialogue_impl ---------------------------------------------------------

std::shared_ptr<dialogue_impl>
dialogue_impl::make (frame_link link, pindex::protocol_index_t const &index,
                     dialogue_options_t const &options)
{
  LUMEX_CONTRACT_ASSERT (!link.empty ());
  std::shared_ptr<dialogue_impl> const made
      = std::make_shared<dialogue_impl> (std::move (link), index, options);
  // At once, not with the first exchange: a subscriber may own the owner of
  // the dialogue before any exchange is started.
  made->enlist ();
  start_step_t step = { made };
  asio::post (made->strand_, std::move (step));
  return made;
}

dialogue_impl::dialogue_impl (frame_link link,
                              pindex::protocol_index_t const &index,
                              dialogue_options_t const &options)
    : link_ (std::move (link)), strand_ (link_.get_strand ()), index_ (&index),
      name_ (options.name),
      max_in_flight_ (options.max_in_flight == 0 ? 1 : options.max_in_flight),
      queue_bound_ (options.queue_bound),
      late_window_ms_ (options.late_reply_window_ms), timers_ (options.timers),
      defaults_ (std::make_shared<retry_policy_t const> (options.defaults)),
      window_timer_ (options.timers.make_timer (executor_type (strand_))),
      window_open_ (false), window_sequence_ (0), reading_ (false),
      read_generation_ (0), closed_ (false), attempt_sequence_ (0),
      timer_sequence_ (0), unknown_warned_ (false), unknown_warned_at_ (),
      unknown_skipped_ (0), next_id_ (0), next_subscription_ (0),
      features_ (options.features),
      min_interval_ms_ (
          options.features != nullptr ? options.features->min_interval_ms : 0),
      any_sent_ (false), last_sent_ (),
      rate_timer_ (options.timers.make_timer (executor_type (strand_))),
      rate_armed_ (false), rate_armed_for_ (), rate_warned_ (false),
      rate_warned_at_ (), rate_skipped_ (0),
      gap_timer_ (options.timers.make_timer (executor_type (strand_))),
      gap_armed_ (false), gap_armed_for_ (), link_up_ (true),
      read_restart_ms_ (options.read_restart_ms),
      read_restart_max_ms_ (
          (std::max)(options.read_restart_max_ms, options.read_restart_ms)),
      read_restart_timer_ (
          options.timers.make_timer (executor_type (strand_))),
      read_restart_armed_ (false), read_failures_ (0),
      read_restarted_at_once_ (false),
      watchdog_ms_ (options.features != nullptr ? options.features->watchdog_ms
                                                : 0),
      watchdog_timer_ (options.timers.make_timer (executor_type (strand_))),
      watchdog_armed_ (false), last_frame_at_ (),
      keepalive_ms_ (options.features != nullptr
                         ? options.features->keepalive_period_ms
                         : 0),
      keepalive_row_ (nullptr),
      keepalive_timer_ (options.timers.make_timer (executor_type (strand_))),
      keepalive_armed_ (false), keepalive_pending_ (false),
      keepalive_arm_failed_ (false), state_known_ (false), state_value_ (0),
      handlers_closed_ (false), enlisted_ (false)
{
  build_watches (options.features);
}

dialogue_impl::~dialogue_impl () {}

executor_type
dialogue_impl::get_executor () const
{
  return link_.get_executor ();
}

void
dialogue_impl::enlist ()
{
  tdetail::enlist_keeper (enlisted_, strand_, shared_from_this ());
}

// Strand data, but nothing runs the strand any more: the io_context is being
// destroyed. The handlers are taken first and die at the end, so a handler
// that destroys the last owner of the engine finds the members empty.
void
dialogue_impl::drop_kept_handlers () LUMEX_NOEXCEPT
{
  std::vector<exchange_handler_t> taken;
  // The policies too: an extra_retries that owns the owner of the dialogue
  // is a cycle as well.
  std::vector<std::shared_ptr<retry_policy_t const>> policies;
  try
    {
      taken.reserve (queue_.size () + flight_.size ());
      policies.reserve (queue_.size () + flight_.size () + 1);
      policies.push_back (defaults_);
    }
  catch (...)
    {
    }
  defaults_.reset ();
  for (std::deque<record_ptr>::iterator it = queue_.begin ();
       it != queue_.end (); ++it)
    {
      exchange_handler_t handler (std::move ((*it)->handler));
      std::shared_ptr<retry_policy_t const> policy (std::move ((*it)->policy));
      try
        {
          taken.push_back (std::move (handler));
          policies.push_back (std::move (policy));
        }
      catch (...)
        {
        }
    }
  for (std::vector<record_ptr>::iterator it = flight_.begin ();
       it != flight_.end (); ++it)
    {
      exchange_handler_t handler (std::move ((*it)->handler));
      std::shared_ptr<retry_policy_t const> policy (std::move ((*it)->policy));
      try
        {
          taken.push_back (std::move (handler));
          policies.push_back (std::move (policy));
        }
      catch (...)
        {
        }
    }
  // The sequences: their handlers and the functions of their steps.
  std::vector<std::unique_ptr<sequence_run_t>> runs;
  runs.swap (runs_);
  // The subscribers too: one that owns the owner of the dialogue is a cycle
  // as well; so is the generator of the keepalive, a handler of edges or of
  // the link state.
  std::vector<subscriber_t> subscribers;
  handler_ptr unknown;
  feature_handlers_t kept;
  try
    {
      std::lock_guard<std::mutex> const lock (handlers_mutex_);
      handlers_closed_ = true;
      subscribers.swap (subscribers_);
      unknown.swap (unknown_);
      take_feature_handlers (kept);
    }
  catch (...)
    {
    }
}

void
dialogue_impl::enqueue (exchange_t request, exchange_handler_t handler,
                        tdetail::operation_mark_ptr const &mark)
{
  enlist ();
  std::uint64_t const id
      = next_id_.fetch_add (1, std::memory_order_relaxed) + 1;
  std::shared_ptr<dialogue_impl> const self = shared_from_this ();
  take_cancellation (self, handler, id, mark, marks_);
  enqueue_step_t step = { self, id, std::move (request), std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::start_cancel ()
{
  cancel_step_t step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::start_close ()
{
  close_step_t step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::request_exchange_cancel (
    std::uint64_t id, std::weak_ptr<tdetail::operation_mark> mark,
    asio::cancellation_type_t type)
{
  cancel_exchange_step_t step = { shared_from_this (), id, mark, type };
  asio::post (strand_, std::move (step));
}

// Posted through the strand: asio::post then runs the handler on its own
// associated executor, never inside the call that started the exchange. The
// mark is settled first, so a signal emitted while the handler waits on its
// executor ends nothing.
void
dialogue_impl::complete (exchange_handler_t handler, std::error_code error,
                         link_frame_t frame)
{
  marks_.settle (handler);
  asio::post (strand_,
              asio::append (std::move (handler), error, std::move (frame)));
}

void
dialogue_impl::finish (record_ptr record, std::error_code error,
                       link_frame_t frame)
{
  record->timer.cancel ();
  complete (std::move (record->handler), error, std::move (frame));
}

void
dialogue_impl::finish_in_flight (std::size_t at, std::error_code error,
                                 link_frame_t frame)
{
  record_ptr record (std::move (flight_[at]));
  flight_.erase (flight_.begin () + static_cast<std::ptrdiff_t> (at));
  if (!error)
    note_success (*record);
  finish (std::move (record), error, std::move (frame));
}

std::size_t
dialogue_impl::find_in_flight (std::uint64_t id) const
{
  for (std::size_t i = 0; i < flight_.size (); ++i)
    if (flight_[i]->id == id)
      return i;
  return flight_.size ();
}

void
dialogue_impl::run_start ()
{
  timing::time_point_t const now = timers_.now ();
  last_frame_at_ = now;
  if (watchdog_ms_ > 0)
    arm_watchdog (::lumex::applied::transport::detail::saturating_add (
        now, std::chrono::milliseconds (watchdog_ms_)));
  arm_keepalive ();
  start_reading ();
}

void
dialogue_impl::start_reading ()
{
  if (reading_ || closed_)
    return;
  reading_ = true;
  read_done_t handler = { std::weak_ptr<dialogue_impl> (shared_from_this ()),
                          strand_, read_generation_ };
  try
    {
      link_.impl ()->start_read (link_read_handler_t (std::move (handler)));
    }
  catch (...)
    {
      reading_ = false;
      throw;
    }
}

void
dialogue_impl::run_enqueue (std::uint64_t id, exchange_t &request,
                            exchange_handler_t &handler)
{
  if (closed_)
    {
      complete (std::move (handler), make_error_code (transport_errc::closed),
                link_frame_t ());
      return;
    }
  // The rows of the request, found again in the index of the dialogue.
  pindex::row_t const *row = nullptr;
  pindex::row_t const *reply_row = nullptr;
  bool known = true;
  if (request.row != nullptr)
    {
      row = index_->find_name (request.row->entry->identifier);
      known = known && row != nullptr;
    }
  if (request.reply_row != nullptr)
    {
      reply_row = index_->find_name (request.reply_row->entry->identifier);
      known = known && reply_row != nullptr;
    }
  if (!known)
    {
      complete (std::move (handler),
                dialogue_error (protocol_errc::not_supported),
                link_frame_t ());
      return;
    }
  bool const complete_request
      = (request.how != dialogue::correlation::by_key || row != nullptr)
        && (request.how != dialogue::correlation::by_reply_key
            || reply_row != nullptr)
        && (request.how != dialogue::correlation::custom
            || request.matcher != nullptr)
        && (request.how == dialogue::correlation::by_key
            || request.how == dialogue::correlation::by_order
            || request.how == dialogue::correlation::by_reply_key
            || request.how == dialogue::correlation::custom);
  if (!complete_request)
    {
      complete (std::move (handler),
                make_error_code (std::errc::invalid_argument),
                link_frame_t ());
      return;
    }
  record_ptr record (new exchange_record_t ());
  record->id = id;
  record->message
      = std::make_shared<link_frame_t const> (std::move (request.message));
  record->row = row;
  record->reply_row = reply_row;
  record->how = request.how;
  record->matcher = request.matcher;
  record->policy = request.policy ? request.policy : defaults_;
  record->handler = std::move (handler);
  record->expects_reply
      = !(row != nullptr && row->reply == pindex::reply_form::none
          && reply_row == nullptr && request.matcher == nullptr);
  record->allowed = 1 + static_cast<std::uint64_t> (record->policy->retries);
  record->reader = request.reader;
  if (queue_.empty () && can_start (*record)
      && rate_due (*record) <= timers_.now ())
    {
      std::int64_t state = 0;
      if (refused_by_state (*record, state))
        {
          refuse_by_state (std::move (record), state);
          return;
        }
      flight_.push_back (std::move (record));
      start_attempt (*flight_.back ());
      return;
    }
  if (queue_.size () >= queue_bound_)
    {
      dialogue_counters_t::bump (counters_.queue_refusals);
      LUMEX_TRANSPORT_LOG (
          warning, kcomponent, name_.c_str (),
          "the queue is full (" + std::to_string (queue_bound_)
              + " requests): refused " + row_name (record->row),
          log::log_bytes_t ());
      finish (std::move (record), dialogue_error (protocol_errc::queue_full),
              link_frame_t ());
      return;
    }
  // Behind the queue, or in front of a flight that is full: the end of an
  // exchange in flight starts it (pump); it may also go past a request that
  // waits for its rate limit.
  queue_.push_back (std::move (record));
  counters_.note_queued (queue_.size ());
  pump ();
}

bool
dialogue_impl::can_start (exchange_record_t const &record) const
{
  if (closed_ || window_open_ || flight_.size () >= max_in_flight_)
    return false;
  // A by_order reply carries no key: its exchange is alone in flight.
  if (record.how == dialogue::correlation::by_order && !flight_.empty ())
    return false;
  for (std::size_t i = 0; i < flight_.size (); ++i)
    if (flight_[i]->how == dialogue::correlation::by_order)
      return false;
  return true;
}

// In the order of the queue. A request that waits for its rate limit stays
// where it is and the ones behind it may go past (its row stays in order: the
// next of the row has the same wait); a request that may go but cannot start
// holds the ones behind it, so a by_order exchange always gets its empty
// flight.
void
dialogue_impl::pump ()
{
  timing::time_point_t const now = timers_.now ();
  bool limited = false;
  timing::time_point_t earliest = now;
  std::size_t at = 0;
  while (at < queue_.size () && can_start (*queue_[at]))
    {
      timing::time_point_t const due = rate_due (*queue_[at]);
      if (due > now)
        {
          note_rate_limited (*queue_[at]);
          if (!limited || due < earliest)
            earliest = due;
          limited = true;
          ++at;
          continue;
        }
      record_ptr record (std::move (queue_[at]));
      queue_.erase (queue_.begin () + static_cast<std::ptrdiff_t> (at));
      std::int64_t state = 0;
      if (refused_by_state (*record, state))
        {
          refuse_by_state (std::move (record), state);
          continue;
        }
      flight_.push_back (std::move (record));
      start_attempt (*flight_.back ());
    }
  for (std::size_t i = 0; i < flight_.size (); ++i)
    if (flight_[i]->waiting_rate)
      {
        timing::time_point_t const due = rate_due (*flight_[i]);
        if (!limited || due < earliest)
          earliest = due;
        limited = true;
      }
  if (limited)
    arm_rate_timer (earliest);
}

bool
dialogue_impl::start_attempt (exchange_record_t &record)
{
  ++record.attempts;
  record.epoch = ++attempt_sequence_;
  record.waiting_window = false;
  record.waiting_rate = false;
  record.rate_counted = false;
  record.acknowledged = false;
  record.awaiting = record.expects_reply;
  note_sent (record);
  dialogue_counters_t::bump (counters_.requests);
  if (record.attempts > 1)
    dialogue_counters_t::bump (counters_.retries);
  try
    {
      start_reading ();
      LUMEX_TRANSPORT_LOG (
          debug, kcomponent, name_.c_str (),
          describe (*record.message, pindex::frame_direction::tx),
          log::log_bytes_t (record.message->data (), record.message->size ()));
      if (record.row != nullptr
          && (record.row->entry->options.flags
              & ::lumex::core::protocol::dsl::row_flags::dangerous)
                 != 0)
        LUMEX_TRANSPORT_LOG (
            info, kcomponent, name_.c_str (),
            "a dangerous request: "
                + describe (*record.message, pindex::frame_direction::tx),
            log::log_bytes_t ());
      write_done_t written
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
              record.id, record.epoch };
      link_.impl ()->start_write (record.message,
                                  status_handler_t (std::move (written)));
      arm_timer (record);
    }
  catch (...)
    {
      // Out of memory (or a link of one's own that throws): the exchange
      // ends here, so that it never waits without a write or a timer. The
      // caller starts the next one.
      std::size_t const at = find_in_flight (record.id);
      if (at != flight_.size ())
        finish_in_flight (at, make_error_code (std::errc::not_enough_memory),
                          link_frame_t ());
      return false;
    }
  return true;
}

// Each wait has a number of its own: a wait that ended by its time just as
// the attempt moved on (a NAK, an acknowledgement, a retry) is a wait of
// the past, even when its completion was already on its way.
void
dialogue_impl::arm_timer (exchange_record_t &record)
{
  std::uint32_t const timeout = record.policy->timeout_ms;
  if (!record.expects_reply || timeout == 0)
    return;
  if (!record.timer.impl ())
    record.timer = timers_.make_timer (executor_type (strand_));
  record.timer_epoch = ++timer_sequence_;
  timeout_done_t expired
      = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
          record.id, record.timer_epoch };
  record.timer.async_wait_for (std::chrono::milliseconds (timeout),
                               std::move (expired));
}

std::string
dialogue_impl::describe (link_frame_t const &frame,
                         pindex::frame_direction direction) const
{
  return index_->describe (::lumex::core::protocol::layout::bytes_view_t (
                               frame.data (), frame.size ()),
                           direction, &session_);
}

void
dialogue_impl::write_done (std::uint64_t id, std::uint64_t epoch,
                           std::error_code error)
{
  std::size_t const at = find_in_flight (id);
  if (at == flight_.size () || flight_[at]->epoch != epoch)
    return;
  exchange_record_t const &record = *flight_[at];
  if (error)
    {
      LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                "the request " + row_name (record.row)
                                    + " was not written: " + error.message (),
                                log::log_bytes_t ());
      finish_in_flight (at, error, link_frame_t ());
      pump ();
      return;
    }
  if (!record.expects_reply)
    {
      finish_in_flight (at, std::error_code (), link_frame_t ());
      pump ();
    }
}

void
dialogue_impl::timeout_done (std::uint64_t id, std::uint64_t epoch,
                             std::error_code error)
{
  if (error)
    return;
  std::size_t const at = find_in_flight (id);
  if (at == flight_.size () || flight_[at]->timer_epoch != epoch
      || !flight_[at]->awaiting)
    return;
  attempt_timed_out (at);
}

void
dialogue_impl::attempt_timed_out (std::size_t at)
{
  exchange_record_t &record = *flight_[at];
  dialogue_counters_t::bump (counters_.timeouts);
  record.awaiting = false;
  note_abandoned (record);
  LUMEX_DIALOGUE_LOG_QUIET (
      info, kcomponent, name_.c_str (),
      "no reply to " + row_name (record.row) + " within "
          + std::to_string (record.policy->timeout_ms) + " ms (attempt "
          + std::to_string (record.attempts)
          + (record.acknowledged ? ", acknowledged)" : ")"),
      log::log_bytes_t ());
  if (record.acknowledged)
    {
      // The device has the request: it is never sent again.
      finish_in_flight (at, dialogue_error (protocol_errc::timeout),
                        link_frame_t ());
      pump ();
      return;
    }
  next_attempt (at, protocol_errc::timeout);
}

// The attempt of flight_[at] ended without its reply (`why`: a timeout or a
// NAK): another attempt while the policy allows, else the end of the
// exchange (`timeout` after one attempt, `retries_exhausted` after more,
// `nak` after a NAK).
void
dialogue_impl::next_attempt (std::size_t at, protocol_errc why)
{
  exchange_record_t &record = *flight_[at];
  std::exception_ptr thrown;
  if (record.attempts >= record.allowed && record.policy->extra_retries)
    {
      try
        {
          record.allowed
              += record.policy->extra_retries (session_, dialogue_error (why));
        }
      catch (...)
        {
          thrown = std::current_exception ();
        }
    }
  if (record.attempts >= record.allowed || thrown)
    {
      // A predicate that throws ends the exchange as one that granted no
      // more.
      bool const several = record.allowed > 1 || thrown;
      protocol_errc const ended = why == protocol_errc::nak
                                      ? protocol_errc::nak
                                  : several ? protocol_errc::retries_exhausted
                                            : protocol_errc::timeout;
      if (ended == protocol_errc::retries_exhausted)
        dialogue_counters_t::bump (counters_.retries_exhausted);
      LUMEX_DIALOGUE_LOG_QUIET (
          warning, kcomponent, name_.c_str (),
          (why == protocol_errc::nak ? "a NAK to " : "no reply to ")
              + row_name (record.row) + " after "
              + std::to_string (record.attempts) + " attempt(s)",
          log::log_bytes_t ());
      finish_in_flight (at, dialogue_error (ended), link_frame_t ());
      pump ();
      if (thrown)
        std::rethrow_exception (thrown);
      return;
    }
  if (window_open_)
    record.waiting_window = true;
  else if (!begin_attempt (record))
    pump ();
}

bool
dialogue_impl::begin_attempt (exchange_record_t &record)
{
  timing::time_point_t const due = rate_due (record);
  if (due <= timers_.now ())
    return start_attempt (record);
  record.waiting_rate = true;
  note_rate_limited (record);
  arm_rate_timer (due);
  return true;
}

// An attempt that saw no reply (a timeout, a cancellation) may still be
// answered: a by_key or by_reply_key reply is known by its row, a by_order
// one only by its time (the window).
void
dialogue_impl::note_abandoned (exchange_record_t const &record)
{
  // Nothing here may throw: it runs while an exchange ends, and a late note
  // that is lost only makes a late reply count as an unexpected one.
  try
    {
      pindex::row_t const *expected = nullptr;
      switch (record.how)
        {
        case dialogue::correlation::by_key:
          expected = record.row;
          break;
        case dialogue::correlation::by_reply_key:
          expected = record.reply_row;
          break;
        case dialogue::correlation::by_order:
          if (late_window_ms_ > 0)
            {
              // The window opens only when its timer runs: it never stays
              // open without one.
              window_done_t ended
                  = { std::weak_ptr<dialogue_impl> (shared_from_this ()),
                      strand_, ++window_sequence_ };
              window_open_ = false;
              window_timer_.async_wait_for (
                  std::chrono::milliseconds (late_window_ms_),
                  std::move (ended));
              window_open_ = true;
            }
          return;
        default:
          return;
        }
      if (expected == nullptr)
        return;
      if (late_rows_.size () >= klate_rows)
        late_rows_.pop_front ();
      late_rows_.push_back (expected);
    }
  catch (...)
    {
    }
}

void
dialogue_impl::window_done (std::uint64_t window, std::error_code error)
{
  if (error || window != window_sequence_ || closed_)
    return;
  window_open_ = false;
  // By number: an attempt that fails at once leaves the flight.
  std::vector<std::uint64_t> waiting;
  for (std::size_t i = 0; i < flight_.size (); ++i)
    if (flight_[i]->waiting_window)
      waiting.push_back (flight_[i]->id);
  for (std::size_t i = 0; i < waiting.size (); ++i)
    {
      std::size_t const at = find_in_flight (waiting[i]);
      if (at != flight_.size ())
        begin_attempt (*flight_[at]);
    }
  pump ();
}

bool
dialogue_impl::exact (exchange_record_t const &record) LUMEX_NOEXCEPT
{
  return record.how == dialogue::correlation::by_key
         || record.how == dialogue::correlation::by_reply_key;
}

bool
dialogue_impl::late_holds (pindex::row_t const *row) const LUMEX_NOEXCEPT
{
  for (std::deque<pindex::row_t const *>::const_iterator it
       = late_rows_.begin ();
       it != late_rows_.end (); ++it)
    if (*it == row)
      return true;
  return false;
}

bool
dialogue_impl::take_late (pindex::row_t const *row)
{
  for (std::deque<pindex::row_t const *>::iterator it = late_rows_.begin ();
       it != late_rows_.end (); ++it)
    if (*it == row)
      {
        late_rows_.erase (it);
        return true;
      }
  return false;
}

reply_reading_t
dialogue_impl::read_reply (exchange_record_t const &record,
                           link_frame_t const &frame) const
{
  reply_reading_t reading;
  try
    {
      ::lumex::core::protocol::field::decode_context_t const context
          = session_.decode_context ();
      reading = record.reader (
          ::lumex::core::protocol::layout::bytes_view_t (frame.data (),
                                                         frame.size ()),
          &context,
          ::lumex::core::protocol::layout::bytes_view_t (
              record.message->data (), record.message->size ()));
    }
  catch (...)
    {
      reading = reply_reading_t ();
      reading.effect = reply_effect::fail;
      reading.error = make_error_code (std::errc::not_enough_memory);
    }
  return reading;
}

bool
dialogue_impl::accepts (exchange_record_t const &record,
                        pindex::row_t const *row, link_frame_t const &frame,
                        std::exception_ptr &thrown, reply_reading_t &reading,
                        bool &read) const
{
  switch (record.how)
    {
    case dialogue::correlation::by_key:
      return row != nullptr && row == record.row;
    case dialogue::correlation::by_reply_key:
      return row != nullptr && row == record.reply_row;
    case dialogue::correlation::by_order:
      // A late reply of a keyed row is no reply of a by_order row: the late
      // note takes it.
      if (row != nullptr && (row->is_event || late_holds (row)))
        return false;
      if (record.reader == nullptr)
        return true;
      // The reader knows its reply kind: a frame that is no part of it is
      // left to whoever else may want it, and the exchange goes on waiting.
      reading = read_reply (record, frame);
      read = true;
      return reading.is_part;
    case dialogue::correlation::custom:
      try
        {
          return record.matcher (
              row,
              ::lumex::core::protocol::layout::bytes_view_t (frame.data (),
                                                             frame.size ()),
              ::lumex::core::protocol::layout::bytes_view_t (
                  record.message->data (), record.message->size ()));
        }
      catch (...)
        {
          if (!thrown)
            thrown = std::current_exception ();
          return false;
        }
    default:
      return false;
    }
}

// A frame of no row: a warning at most once a second of the timer source
// (a device that sends noise for ever hides the rest of the log otherwise),
// carrying the number of the frames since the last warning that went to
// debug.
void
dialogue_impl::log_unknown (link_frame_t const &frame)
{
  timing::time_point_t const now = timers_.now ();
  bool const warn = !unknown_warned_ || now < unknown_warned_at_
                    || now - unknown_warned_at_ >= std::chrono::seconds (1);
  if (!warn)
    {
      ++unknown_skipped_;
      LUMEX_DIALOGUE_LOG_QUIET (
          debug, kcomponent, name_.c_str (),
          "a frame of no row: "
              + describe (frame, pindex::frame_direction::rx),
          log::log_bytes_t ());
      return;
    }
  std::uint64_t const skipped = unknown_skipped_;
  unknown_warned_ = true;
  unknown_warned_at_ = now;
  unknown_skipped_ = 0;
  LUMEX_DIALOGUE_LOG_QUIET (
      warning, kcomponent, name_.c_str (),
      "a frame of no row: " + describe (frame, pindex::frame_direction::rx)
          + (skipped == 0 ? std::string ()
                          : " (" + std::to_string (skipped)
                                + " more since the last warning, logged at "
                                  "debug)"),
      log::log_bytes_t ());
}

void
dialogue_impl::handle_frame (link_frame_t &frame)
{
  pindex::frame_match_t const found
      = index_->match (::lumex::core::protocol::layout::bytes_view_t (
          frame.data (), frame.size ()));
  pindex::row_t const *const row = found.row;
  // What the frame says of the stream and the levels of its row comes first:
  // a queued request is checked against the state the frame brought.
  std::vector<edge_t> edges;
  bool state_changed = false;
  std::int64_t state = 0;
  row_watch_t *const watch = watch_of (row);
  if (watch != nullptr)
    {
      note_stream_frame (*watch);
      read_levels (*watch, frame, edges, state_changed, state);
    }
  std::exception_ptr thrown;
  std::vector<handler_ptr> handlers;
  std::vector<std::uint64_t> ready;
  bool answered = false;
  // An exact match (by_key, by_reply_key) first, then the exchanges that
  // take a frame by their own rule (custom, by_order): a reply never goes to
  // an earlier exchange that would accept anything.
  for (int pass = 0; pass < 2 && !answered; ++pass)
    for (std::size_t i = 0; i < flight_.size () && !answered; ++i)
      {
        reply_reading_t reading;
        bool read = false;
        if (flight_[i]->awaiting && exact (*flight_[i]) == (pass == 0)
            && accepts (*flight_[i], row, frame, thrown, reading, read))
          {
            dialogue_counters_t::bump (counters_.replies);
            apply_reply (i, frame, read ? &reading : nullptr);
            answered = true;
          }
      }
  if (!answered)
    {
      bool const taken = offer_to_runs (row, frame, ready);
      if (taken && !row->is_event)
        dialogue_counters_t::bump (counters_.replies);
      else if (window_open_ && (row == nullptr || !row->is_event))
        {
          dialogue_counters_t::bump (counters_.late_replies);
          LUMEX_TRANSPORT_LOG (debug, kcomponent, name_.c_str (),
                               "dropped a late reply", log::log_bytes_t ());
        }
      else if (row != nullptr && take_late (row))
        {
          dialogue_counters_t::bump (counters_.late_replies);
          LUMEX_TRANSPORT_LOG (debug, kcomponent, name_.c_str (),
                               "dropped a late reply", log::log_bytes_t ());
        }
      else if (row == nullptr)
        {
          dialogue_counters_t::bump (counters_.unknown_frames);
          log_unknown (frame);
          handlers = unknown_handlers ();
        }
      else
        {
          dialogue_counters_t::bump (
              row->is_event ? counters_.events : counters_.unexpected_replies);
          handlers = subscribers_of (row);
        }
    }
  // The runs go on with the frame counted; a predicate of theirs may throw.
  try
    {
      if (state_changed)
        wake_state_waiters (state);
      for (std::size_t i = 0; i < ready.size (); ++i)
        sequence_advance (ready[i]);
    }
  catch (...)
    {
      if (!thrown)
        thrown = std::current_exception ();
    }
  // The handlers run last, with the frame counted: one that throws or
  // destroys the dialogue leaves the engine as it is; the others run too.
  for (std::size_t i = 0; i < edges.size (); ++i)
    {
      std::vector<std::shared_ptr<edge_handler_t const>> const hearing
          = edge_handlers_of (edges[i].row);
      for (std::size_t j = 0; j < hearing.size (); ++j)
        try
          {
            (*hearing[j]) (edges[i]);
          }
        catch (...)
          {
            if (!thrown)
              thrown = std::current_exception ();
          }
    }
  for (std::size_t i = 0; i < handlers.size (); ++i)
    try
      {
        (*handlers[i]) (frame);
      }
    catch (...)
      {
        if (!thrown)
          thrown = std::current_exception ();
      }
  if (thrown)
    std::rethrow_exception (thrown);
}

// A frame that answers flight_[at], by the reader of its reply kind.
void
dialogue_impl::apply_reply (std::size_t at, link_frame_t &frame,
                            reply_reading_t const *known)
{
  exchange_record_t &record = *flight_[at];
  if (record.reader == nullptr)
    {
      finish_in_flight (at, std::error_code (), std::move (frame));
      pump ();
      return;
    }
  reply_reading_t const reading
      = known != nullptr ? *known : read_reply (record, frame);
  switch (reading.effect)
    {
    case reply_effect::complete:
      finish_in_flight (at, std::error_code (), std::move (frame));
      pump ();
      return;
    case reply_effect::more:
      // The acknowledgement of a two-phase row: the completion comes later,
      // within a timeout of its own.
      // Only the first acknowledgement arms the timer: a repeated one
      // leaves it running, so that the exchange always ends.
      if (!record.acknowledged)
        {
          record.acknowledged = true;
          arm_timer (record);
        }
      return;
    case reply_effect::nak:
      dialogue_counters_t::bump (counters_.naks);
      record.awaiting = false;
      record.timer.cancel ();
      LUMEX_DIALOGUE_LOG_QUIET (info, kcomponent, name_.c_str (),
                                "a NAK to " + row_name (record.row)
                                    + " (attempt "
                                    + std::to_string (record.attempts) + ")",
                                log::log_bytes_t ());
      next_attempt (at, protocol_errc::nak);
      return;
    default:
      LUMEX_DIALOGUE_LOG_QUIET (
          warning, kcomponent, name_.c_str (),
          row_name (record.row) + " failed: " + reading.error.message ()
              + "; sent: "
              + describe (*record.message, pindex::frame_direction::tx)
              + "; got: " + describe (frame, pindex::frame_direction::rx),
          log::log_bytes_t ());
      finish_in_flight (at, reading.error, std::move (frame));
      pump ();
      return;
    }
}

std::vector<dialogue_impl::handler_ptr>
dialogue_impl::subscribers_of (pindex::row_t const *row) const
{
  std::vector<handler_ptr> found;
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  for (std::size_t i = 0; i < subscribers_.size (); ++i)
    if (subscribers_[i].row == row)
      found.push_back (subscribers_[i].handler);
  return found;
}

std::vector<dialogue_impl::handler_ptr>
dialogue_impl::unknown_handlers () const
{
  std::vector<handler_ptr> found;
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  if (unknown_)
    found.push_back (unknown_);
  return found;
}

subscription_t
dialogue_impl::subscribe (pindex::row_t const *row, frame_handler_t handler)
{
  subscription_t made;
  pindex::row_t const *const own
      = row != nullptr ? index_->find_name (row->entry->identifier) : nullptr;
  if (own == nullptr || !handler)
    return made;
  subscriber_t added;
  added.id = next_subscription_.fetch_add (1, std::memory_order_relaxed) + 1;
  added.row = own;
  added.handler
      = std::make_shared<frame_handler_t const> (std::move (handler));
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  // A closed dialogue keeps no handler (one that owns the owner of the
  // dialogue would be a cycle); `added` dies after the lock is released.
  if (handlers_closed_)
    return made;
  subscribers_.push_back (added);
  made.id = added.id;
  return made;
}

// The handler dies after the lock is released: its destructor may call the
// engine.
bool
dialogue_impl::unsubscribe (subscription_t subscription)
{
  handler_ptr removed;
  std::shared_ptr<edge_handler_t const> removed_edge;
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  for (std::vector<subscriber_t>::iterator it = subscribers_.begin ();
       it != subscribers_.end (); ++it)
    if (it->id == subscription.id)
      {
        removed.swap (it->handler);
        subscribers_.erase (it);
        return true;
      }
  for (std::vector<edge_subscriber_t>::iterator it
       = edge_subscribers_.begin ();
       it != edge_subscribers_.end (); ++it)
    if (it->id == subscription.id)
      {
        removed_edge.swap (it->handler);
        edge_subscribers_.erase (it);
        return true;
      }
  return false;
}

void
dialogue_impl::set_unknown_handler (frame_handler_t handler)
{
  // The old one dies after the lock is released: its destructor may call the
  // engine.
  handler_ptr made
      = handler ? std::make_shared<frame_handler_t const> (std::move (handler))
                : handler_ptr ();
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  if (handlers_closed_)
    return;
  unknown_.swap (made);
}

void
dialogue_impl::read_done (std::uint64_t generation, std::error_code error,
                          link_frame_t frame)
{
  if (generation != read_generation_ || closed_)
    return;
  reading_ = false;
  if (!error)
    {
      std::exception_ptr thrown;
      frame_seen (thrown);
      // The next read first: a frame whose handling throws must not stop
      // the reading.
      try
        {
          start_reading ();
        }
      catch (...)
        {
          schedule_read_restart ();
          throw;
        }
      try
        {
          LUMEX_TRANSPORT_LOG (
              debug, kcomponent, name_.c_str (),
              describe (frame, pindex::frame_direction::rx),
              log::log_bytes_t (frame.data (), frame.size ()));
          handle_frame (frame);
        }
      catch (...)
        {
          if (!thrown)
            thrown = std::current_exception ();
        }
      if (thrown)
        std::rethrow_exception (thrown);
      return;
    }
  if (error::is_transport_error (error, transport_errc::cancelled)
      && link_.is_open ())
    {
      // The first time at once (someone cancelled the link); a link whose
      // reads keep ending so must not spin.
      if (!read_restarted_at_once_)
        {
          read_restarted_at_once_ = true;
          start_reading ();
        }
      else
        schedule_read_restart ();
      return;
    }
  schedule_read_restart ();
  link_lost (error);
}

void
dialogue_impl::link_lost (std::error_code error)
{
  bool const was_up = link_up_.exchange (false, std::memory_order_acq_rel);
  // Without an exchange in flight nothing was lost yet (a link that is not
  // open when the dialogue starts reads again by itself and with the next
  // request).
  if (!flight_.empty ())
    LUMEX_TRANSPORT_LOG (warning, kcomponent, name_.c_str (),
                         "the link was lost: " + error.message (),
                         log::log_bytes_t ());
  else if (was_up)
    LUMEX_TRANSPORT_LOG (info, kcomponent, name_.c_str (),
                         "reading stopped: " + error.message (),
                         log::log_bytes_t ());
  forget_watches ();
  std::error_code const lost = dialogue_error (protocol_errc::link_lost);
  std::vector<record_ptr> ended;
  ended.swap (flight_);
  for (std::size_t i = 0; i < ended.size (); ++i)
    finish (std::move (ended[i]), lost, link_frame_t ());
  pump ();
  // Last: the handler may throw or destroy the dialogue.
  if (was_up)
    notify_link_state (link_state::lost, error);
}

void
dialogue_impl::end_all (std::error_code error, bool abandoned)
{
  std::deque<record_ptr> queued;
  queued.swap (queue_);
  std::vector<record_ptr> flying;
  flying.swap (flight_);
  for (std::size_t i = 0; i < flying.size (); ++i)
    {
      if (abandoned && flying[i]->awaiting)
        note_abandoned (*flying[i]);
      finish (std::move (flying[i]), error, link_frame_t ());
    }
  for (std::size_t i = 0; i < queued.size (); ++i)
    finish (std::move (queued[i]), error, link_frame_t ());
}

void
dialogue_impl::run_cancel ()
{
  std::error_code const cancelled
      = make_error_code (transport_errc::cancelled);
  end_all (cancelled, true);
  end_all_runs (cancelled);
}

void
dialogue_impl::run_close ()
{
  if (closed_)
    return;
  closed_ = true;
  ++read_generation_;
  window_open_ = false;
  window_timer_.cancel ();
  cancel_feature_timers ();
  std::error_code const cancelled
      = make_error_code (transport_errc::cancelled);
  end_all (cancelled, false);
  end_all_runs (cancelled);
  link_.close ();
  // A closed dialogue calls no handler any more; they die here, outside the
  // lock (one may own the owner of the dialogue).
  std::vector<subscriber_t> subscribers;
  handler_ptr unknown;
  feature_handlers_t kept;
  {
    std::lock_guard<std::mutex> const lock (handlers_mutex_);
    handlers_closed_ = true;
    subscribers.swap (subscribers_);
    unknown.swap (unknown_);
    take_feature_handlers (kept);
  }
}

void
dialogue_impl::run_cancel_exchange (std::uint64_t id,
                                    asio::cancellation_type_t type)
{
  std::error_code const cancelled
      = make_error_code (transport_errc::cancelled);
  for (std::deque<record_ptr>::iterator it = queue_.begin ();
       it != queue_.end (); ++it)
    if ((*it)->id == id)
      {
        record_ptr record (std::move (*it));
        queue_.erase (it);
        finish (std::move (record), cancelled, link_frame_t ());
        // The head of the queue may have held the others back (a by_order
        // exchange waits for an empty flight).
        pump ();
        return;
      }
  std::size_t const at = find_in_flight (id);
  if (at == flight_.size ())
    return;
  // Once written, only a cancellation that allows side effects ends it.
  if ((type
       & (asio::cancellation_type::terminal
          | asio::cancellation_type::partial))
      == asio::cancellation_type::none)
    return;
  if (flight_[at]->awaiting)
    note_abandoned (*flight_[at]);
  finish_in_flight (at, cancelled, link_frame_t ());
  pump ();
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
