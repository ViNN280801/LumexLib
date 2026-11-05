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
#include <deque>
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
#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
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
namespace pindex = ::lumex::core::protocol::index;
namespace pdsl = ::lumex::core::protocol::dsl;
using error::transport_errc;
using ::lumex::core::protocol::error::protocol_errc;

/// The component of the log records.
char const *const kcomponent = "transport.dialogue";

std::error_code
dialogue_error (protocol_errc value)
{
  return ::lumex::core::protocol::error::make_error_code (value);
}

// Holds the engine weakly: the link keeps it.
struct connection_watch_t
{
  std::weak_ptr<dialogue_impl> impl;

  void
  operator() (connection_event event, std::error_code const &reason) const
  {
    std::shared_ptr<dialogue_impl> const alive = impl.lock ();
    if (alive)
      alive->connection_changed (event, reason);
  }
};

struct device_checked_step_t
{
  std::shared_ptr<dialogue_impl> impl;

  void
  operator() ()
  {
    impl->run_device_checked ();
  }
};

struct checked_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::uint64_t restoration;
  std::error_code error;
  bool same_device;

  void
  operator() ()
  {
    impl->reconnect_checked (restoration, error, same_device);
  }
};

// What `reconnect_steps_t::check` gets: posted, so that the check may end
// inside the call that started it.
struct check_done_t
{
  std::weak_ptr<dialogue_impl> impl;
  dialogue_impl::strand_type strand;
  std::uint64_t restoration;

  void
  operator() (std::error_code error, bool same_device) const
  {
    std::shared_ptr<dialogue_impl> const alive = impl.lock ();
    if (!alive)
      return;
    checked_step_t step = { alive, restoration, error, same_device };
    asio::post (strand, std::move (step));
  }
};

struct stream_end_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t restoration;

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
      alive->reconnect_stream_ended (restoration, error);
  }
};

struct wait_done_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t wait;

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
      alive->reconnect_wait_done (wait, error);
  }
};

struct report_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::error_code error;

  void
  operator() ()
  {
    impl->run_reconnect_report (error);
  }
};

// A check that ends so may do better on another connection.
bool
gives_up_connection (std::error_code const &error) LUMEX_NOEXCEPT
{
  return error == dialogue_error (protocol_errc::timeout)
         || error == dialogue_error (protocol_errc::retries_exhausted)
         || error == dialogue_error (protocol_errc::link_lost);
}

// Whether an exchange in flight goes out again on a new connection.
bool
sent_again (exchange_record_t const &record) LUMEX_NOEXCEPT
{
  if (record.internal || record.before_check || record.row == nullptr)
    return false;
  std::uint32_t const flags = record.row->entry->options.flags;
  return (flags & pdsl::row_flags::idempotent) != 0
         && (flags & pdsl::row_flags::dangerous) == 0;
}
} // namespace

void
dialogue_impl::set_reconnect_handler (reconnect_handler_t handler)
{
  // Before the lock: the handler that goes dies after it, and its destructor
  // may call back.
  std::shared_ptr<reconnect_handler_t const> made
      = handler
            ? std::make_shared<reconnect_handler_t const> (std::move (handler))
            : std::shared_ptr<reconnect_handler_t const> ();
  std::lock_guard<std::mutex> const lock (handlers_mutex_);
  if (!handlers_closed_)
    reconnect_handler_.swap (made);
}

