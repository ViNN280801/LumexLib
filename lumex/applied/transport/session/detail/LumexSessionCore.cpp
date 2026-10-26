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
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionCore.hpp"

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
namespace session
{
namespace detail
{
namespace
{
using error::transport_errc;
typedef std::shared_ptr<session_core> core_ptr;

char const KSESSION_COMPONENT[] = "transport.session";

// The shortest wait between two attempts: a delay of 0 from the policy would
// retry back to back (REVIEW13 RT_K).
std::chrono::milliseconds::rep const KMIN_RETRY_DELAY_MS = 1;

// The state word: the state in the low 8 bits, the number of its changes in
// the rest.
unsigned const KSEQUENCE_SHIFT = 8u;
std::uint64_t const KSTATE_MASK = 0xffu;

channel_state
state_of (std::uint64_t word) LUMEX_NOEXCEPT
{
  return static_cast<channel_state> (static_cast<int> (word & KSTATE_MASK));
}

std::uint64_t
sequence_of (std::uint64_t word) LUMEX_NOEXCEPT
{
  return word >> KSEQUENCE_SHIFT;
}

std::uint64_t
make_word (channel_state state, std::uint64_t sequence) LUMEX_NOEXCEPT
{
  return (sequence << KSEQUENCE_SHIFT)
         | static_cast<std::uint64_t> (static_cast<int> (state));
}

bool
in_cycle (channel_state state) LUMEX_NOEXCEPT
{
  return state == channel_state::connecting
         || state == channel_state::reconnecting;
}

// The default random source: one generator per session, used on the strand
// only.
random_source_t
make_default_random (void const *salt)
{
  std::uint64_t const now = static_cast<std::uint64_t> (
      std::chrono::steady_clock::now ().time_since_epoch ().count ());
  std::uint64_t const address
      = static_cast<std::uint64_t> (reinterpret_cast<std::uintptr_t> (salt));
  std::shared_ptr<std::minstd_rand> const generator
      = std::make_shared<std::minstd_rand> (
          static_cast<std::minstd_rand::result_type> ((now ^ address)
                                                      % 2147483646u)
          + 1u);
  return [generator] ()
    {
      // generate_canonical never returns 1 by its contract; some library
      // versions do, and reconnect_delay clamps it anyway.
      return std::generate_canonical<double, 32> (*generator);
    };
}
} // namespace

std::string const &
empty_session_name () LUMEX_NOEXCEPT
{
  static std::string const name;
  return name;
}

// The posted steps reach the private members through it.
struct session_core_access
{
  static bool
  start (session_core &core, unsigned epoch)
  {
    return core.do_start (epoch);
  }
  static void
  stop (session_core &core, unsigned epoch, bool close_link)
  {
    core.do_stop (epoch, close_link);
  }
  static void
  presence (session_core &core, bool present)
  {
    core.do_presence (present);
  }
  static void
  state_wait (session_core &core, channel_state seen, std::uint64_t sequence,
              state_handler_t handler)
  {
    core.do_state_wait (seen, sequence, std::move (handler));
  }
  static void
  snapshot_wait (session_core &core, std::uint64_t sequence,
                 snapshot_handler_t handler)
  {
    core.do_snapshot_wait (sequence, std::move (handler));
  }
  static void
  open_wait (session_core &core, unsigned epoch, std::uint64_t id,
             status_handler_t handler)
  {
    core.do_open_wait (epoch, id, std::move (handler));
  }
  static void
  forget_open_wait (session_core &core, std::uint64_t id)
  {
    core.do_forget_open_wait (id);
  }
  static void
  retry_timer (session_core &core, unsigned wait_id)
  {
    core.on_retry_timer (wait_id);
  }
  static void
  open_timer (session_core &core, unsigned generation,
              std::error_code const &error)
  {
    core.on_open_timer (generation, error);
  }
};

namespace
{
// Each posted step is a small function object: C++11 lambdas cannot move a
// move-only handler into their capture.
struct post_state_wait
{
  core_ptr core;
  channel_state seen;
  std::uint64_t sequence;
  state_handler_t handler;
  void
  operator() ()
  {
    session_core_access::state_wait (*core, seen, sequence,
                                     std::move (handler));
  }
};

struct post_snapshot_wait
{
  core_ptr core;
  std::uint64_t sequence;
  snapshot_handler_t handler;
  void
  operator() ()
  {
    session_core_access::snapshot_wait (*core, sequence, std::move (handler));
  }
};

struct post_open_wait
{
  core_ptr core;
  unsigned epoch;
  std::uint64_t id;
  status_handler_t handler;
  void
  operator() ()
  {
    session_core_access::open_wait (*core, epoch, id, std::move (handler));
  }
};

struct retry_timer_done
{
  core_ptr core;
  unsigned wait_id;
  void
  operator() (std::error_code const &)
  {
    session_core_access::retry_timer (*core, wait_id);
  }
};

struct open_timer_done
{
  core_ptr core;
  unsigned generation;
  void
  operator() (std::error_code const &error)
  {
    session_core_access::open_timer (*core, generation, error);
  }
};

// The result of a waiter is fixed here: its mark is settled first, so a
// signal emitted while the handler waits on its executor ends nothing.
void
complete_state (session_core::strand_type const &strand,
                ::lumex::applied::transport::detail::mark_registry &marks,
                state_handler_t handler, std::error_code const &error,
                channel_state state)
{
  marks.settle (handler);
  asio::post (strand, asio::append (std::move (handler), error, state));
}

void
complete_snapshot (session_core::strand_type const &strand,
                   ::lumex::applied::transport::detail::mark_registry &marks,
                   snapshot_handler_t handler, std::error_code const &error,
                   state_snapshot_t const &snapshot)
{
  marks.settle (handler);
  asio::post (strand, asio::append (std::move (handler), error, snapshot));
}

void
complete_status (session_core::strand_type const &strand,
                 ::lumex::applied::transport::detail::mark_registry &marks,
                 status_handler_t handler, std::error_code const &error)
{
  marks.settle (handler);
  asio::post (strand, asio::append (std::move (handler), error));
}
} // namespace

session_core::session_core (executor_type const &executor,
                            session_options_t options)
    : executor_ (executor), strand_ (asio::make_strand (executor)),
      options_ (std::move (options)), retry_timer_ (strand_),
      open_timer_ (strand_),
      state_word_ (make_word (channel_state::closed, 0)), stop_epoch_ (0),
      detached_ (false), next_waiter_id_ (1), attempts_ (0),
      failed_attempts_ (0), reconnects_ (0), running_ (false), run_epoch_ (0),
      generation_ (0), wait_id_ (0), attempt_ (0), opening_ (false),
      waiting_retry_ (false), parked_ (false), device_absent_ (false),
      carried_data_ (false), lost_generation_ (0)
{
  if (options_.name.empty ())
    options_.name = "session";
  if (!options_.random)
    options_.random = make_default_random (this);
}

session_core::~session_core () {}

channel_state
session_core::state () const LUMEX_NOEXCEPT
{
  return state_of (state_word_.load (std::memory_order_acquire));
}

state_snapshot_t
session_core::state_snapshot () const LUMEX_NOEXCEPT
{
  std::uint64_t const word = state_word_.load (std::memory_order_acquire);
  state_snapshot_t snapshot;
  snapshot.state = state_of (word);
  snapshot.sequence = sequence_of (word);
  return snapshot;
}

std::error_code
session_core::last_error () const
{
  std::lock_guard<std::mutex> const lock (error_mutex_);
  return last_error_;
}

session_statistics_t
session_core::session_statistics () const LUMEX_NOEXCEPT
{
  session_statistics_t result;
  result.attempts = attempts_.load (std::memory_order_relaxed);
  result.failed_attempts = failed_attempts_.load (std::memory_order_relaxed);
  result.reconnects = reconnects_.load (std::memory_order_relaxed);
  return result;
}

void
session_core::reset_session_statistics () LUMEX_NOEXCEPT
{
  attempts_.store (0, std::memory_order_relaxed);
  failed_attempts_.store (0, std::memory_order_relaxed);
  reconnects_.store (0, std::memory_order_relaxed);
}

// --- any thread
// --------------------------------------------------------------

void
session_core::request_start ()
{
  // The epoch of the call: a close or cancel that comes after it (even if
  // its step runs first) makes the start void.
  unsigned const epoch = stop_epoch_.load (std::memory_order_acquire);
  core_ptr const self = shared_from_this ();
  asio::post (strand_,
              [self, epoch] () { session_core_access::start (*self, epoch); });
}

void
session_core::request_close ()
{
  unsigned const epoch
      = stop_epoch_.fetch_add (1, std::memory_order_acq_rel) + 1;
  core_ptr const self = shared_from_this ();
  asio::post (strand_, [self, epoch] ()
                { session_core_access::stop (*self, epoch, true); });
}

void
session_core::request_cancel ()
{
  unsigned const epoch
      = stop_epoch_.fetch_add (1, std::memory_order_acq_rel) + 1;
  core_ptr const self = shared_from_this ();
  asio::post (strand_, [self, epoch] ()
                { session_core_access::stop (*self, epoch, false); });
}

void
session_core::set_device_present (bool present)
{
  core_ptr const self = shared_from_this ();
  asio::post (strand_, [self, present] ()
                { session_core_access::presence (*self, present); });
}

void
session_core::detach () LUMEX_NOEXCEPT
{
  detached_.store (true, std::memory_order_release);
}

std::uint64_t
session_core::next_waiter_id () LUMEX_NOEXCEPT
{
  return next_waiter_id_.fetch_add (1, std::memory_order_relaxed);
}

void
session_core::forget_open_wait (std::uint64_t id)
{
  core_ptr const self = shared_from_this ();
  asio::post (strand_, [self, id] ()
                { session_core_access::forget_open_wait (*self, id); });
}

void
session_core::enqueue_state_wait (channel_state seen, std::uint64_t sequence,
                                  state_handler_t handler)
{
  post_state_wait step
      = { shared_from_this (), seen, sequence, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
session_core::enqueue_snapshot_wait (std::uint64_t sequence,
                                     snapshot_handler_t handler)
{
  post_snapshot_wait step
      = { shared_from_this (), sequence, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
session_core::enqueue_open_wait (std::uint64_t id, status_handler_t handler)
{
  unsigned const epoch = stop_epoch_.load (std::memory_order_acquire);
  post_open_wait step
      = { shared_from_this (), epoch, id, std::move (handler) };
  asio::post (strand_, std::move (step));
}

// --- the strand
// --------------------------------------------------------------

bool
session_core::running () const LUMEX_NOEXCEPT
{
  return running_
         && run_epoch_ == stop_epoch_.load (std::memory_order_acquire);
}

bool
session_core::usable (unsigned &generation) const LUMEX_NOEXCEPT
{
  generation = generation_;
  return running () && state () == channel_state::open;
}

bool
session_core::do_start (unsigned epoch)
{
  if (epoch != stop_epoch_.load (std::memory_order_acquire))
    return false;
  channel_state const now = state ();
  if (running_ && run_epoch_ == epoch
      && (now == channel_state::open || in_cycle (now)))
    return true;
  running_ = true;
  run_epoch_ = epoch;
  if (now == channel_state::open || in_cycle (now))
    return true;
  begin_cycle (channel_state::connecting, std::error_code ());
  return true;
}

void
session_core::do_stop (unsigned epoch, bool close_link)
{
  channel_state const now = state ();
  bool const cycling = in_cycle (now) || opening_;
  cancel_retry ();
  parked_ = false;
  opening_ = false;
  open_timer_.cancel ();
  cancel_waiters ();
  std::error_code const lost = lost_generation_ == generation_
                                   ? lost_while_stopping_
                                   : std::error_code ();
  lost_while_stopping_ = std::error_code ();
  if (close_link)
    {
      running_ = false;
      drop_current ();
      if (now != channel_state::closed)
        set_state (channel_state::closed, std::error_code ());
      return;
    }
  cancel_transport ();
  if (cycling)
    {
      running_ = false;
      drop_current ();
      set_state (channel_state::closed,
                 make_error_code (transport_errc::cancelled));
      return;
    }
  if (now != channel_state::open || !running_)
    return;
  if (stop_epoch_.load (std::memory_order_acquire) != epoch)
    {
      // Another stop came after this one: its own step decides (and
      // replays the loss when it is a cancel too).
      if (lost)
        {
          lost_while_stopping_ = lost;
          lost_generation_ = generation_;
        }
      return;
    }
  // Open: only the I/O ends and the session keeps watching its link; a loss
  // or a removal seen while this cancel was on its way ends the link now
  // (REVIEW13 L2).
  run_epoch_ = epoch;
  if (lost)
    lose_link (lost);
}

void
session_core::do_presence (bool present)
{
  if (present)
    {
      device_absent_ = false;
      if (!running ())
        return;
      if (parked_)
        {
          parked_ = false;
          LUMEX_TRANSPORT_LOG (info, KSESSION_COMPONENT, name ().c_str (),
                               "device arrived: attempt now",
                               log::log_bytes_t ());
          attempt ();
        }
      else if (waiting_retry_)
        {
          cancel_retry ();
          attempt ();
        }
      return;
    }
  device_absent_ = true;
  if (!running ())
    {
      // A stop is on its way: a cancel keeps an open link, so its step
      // replays this removal (only one seen while it was on its way).
      if (running_ && state () == channel_state::open
          && !(lost_while_stopping_ && lost_generation_ == generation_))
        {
          lost_while_stopping_
              = make_error_code (transport_errc::disconnected);
          lost_generation_ = generation_;
        }
      return;
    }
  if (state () == channel_state::open)
    {
      lose_link (make_error_code (transport_errc::disconnected));
      return;
    }
  if (waiting_retry_)
    {
      cancel_retry ();
      parked_ = true;
      LUMEX_TRANSPORT_LOG (info, KSESSION_COMPONENT, name ().c_str (),
                           "device removed: waiting for it",
                           log::log_bytes_t ());
    }
}

void
session_core::do_state_wait (channel_state seen, std::uint64_t sequence,
                             state_handler_t handler)
{
  // A change after the call (the sequence moved) is reported even when the
  // state came back to `seen`.
  state_snapshot_t const now = state_snapshot ();
  if (now.state != seen || now.sequence != sequence)
    {
      complete_state (strand_, marks_, std::move (handler), std::error_code (),
                      now.state);
      return;
    }
  state_waiters_.push_back (std::move (handler));
}

void
session_core::do_snapshot_wait (std::uint64_t sequence,
                                snapshot_handler_t handler)
{
  state_snapshot_t const now = state_snapshot ();
  if (now.sequence != sequence)
    {
      complete_snapshot (strand_, marks_, std::move (handler),
                         std::error_code (), now);
      return;
    }
  snapshot_waiters_.push_back (std::move (handler));
}

void
session_core::do_open_wait (unsigned epoch, std::uint64_t id,
                            status_handler_t handler)
{
  if (!do_start (epoch))
    {
      complete_status (strand_, marks_, std::move (handler),
                       make_error_code (transport_errc::cancelled));
      return;
    }
  // The cycle may have ended inside do_start already (a factory that throws
  // with max_attempts 1 faults at once): settle with the outcome now.
  channel_state const now = state ();
  if (now == channel_state::open)
    {
      complete_status (strand_, marks_, std::move (handler),
                       std::error_code ());
      return;
    }
  if (now == channel_state::faulted)
    {
      complete_status (strand_, marks_, std::move (handler), last_error ());
      return;
    }
  open_waiter waiter = { id, std::move (handler) };
  open_waiters_.push_back (std::move (waiter));
}

void
session_core::do_forget_open_wait (std::uint64_t id)
{
  for (std::vector<open_waiter>::iterator it = open_waiters_.begin ();
       it != open_waiters_.end (); ++it)
    if (it->id == id)
      {
        open_waiters_.erase (it);
        return;
      }
}

void
session_core::on_retry_timer (unsigned wait_id)
{
  // A cancelled wait has a newer wait_id_ (cancel_retry bumps it), also when
  // its completion was already queued with success.
  if (wait_id != wait_id_ || !waiting_retry_)
    return;
  waiting_retry_ = false;
  attempt ();
}

void
session_core::on_open_timer (unsigned generation, std::error_code const &error)
{
  if (error || generation != generation_ || !opening_)
    return;
  opening_ = false;
  // A stop is queued: its step closes this transport.
  if (!running ())
    return;
  LUMEX_TRANSPORT_LOG (warning, KSESSION_COMPONENT, name ().c_str (),
                       "open timed out", log::log_bytes_t ());
  // The attempt fails now, not when the transport gives its open up (the
  // bounded open of the serial stream ignores cancel): fail_attempt drops
  // the transport, whose close makes it close what it opens late, and its
  // completion sees a newer generation (REVIEW13 RT_B).
  fail_attempt (make_error_code (transport_errc::timeout));
}

void
session_core::on_opened (unsigned generation, std::error_code const &error)
{
  if (generation != generation_ || !opening_)
    return;
  opening_ = false;
  open_timer_.cancel ();
  // A stop is queued: its step closes this transport.
  if (!running ())
    return;
  if (error)
    {
      fail_attempt (error);
      return;
    }
  if (state () == channel_state::reconnecting)
    reconnects_.fetch_add (1, std::memory_order_relaxed);
  // attempt_ stays: the link counts as established only once it stayed
  // open (see lose_link). The device is there: a removal reported while the
  // open ran is stale (REVIEW13 re-review N3).
  opened_at_ = std::chrono::steady_clock::now ();
  carried_data_ = false;
  device_absent_ = false;
  set_state (channel_state::open, std::error_code ());
}

void
session_core::note_io_result (unsigned generation,
                              std::error_code const &error, bool carried)
{
  if (generation != generation_)
    return;
  if (carried)
    carried_data_ = true;
  if (is_link_lost (error))
    lose_link (error);
}

void
session_core::begin_cycle (channel_state state, std::error_code const &reason)
{
  attempt_ = 0;
  set_state (state, reason);
  attempt ();
}

void
session_core::attempt ()
{
  if (!running ())
    return;
  if (device_absent_)
    {
      parked_ = true;
      LUMEX_TRANSPORT_LOG (info, KSESSION_COMPONENT, name ().c_str (),
                           "device absent: waiting for it",
                           log::log_bytes_t ());
      return;
    }
  parked_ = false;
  ++attempt_;
  attempts_.fetch_add (1, std::memory_order_relaxed);
  drop_current ();
  // A factory that throws is a failed attempt, not a crash.
  std::error_code made;
  try
    {
      make_transport ();
    }
  catch (std::system_error const &failure)
    {
      made = failure.code ();
      if (!made)
        made = make_error_code (transport_errc::invalid_endpoint);
      LUMEX_TRANSPORT_LOG (warning, KSESSION_COMPONENT, name ().c_str (),
                           std::string ("factory failed: ") + failure.what (),
                           log::log_bytes_t ());
    }
  catch (std::exception const &failure)
    {
      made = make_error_code (transport_errc::invalid_endpoint);
      LUMEX_TRANSPORT_LOG (warning, KSESSION_COMPONENT, name ().c_str (),
                           std::string ("factory failed: ") + failure.what (),
                           log::log_bytes_t ());
    }
  catch (...)
    {
      made = make_error_code (transport_errc::invalid_endpoint);
      LUMEX_TRANSPORT_LOG (warning, KSESSION_COMPONENT, name ().c_str (),
                           "factory failed", log::log_bytes_t ());
    }
  if (made)
    {
      fail_attempt (made);
      return;
    }
  // A close () or cancel () (or the destruction of the session) that came
  // while the factory ran: the transport it made is not opened (REVIEW13
  // RT_D, RT_F); the step of the stop follows.
  if (!running ())
    {
      drop_current ();
      return;
    }
  opening_ = true;
  unsigned const generation = generation_;
  if (options_.open_timeout.count () > 0)
    {
      open_timer_.expires_after (options_.open_timeout);
      open_timer_done done = { shared_from_this (), generation };
      open_timer_.async_wait (std::move (done));
    }
  open_transport (generation);
}

void
session_core::fail_attempt (std::error_code const &error)
{
  failed_attempts_.fetch_add (1, std::memory_order_relaxed);
  set_last_error (error);
  LUMEX_TRANSPORT_LOG (warning, KSESSION_COMPONENT, name ().c_str (),
                       "attempt " + std::to_string (attempt_)
                           + " failed: " + error.message (),
                       log::log_bytes_t ());
  drop_current ();
  if (!running ())
    return;
  if (options_.policy.max_attempts != 0
      && attempt_ >= options_.policy.max_attempts)
    {
      running_ = false;
      set_state (channel_state::faulted, error);
      return;
    }
  if (device_absent_)
    {
      parked_ = true;
      return;
    }
  schedule_retry (attempt_);
}

void
session_core::schedule_retry (std::uint32_t failed_attempt)
{
  std::chrono::milliseconds delay
      = reconnect_delay (options_.policy, failed_attempt, draw_random ());
  if (delay.count () < KMIN_RETRY_DELAY_MS)
    delay = std::chrono::milliseconds (KMIN_RETRY_DELAY_MS);
  waiting_retry_ = true;
  unsigned const wait_id = ++wait_id_;
  retry_timer_.expires_after (delay);
  retry_timer_done done = { shared_from_this (), wait_id };
  retry_timer_.async_wait (std::move (done));
}

void
session_core::cancel_retry ()
{
  ++wait_id_;
  waiting_retry_ = false;
  retry_timer_.cancel ();
}

bool
session_core::established () const
{
  std::chrono::milliseconds::rep const after
      = std::max (options_.policy.initial_delay.count (), KMIN_RETRY_DELAY_MS);
  return std::chrono::steady_clock::now () - opened_at_
         >= std::chrono::milliseconds (after);
}

void
session_core::lose_link (std::error_code const &error)
{
  if (state () != channel_state::open)
    return;
  if (!running ())
    {
      // A stop is on its way. A close drops the link anyway; a cancel keeps
      // it, so its step replays this loss (REVIEW13 L2).
      if (running_)
        {
          lost_while_stopping_ = error;
          lost_generation_ = generation_;
        }
      return;
    }
  set_last_error (error);
  bool const was_established = established ();
  bool const carried = carried_data_;
  LUMEX_TRANSPORT_LOG (warning, KSESSION_COMPONENT, name ().c_str (),
                       "link lost: " + error.message (), log::log_bytes_t ());
  drop_current ();
  if (was_established)
    {
      begin_cycle (channel_state::reconnecting, error);
      return;
    }
  // Gone before it was established: the next attempt waits its delay
  // (REVIEW13 RT_C).
  set_state (channel_state::reconnecting, error);
  if (!carried)
    {
      // It moved no data (a peer that accepts and closes, a device that
      // resets on open): a failed attempt of the cycle that opened it, which
      // keeps its backoff and its max_attempts.
      fail_attempt (error);
      return;
    }
  // It worked for a while: a new cycle, whose first attempt waits the delay
  // (REVIEW13 re-review N2).
  attempt_ = 0;
  if (!running ())
    return;
  if (device_absent_)
    {
      parked_ = true;
      return;
    }
  schedule_retry (1);
}

void
session_core::drop_current () LUMEX_NOEXCEPT
{
  // A late completion of the transport dropped here sees a newer generation.
  ++generation_;
  drop_transport ();
}

void
session_core::set_state (channel_state state, std::error_code const &reason)
{
  // Only the strand writes the word.
  std::uint64_t const word = state_word_.load (std::memory_order_acquire);
  channel_state const previous = state_of (word);
  if (previous == state)
    return;
  state_snapshot_t snapshot;
  snapshot.state = state;
  snapshot.sequence = sequence_of (word) + 1;
  state_word_.store (make_word (state, snapshot.sequence),
                     std::memory_order_release);
  LUMEX_TRANSPORT_LOG (info, KSESSION_COMPONENT, name ().c_str (),
                       std::string (to_string (previous)) + " -> "
                           + to_string (state),
                       log::log_bytes_t ());

  std::vector<state_handler_t> waiters;
  waiters.swap (state_waiters_);
  for (std::size_t i = 0; i < waiters.size (); ++i)
    complete_state (strand_, marks_, std::move (waiters[i]),
                    std::error_code (), state);
  std::vector<snapshot_handler_t> snapshots;
  snapshots.swap (snapshot_waiters_);
  for (std::size_t i = 0; i < snapshots.size (); ++i)
    complete_snapshot (strand_, marks_, std::move (snapshots[i]),
                       std::error_code (), snapshot);

  if (state == channel_state::open || state == channel_state::faulted
      || state == channel_state::closed)
    {
      std::error_code const outcome
          = state == channel_state::open ? std::error_code ()
            : state == channel_state::faulted
                ? reason
                : make_error_code (transport_errc::cancelled);
      std::vector<open_waiter> opens;
      opens.swap (open_waiters_);
      for (std::size_t i = 0; i < opens.size (); ++i)
        complete_status (strand_, marks_, std::move (opens[i].handler),
                         outcome);
    }

  if (!options_.on_state_change || detached_.load (std::memory_order_acquire))
    return;
  state_change_t change;
  change.previous = previous;
  change.current = state;
  change.error = reason;
  change.attempt = attempt_;
  change.sequence = snapshot.sequence;
  try
    {
      options_.on_state_change (change);
    }
  catch (std::exception const &failure)
    {
      LUMEX_TRANSPORT_LOG (error, KSESSION_COMPONENT, name ().c_str (),
                           std::string ("state callback threw: ")
                               + failure.what (),
                           log::log_bytes_t ());
    }
  catch (...)
    {
      LUMEX_TRANSPORT_LOG (error, KSESSION_COMPONENT, name ().c_str (),
                           "state callback threw", log::log_bytes_t ());
    }
}

void
session_core::set_last_error (std::error_code const &error)
{
  std::lock_guard<std::mutex> const lock (error_mutex_);
  last_error_ = error;
}

void
session_core::cancel_waiters ()
{
  std::error_code const cancelled
      = make_error_code (transport_errc::cancelled);
  state_snapshot_t const now = state_snapshot ();
  std::vector<state_handler_t> waiters;
  waiters.swap (state_waiters_);
  for (std::size_t i = 0; i < waiters.size (); ++i)
    complete_state (strand_, marks_, std::move (waiters[i]), cancelled,
                    now.state);
  std::vector<snapshot_handler_t> snapshots;
  snapshots.swap (snapshot_waiters_);
  for (std::size_t i = 0; i < snapshots.size (); ++i)
    complete_snapshot (strand_, marks_, std::move (snapshots[i]), cancelled,
                       now);
  std::vector<open_waiter> opens;
  opens.swap (open_waiters_);
  for (std::size_t i = 0; i < opens.size (); ++i)
    complete_status (strand_, marks_, std::move (opens[i].handler), cancelled);
}

double
session_core::draw_random ()
{
  try
    {
      return options_.random ();
    }
  catch (...)
    {
      return 0.5;
    }
}
} // namespace detail
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
