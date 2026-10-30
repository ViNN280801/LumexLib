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
#include <deque>
#include <exception>
#include <memory>
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
                                              &queue_refusals };
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
      next_id_ (0), enlisted_ (false)
{
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
  try
    {
      taken.reserve (queue_.size () + flight_.size ());
    }
  catch (...)
    {
    }
  for (std::deque<record_ptr>::iterator it = queue_.begin ();
       it != queue_.end (); ++it)
    {
      exchange_handler_t handler (std::move ((*it)->handler));
      try
        {
          taken.push_back (std::move (handler));
        }
      catch (...)
        {
        }
    }
  for (std::vector<record_ptr>::iterator it = flight_.begin ();
       it != flight_.end (); ++it)
    {
      exchange_handler_t handler (std::move ((*it)->handler));
      try
        {
          taken.push_back (std::move (handler));
        }
      catch (...)
        {
        }
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
  if (queue_.empty () && can_start (*record))
    {
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
  // exchange in flight starts it (pump).
  queue_.push_back (std::move (record));
  counters_.note_queued (queue_.size ());
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

void
dialogue_impl::pump ()
{
  while (!queue_.empty () && can_start (*queue_.front ()))
    {
      record_ptr record (std::move (queue_.front ()));
      queue_.pop_front ();
      flight_.push_back (std::move (record));
      start_attempt (*flight_.back ());
    }
}

void
dialogue_impl::start_attempt (exchange_record_t &record)
{
  ++record.attempts;
  record.epoch = ++attempt_sequence_;
  record.waiting_window = false;
  record.awaiting = record.expects_reply;
  dialogue_counters_t::bump (counters_.requests);
  if (record.attempts > 1)
    dialogue_counters_t::bump (counters_.retries);
  start_reading ();
  std::weak_ptr<dialogue_impl> const self (shared_from_this ());
  write_done_t written = { self, strand_, record.id, record.epoch };
  link_.impl ()->start_write (record.message,
                              status_handler_t (std::move (written)));
  std::uint32_t const timeout = record.policy->timeout_ms;
  if (!record.expects_reply || timeout == 0)
    return;
  if (!record.timer.impl ())
    record.timer = timers_.make_timer (executor_type (strand_));
  timeout_done_t expired = { self, strand_, record.id, record.epoch };
  record.timer.async_wait_for (std::chrono::milliseconds (timeout),
                               std::move (expired));
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
      LUMEX_TRANSPORT_LOG (warning, kcomponent, name_.c_str (),
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
  if (at == flight_.size () || flight_[at]->epoch != epoch
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
  LUMEX_TRANSPORT_LOG (info, kcomponent, name_.c_str (),
                       "no reply to " + row_name (record.row) + " within "
                           + std::to_string (record.policy->timeout_ms)
                           + " ms (attempt " + std::to_string (record.attempts)
                           + ")",
                       log::log_bytes_t ());
  std::exception_ptr thrown;
  if (record.attempts >= record.allowed && record.policy->extra_retries)
    {
      try
        {
          record.allowed += record.policy->extra_retries (
              session_, dialogue_error (protocol_errc::timeout));
        }
      catch (...)
        {
          thrown = std::current_exception ();
        }
    }
  if (record.attempts >= record.allowed || thrown)
    {
      // A predicate that throws ends the exchange as one that granted no
      // more: retries_exhausted.
      bool const several = record.allowed > 1 || thrown;
      if (several)
        dialogue_counters_t::bump (counters_.retries_exhausted);
      LUMEX_TRANSPORT_LOG (warning, kcomponent, name_.c_str (),
                           "no reply to " + row_name (record.row) + " after "
                               + std::to_string (record.attempts)
                               + " attempt(s)",
                           log::log_bytes_t ());
      finish_in_flight (at,
                        dialogue_error (several
                                            ? protocol_errc::retries_exhausted
                                            : protocol_errc::timeout),
                        link_frame_t ());
      pump ();
      if (thrown)
        std::rethrow_exception (thrown);
      return;
    }
  if (window_open_)
    record.waiting_window = true;
  else
    start_attempt (record);
}

// An attempt that saw no reply (a timeout, a cancellation) may still be
// answered: a by_key or by_reply_key reply is known by its row, a by_order
// one only by its time (the window).
void
dialogue_impl::note_abandoned (exchange_record_t const &record)
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
          window_open_ = true;
          window_done_t ended
              = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
                  ++window_sequence_ };
          window_timer_.async_wait_for (
              std::chrono::milliseconds (late_window_ms_), std::move (ended));
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

void
dialogue_impl::window_done (std::uint64_t window, std::error_code error)
{
  if (error || window != window_sequence_ || closed_)
    return;
  window_open_ = false;
  for (std::size_t i = 0; i < flight_.size (); ++i)
    if (flight_[i]->waiting_window)
      start_attempt (*flight_[i]);
  pump ();
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

bool
dialogue_impl::accepts (exchange_record_t const &record,
                        pindex::row_t const *row, link_frame_t const &frame,
                        std::exception_ptr &thrown) const
{
  switch (record.how)
    {
    case dialogue::correlation::by_key:
      return row != nullptr && row == record.row;
    case dialogue::correlation::by_reply_key:
      return row != nullptr && row == record.reply_row;
    case dialogue::correlation::by_order:
      return row == nullptr || !row->is_event;
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

void
dialogue_impl::handle_frame (link_frame_t &frame)
{
  pindex::frame_match_t const found
      = index_->match (::lumex::core::protocol::layout::bytes_view_t (
          frame.data (), frame.size ()));
  pindex::row_t const *const row = found.row;
  std::exception_ptr thrown;
  for (std::size_t i = 0; i < flight_.size (); ++i)
    if (flight_[i]->awaiting && accepts (*flight_[i], row, frame, thrown))
      {
        dialogue_counters_t::bump (counters_.replies);
        finish_in_flight (i, std::error_code (), std::move (frame));
        pump ();
        if (thrown)
          std::rethrow_exception (thrown);
        return;
      }
  if (window_open_ && (row == nullptr || !row->is_event))
    dialogue_counters_t::bump (counters_.late_replies);
  else if (row != nullptr && take_late (row))
    dialogue_counters_t::bump (counters_.late_replies);
  else if (row == nullptr)
    dialogue_counters_t::bump (counters_.unknown_frames);
  else if (row->is_event)
    dialogue_counters_t::bump (counters_.events);
  else
    dialogue_counters_t::bump (counters_.unexpected_replies);
  if (thrown)
    std::rethrow_exception (thrown);
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
      // The next read first: a frame whose handling throws must not stop
      // the reading.
      start_reading ();
      handle_frame (frame);
      return;
    }
  if (error::is_transport_error (error, transport_errc::cancelled)
      && link_.is_open ())
    {
      start_reading ();
      return;
    }
  link_lost (error);
}

void
dialogue_impl::link_lost (std::error_code error)
{
  // Without an exchange in flight nothing was lost yet (a link that is not
  // open when the dialogue starts reads again with the next request).
  if (flight_.empty ())
    LUMEX_TRANSPORT_LOG (info, kcomponent, name_.c_str (),
                         "reading stopped: " + error.message (),
                         log::log_bytes_t ());
  else
    LUMEX_TRANSPORT_LOG (warning, kcomponent, name_.c_str (),
                         "the link was lost: " + error.message (),
                         log::log_bytes_t ());
  std::error_code const lost = dialogue_error (protocol_errc::link_lost);
  std::vector<record_ptr> ended;
  ended.swap (flight_);
  for (std::size_t i = 0; i < ended.size (); ++i)
    finish (std::move (ended[i]), lost, link_frame_t ());
  pump ();
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
  end_all (make_error_code (transport_errc::cancelled), true);
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
  end_all (make_error_code (transport_errc::cancelled), false);
  late_rows_.clear ();
  link_.close ();
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

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