void
dialogue_impl::start_device_checked ()
{
  device_checked_step_t step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::run_device_checked ()
{
  if (closed_ || !reconnects_ || connection_down_ || device_checked_
      || restoring_)
    return;
  device_checked_ = true;
  gate_error_ = std::error_code ();
  pump ();
}

void
dialogue_impl::start_watching_connection ()
{
  if (!reconnects_)
    return;
  connection_watch_t watch
      = { std::weak_ptr<dialogue_impl> (shared_from_this ()) };
  link_.impl ()->watch_connection (connection_handler_t (std::move (watch)));
  connection_down_ = !link_.is_open ();
  never_opened_ = connection_down_;
  if (connection_down_)
    link_up_.store (false, std::memory_order_release);
}

void
dialogue_impl::connection_changed (connection_event event,
                                   std::error_code const &reason)
{
  if (closed_)
    return;
  switch (event)
    {
    case connection_event::opened:
      connection_opened ();
      break;
    case connection_event::lost:
      connection_lost (reason);
      break;
    default:
      connection_lost (reason);
      // Nobody is connecting: nothing is worth waiting for.
      fail_waiting (dialogue_error (protocol_errc::link_lost));
      break;
    }
}

void
dialogue_impl::connection_lost (std::error_code const &reason)
{
  if (connection_down_)
    return;
  connection_down_ = true;
  device_checked_ = false;
  gate_error_ = dialogue_error (protocol_errc::link_lost);
  restoring_ = false;
  ++restoration_;
  bool const was_up = link_up_.exchange (false, std::memory_order_acq_rel);
  LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                            "the connection was lost: " + reason.message (),
                            log::log_bytes_t ());
  // What the connection taught is over with it.
  forget_watches ();
  clear_state ();
  late_rows_.clear ();
  window_open_ = false;
  ++window_sequence_;
  window_timer_.cancel ();
  std::error_code const lost = dialogue_error (protocol_errc::link_lost);
  timing::time_point_t const lost_at = timers_.now ();
  // A run stands on what the device did so far.
  end_all_runs (lost);
  // What the engine made and what looks for the device of this connection are
  // over with it, also those still queued.
  std::deque<record_ptr> queued;
  queued.swap (queue_);
  for (std::size_t i = 0; i < queued.size (); ++i)
    {
      if (queued[i]->internal || queued[i]->before_check
          || loss_ends (*queued[i], lost_at))
        finish (std::move (queued[i]), lost, link_frame_t ());
      else
        queue_.push_back (std::move (queued[i]));
    }
  // Back to the front of the queue, in the order they went.
  std::vector<record_ptr> flying;
  flying.swap (flight_);
  for (std::size_t i = flying.size (); i != 0; --i)
    {
      record_ptr record (std::move (flying[i - 1]));
      if (!sent_again (*record) || loss_ends (*record, lost_at))
        {
          finish (std::move (record), lost, link_frame_t ());
          continue;
        }
      record->timer.cancel ();
      ++record->timer_epoch;
      record->attempts = 0;
      record->allowed
          = 1 + static_cast<std::uint64_t> (record->policy->retries);
      record->acknowledged = false;
      record->awaiting = false;
      record->waiting_rate = false;
      record->waiting_window = false;
      record->rate_counted = false;
      queue_.push_front (std::move (record));
    }
  arm_reconnect_wait ();
  if (reconnect_steps_.forget)
    reconnect_steps_.forget ();
  // Last: the handler may throw or destroy the dialogue.
  if (was_up)
    notify_link_state (link_state::lost, reason);
}

void
dialogue_impl::connection_opened ()
{
  if (!connection_down_)
    return;
  connection_down_ = false;
  bool const first = never_opened_;
  never_opened_ = false;
  read_failures_ = 0;
  read_restarted_at_once_ = false;
  timing::time_point_t const now = timers_.now ();
  last_frame_at_ = now;
  if (watchdog_ms_ > 0)
    arm_watchdog (::lumex::applied::transport::detail::saturating_add (
        now, std::chrono::milliseconds (watchdog_ms_)));
  arm_keepalive ();
  start_reading ();
  bool const was_up = link_up_.exchange (true, std::memory_order_acq_rel);
  if (first)
    {
      device_checked_ = true;
      gate_error_ = std::error_code ();
      pump ();
    }
  else
    {
      dialogue_counters_t::bump (counters_.reconnects);
      LUMEX_DIALOGUE_LOG_QUIET (info, kcomponent, name_.c_str (),
                                "the link is open again: restoring",
                                log::log_bytes_t ());
      start_restoration ();
    }
  // Last: the handler may throw or destroy the dialogue.
  if (!was_up)
    notify_link_state (link_state::up, std::error_code ());
}

void
dialogue_impl::start_restoration ()
{
  restoring_ = true;
  check_pending_ = true;
  std::uint64_t const restoration = ++restoration_;
  if (!reconnect_steps_.check)
    {
      reconnect_checked (restoration, std::error_code (), true);
      return;
    }
  check_done_t done = { std::weak_ptr<dialogue_impl> (shared_from_this ()),
                        strand_, restoration };
  try
    {
      reconnect_steps_.check (reconnect_done_t (done));
    }
  catch (...)
    {
      done (make_error_code (std::errc::not_enough_memory), true);
    }
}

void
dialogue_impl::reconnect_checked (std::uint64_t restoration,
                                  std::error_code error, bool same_device)
{
  if (restoration != restoration_ || closed_ || connection_down_ || !restoring_
      || !check_pending_)
    return;
  check_pending_ = false;
  if (error::is_transport_error (error, transport_errc::cancelled))
    {
      // The application cancelled what ran: not the restoration.
      start_restoration ();
      return;
    }
  if (gives_up_connection (error))
    {
      LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                "the device was not checked: "
                                    + error.message (),
                                log::log_bytes_t ());
      report_reconnect (error);
      link_.impl ()->drop_connection ();
      connection_lost (error);
      return;
    }
  if (error)
    {
      LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                "the device is not the one of the table: "
                                    + error.message (),
                                log::log_bytes_t ());
      restoring_ = false;
      gate_error_ = dialogue_error (protocol_errc::not_supported);
      fail_waiting (dialogue_error (protocol_errc::link_lost));
      report_reconnect (error);
      return;
    }
  if (!same_device)
    {
      fail_waiting (dialogue_error (protocol_errc::link_lost));
      enabled_streams_.clear ();
    }
  stream_position_ = 0;
  stream_error_ = std::error_code ();
  restore_next_stream ();
}

void
dialogue_impl::restore_next_stream ()
{
  while (stream_position_ < enabled_streams_.size ())
    {
      enabled_stream_t const &item = enabled_streams_[stream_position_];
      ++stream_position_;
      pindex::row_t const *const row
          = index_now ().find_name (item.identifier);
      if (row == nullptr)
        continue;
      try
        {
          exchange_t request;
          request.message = *item.message;
          request.before_check = true;
          row_watch_t const *const watch = watch_of (row);
          fill_exchange (request, watch != nullptr ? watch->features : nullptr,
                         row);
          stream_end_t ended
              = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_,
                  restoration_ };
          exchange_handler_t handler (std::move (ended));
          std::uint64_t const id
              = next_id_.fetch_add (1, std::memory_order_relaxed) + 1;
          run_enqueue (id, request, handler, true);
          return;
        }
      catch (...)
        {
          stream_error_ = make_error_code (std::errc::not_enough_memory);
        }
    }
  end_restoration (stream_error_);
}

void
dialogue_impl::reconnect_stream_ended (std::uint64_t restoration,
                                       std::error_code error)
{
  if (restoration != restoration_ || closed_ || connection_down_
      || !restoring_)
    return;
  if (error::is_transport_error (error, transport_errc::cancelled))
    {
      start_restoration ();
      return;
    }
  if (error && !stream_error_)
    {
      stream_error_ = error;
      LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                "a stream did not come back: "
                                    + error.message (),
                                log::log_bytes_t ());
    }
  restore_next_stream ();
}

void
dialogue_impl::end_restoration (std::error_code const &error)
{
  restoring_ = false;
  device_checked_ = true;
  gate_error_ = std::error_code ();
  reconnect_wait_timer_.cancel ();
  reconnect_wait_armed_ = false;
  pump ();
  report_reconnect (error);
}

void
dialogue_impl::fail_waiting (std::error_code const &error)
{
  reconnect_wait_timer_.cancel ();
  reconnect_wait_armed_ = false;
  std::deque<record_ptr> queued;
  queued.swap (queue_);
  for (std::size_t i = 0; i < queued.size (); ++i)
    {
      if (queued[i]->before_check)
        queue_.push_back (std::move (queued[i]));
      else
        finish (std::move (queued[i]), error, link_frame_t ());
    }
}

// Whether the exchange has waited for the dialogue as long as it may, from its
// first loss; the loss that comes now is its first when it had none.
bool
dialogue_impl::loss_ends (exchange_record_t &record, timing::time_point_t now)
{
  if (!record.lost_once)
    {
      record.lost_once = true;
      record.first_lost_at = now;
    }
  return now >= ::lumex::applied::transport::detail::saturating_add (
             record.first_lost_at,
             std::chrono::milliseconds (reconnect_wait_ms_));
}

// One timer for the queue, set for the exchange whose time ends first.
void
dialogue_impl::arm_reconnect_wait ()
{
  bool any = false;
  timing::time_point_t earliest;
  for (std::size_t i = 0; i < queue_.size (); ++i)
    {
      exchange_record_t const &record = *queue_[i];
      if (record.before_check || !record.lost_once)
        continue;
      timing::time_point_t const end
          = ::lumex::applied::transport::detail::saturating_add (
              record.first_lost_at,
              std::chrono::milliseconds (reconnect_wait_ms_));
      if (!any || end < earliest)
        earliest = end;
      any = true;
    }
  if (!any || (reconnect_wait_armed_ && reconnect_wait_deadline_ <= earliest))
    return;
  try
    {
      wait_done_t done = { std::weak_ptr<dialogue_impl> (shared_from_this ()),
                           strand_, ++reconnect_wait_ };
      reconnect_wait_timer_.async_wait_until (earliest, std::move (done));
      reconnect_wait_armed_ = true;
      reconnect_wait_deadline_ = earliest;
    }
  catch (...)
    {
      // Out of memory: nothing may wait without a time.
      fail_waiting (dialogue_error (protocol_errc::link_lost));
    }
}

void
dialogue_impl::reconnect_wait_done (std::uint64_t wait, std::error_code error)
{
  if (error || wait != reconnect_wait_ || !reconnect_wait_armed_ || closed_)
    return;
  reconnect_wait_armed_ = false;
  LUMEX_DIALOGUE_LOG_QUIET (info, kcomponent, name_.c_str (),
                            "stopped waiting for the link",
                            log::log_bytes_t ());
  if (!gate_open ())
    expire_waiting ();
}

// Ends the queued exchanges whose time is up, and waits on for the others.
void
dialogue_impl::expire_waiting ()
{
  timing::time_point_t const now = timers_.now ();
  std::deque<record_ptr> queued;
  queued.swap (queue_);
  for (std::size_t i = 0; i < queued.size (); ++i)
    {
      if (!queued[i]->before_check && loss_ends (*queued[i], now))
        finish (std::move (queued[i]),
                dialogue_error (protocol_errc::link_lost), link_frame_t ());
      else
        queue_.push_back (std::move (queued[i]));
    }
  arm_reconnect_wait ();
}

// Posted: never inside what ended the restoration, with the state final.
void
dialogue_impl::report_reconnect (std::error_code const &error)
{
  report_step_t step = { shared_from_this (), error };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::run_reconnect_report (std::error_code const &error)
{
  std::shared_ptr<reconnect_handler_t const> handler;
  {
    std::lock_guard<std::mutex> const lock (handlers_mutex_);
    handler = reconnect_handler_;
  }
  if (handler)
    (*handler) (error);
}

void
dialogue_impl::note_enabled_stream (exchange_record_t const &record)
{
  if (!reconnects_ || record.before_check || record.row == nullptr)
    return;
  // Nothing may throw here: the exchange is about to end. A row that is not
  // remembered does not come back.
  try
    {
      char const *const identifier = record.row->entry->identifier;
      for (std::size_t i = 0; i < enabled_streams_.size (); ++i)
        if (std::strcmp (enabled_streams_[i].identifier, identifier) == 0)
          {
            enabled_streams_[i].message = record.message;
            return;
          }
      enabled_stream_t item;
      item.identifier = identifier;
      item.message = record.message;
      enabled_streams_.push_back (std::move (item));
    }
  catch (...)
    {
    }
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
