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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/detail/LumexDialogueImpl.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolScenario.hpp"
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
namespace pdsl = ::lumex::core::protocol::dsl;
using error::transport_errc;
using ::lumex::core::protocol::error::protocol_errc;

/// The component of the log records.
char const *const kcomponent = "transport.dialogue";

/// How many steps without a wait a run takes before it lets others run.
unsigned const krun_turn = 16;

std::error_code
dialogue_error (protocol_errc value)
{
  return ::lumex::core::protocol::error::make_error_code (value);
}

// --- the posted steps
// --------------------------------------------------------

struct begin_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::uint64_t id;
  pdsl::scenario_t scenario;
  protocol_features_t const *features;
  sequence_handler_t handler;

  void
  operator() ()
  {
    impl->run_begin_sequence (id, scenario, features, handler);
  }
};

struct enable_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::uint64_t id;
  sequence_handler_t handler;

  void
  operator() ()
  {
    impl->run_enable_streams (id, handler);
  }
};

struct resume_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::uint64_t id;

  void
  operator() ()
  {
    impl->sequence_advance (id);
  }
};

struct cancel_run_step_t
{
  std::shared_ptr<dialogue_impl> impl;
  std::uint64_t id;
  std::weak_ptr<tdetail::operation_mark> mark;

  void
  operator() ()
  {
    if (tdetail::take_cancellation_request (mark))
      impl->run_cancel_sequence (id);
  }
};

// The handler the engine puts in the cancellation slot of a run's handler: as
// the one of an exchange, but only a cancellation that allows side effects
// ends a run (its steps have side effects from the first one on).
class run_cancellation_t
{
public:
  run_cancellation_t (std::shared_ptr<dialogue_impl> const &impl,
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
            | asio::cancellation_type::partial))
        == asio::cancellation_type::none)
      return;
    std::shared_ptr<dialogue_impl> const impl = impl_.lock ();
    if (!impl || !tdetail::request_cancellation (mark_))
      return;
    try
      {
        impl->request_run_cancel (id_, mark_);
      }
    catch (...)
      {
        // Out of memory: the run goes on; a later signal may try again.
        tdetail::take_cancellation_request (mark_);
      }
  }

private:
  std::weak_ptr<dialogue_impl> impl_;
  std::uint64_t id_;
  std::weak_ptr<tdetail::operation_mark> mark_;
};

// The end of the exchange of a `send`.
struct exchange_end_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t run;
  std::uint64_t exchange;

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
      alive->sequence_exchange_ended (run, exchange, error, std::move (frame));
  }
};

// The end of the time of an `expect` or a `wait_state`.
struct step_wait_done_t
{
  using executor_type = dialogue_impl::strand_type;
  std::weak_ptr<dialogue_impl> impl;
  executor_type strand;
  std::uint64_t run;
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
      alive->sequence_timer_done (run, epoch, error);
  }
};

void
take_cancellation (std::shared_ptr<dialogue_impl> const &impl,
                   sequence_handler_t &handler, std::uint64_t id,
                   tdetail::operation_mark_ptr const &mark,
                   tdetail::mark_registry &marks)
{
  if (!mark)
    return;
  asio::cancellation_slot slot
      = asio::get_associated_cancellation_slot (handler);
  if (!slot.is_connected ())
    return;
  slot.template emplace<run_cancellation_t> (impl, id, mark);
  marks.watch (slot, mark);
}

row_features_t const *
find_features (protocol_features_t const *features, char const *identifier)
{
  if (features == nullptr)
    return nullptr;
  for (std::size_t i = 0; i < features->row_count; ++i)
    if (std::strcmp (features->rows[i].identifier, identifier) == 0)
      return &features->rows[i];
  return nullptr;
}
} // namespace

// --- any thread
// --------------------------------------------------------------

void
dialogue_impl::start_scenario (pdsl::scenario_t scenario,
                               protocol_features_t const *features,
                               sequence_handler_t handler,
                               tdetail::operation_mark_ptr const &mark)
{
  enlist ();
  std::uint64_t const id
      = next_id_.fetch_add (1, std::memory_order_relaxed) + 1;
  std::shared_ptr<dialogue_impl> const self = shared_from_this ();
  take_cancellation (self, handler, id, mark, marks_);
  begin_step_t step
      = { self, id, std::move (scenario), features, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::start_enable_streams (sequence_handler_t handler,
                                     tdetail::operation_mark_ptr const &mark)
{
  enlist ();
  std::uint64_t const id
      = next_id_.fetch_add (1, std::memory_order_relaxed) + 1;
  std::shared_ptr<dialogue_impl> const self = shared_from_this ();
  take_cancellation (self, handler, id, mark, marks_);
  enable_step_t step = { self, id, std::move (handler) };
  asio::post (strand_, std::move (step));
}

void
dialogue_impl::request_run_cancel (std::uint64_t id,
                                   std::weak_ptr<tdetail::operation_mark> mark)
{
  cancel_run_step_t step = { shared_from_this (), id, mark };
  asio::post (strand_, std::move (step));
}

// --- on the strand
// -----------------------------------------------------------

void
dialogue_impl::complete_sequence (sequence_handler_t handler,
                                  std::error_code error, std::size_t step)
{
  marks_.settle (handler);
  asio::post (strand_, asio::append (std::move (handler), error, step));
}

sequence_run_t *
dialogue_impl::find_run (std::uint64_t id) const LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < runs_.size (); ++i)
    if (runs_[i]->id == id)
      return runs_[i].get ();
  return nullptr;
}

void
dialogue_impl::run_begin_sequence (std::uint64_t id,
                                   pdsl::scenario_t &scenario,
                                   protocol_features_t const *features,
                                   sequence_handler_t &handler)
{
  begin_sequence (id, std::move (scenario), features, std::move (handler));
}

void
dialogue_impl::run_enable_streams (std::uint64_t id,
                                   sequence_handler_t &handler)
{
  pdsl::scenario_t steps;
  steps.name = "enable_streams";
  if (features_ != nullptr)
    for (std::size_t i = 0; i < features_->row_count; ++i)
      {
        row_features_t const &row = features_->rows[i];
        if (row.enables == nullptr)
          continue;
        pdsl::scenario_step_t step;
        step.kind = pdsl::scenario_step_kind::send;
        step.row = row.identifier;
        step.encode = row.empty_request;
        steps.steps.push_back (std::move (step));
      }
  begin_sequence (id, std::move (steps), features_, std::move (handler));
}

void
dialogue_impl::begin_sequence (std::uint64_t id, pdsl::scenario_t scenario,
                               protocol_features_t const *features,
                               sequence_handler_t handler)
{
  if (closed_)
    {
      complete_sequence (std::move (handler),
                         make_error_code (transport_errc::closed), 0);
      return;
    }
  std::error_code const invalid = scenario.check ();
  if (invalid)
    {
      LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                std::string ("the scenario ") + scenario.name
                                    + " has a label missing or given twice",
                                log::log_bytes_t ());
      complete_sequence (std::move (handler), invalid, 0);
      return;
    }
  std::unique_ptr<sequence_run_t> run (new sequence_run_t ());
  run->id = id;
  run->scenario = std::move (scenario);
  run->features = features;
  run->handler = std::move (handler);
  LUMEX_DIALOGUE_LOG_QUIET (info, kcomponent, name_.c_str (),
                            std::string ("the scenario ") + run->scenario.name
                                + " started",
                            log::log_bytes_t ());
  runs_.push_back (std::move (run));
  sequence_advance (id);
}

void
dialogue_impl::end_run (std::uint64_t id, std::error_code error)
{
  for (std::size_t i = 0; i < runs_.size (); ++i)
    {
      if (runs_[i]->id != id)
        continue;
      std::unique_ptr<sequence_run_t> run (std::move (runs_[i]));
      runs_.erase (runs_.begin () + static_cast<std::ptrdiff_t> (i));
      if (error::is_transport_error (error, transport_errc::cancelled))
        LUMEX_DIALOGUE_LOG_QUIET (
            info, kcomponent, name_.c_str (),
            std::string ("the scenario ") + run->scenario.name
                + " was cancelled at step " + std::to_string (run->step),
            log::log_bytes_t ());
      else if (error)
        LUMEX_DIALOGUE_LOG_QUIET (warning, kcomponent, name_.c_str (),
                                  std::string ("the scenario ")
                                      + run->scenario.name + " failed at step "
                                      + std::to_string (run->step) + ": "
                                      + error.message (),
                                  log::log_bytes_t ());
      else
        LUMEX_DIALOGUE_LOG_QUIET (info, kcomponent, name_.c_str (),
                                  std::string ("the scenario ")
                                      + run->scenario.name + " finished",
                                  log::log_bytes_t ());
      complete_sequence (std::move (run->handler), error, run->step);
      return;
    }
}

void
dialogue_impl::end_all_runs (std::error_code error)
{
  std::vector<std::uint64_t> ids;
  for (std::size_t i = 0; i < runs_.size (); ++i)
    ids.push_back (runs_[i]->id);
  for (std::size_t i = 0; i < ids.size (); ++i)
    end_run (ids[i], error);
}

void
dialogue_impl::run_cancel_sequence (std::uint64_t id)
{
  sequence_run_t *const run = find_run (id);
  if (run == nullptr)
    return;
  bool const exchanging = run->waiting == sequence_run_t::waiting_t::exchange;
  std::uint64_t const exchange = run->exchange_id;
  end_run (id, make_error_code (transport_errc::cancelled));
  if (exchanging)
    run_cancel_exchange (exchange, asio::cancellation_type::terminal);
}

void
dialogue_impl::remember_reply (sequence_run_t &run, pindex::row_t const *row,
                               link_frame_t const &frame)
{
  if (row == nullptr)
    return;
  try
    {
      for (std::size_t i = 0; i < run.replies.size (); ++i)
        if (run.replies[i].first == row)
          {
            run.replies[i].second = frame;
            return;
          }
      run.replies.push_back (std::make_pair (row, frame));
    }
  catch (...)
    {
      // Out of memory: a branch on the row then finds no reply; the step
      // itself goes on.
    }
}

void
dialogue_impl::note_held_row (sequence_run_t &run)
{
  run.has_held = false;
  run.held_row = nullptr;
  std::size_t const next = run.step + 1;
  if (next < run.scenario.steps.size ()
      && run.scenario.steps[next].kind == pdsl::scenario_step_kind::expect)
    run.held_row = index_now ().find_name (run.scenario.steps[next].row);
}

bool
dialogue_impl::run_send_step (sequence_run_t &run,
                              pdsl::scenario_step_t const &step)
{
  std::uint64_t const id = run.id;
  pindex::row_t const *const row = index_now ().find_name (step.row);
  if (row == nullptr)
    {
      end_run (id, dialogue_error (protocol_errc::not_supported));
      return false;
    }
  try
    {
      exchange_t request;
      ::lumex::core::protocol::field::decode_context_t const context
          = session_.decode_context ();
      std::error_code const encoded = step.encode (request.message, context);
      if (encoded)
        {
          end_run (id, encoded);
          return false;
        }
      fill_exchange (request, find_features (run.features, step.row), row);
      std::uint64_t const exchange
          = next_id_.fetch_add (1, std::memory_order_relaxed) + 1;
      exchange_end_t ended
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_, id,
              exchange };
      exchange_handler_t handler (std::move (ended));
      note_held_row (run);
      run.waiting = sequence_run_t::waiting_t::exchange;
      run.exchange_id = exchange;
      run_enqueue (exchange, request, handler);
    }
  catch (...)
    {
      end_run (id, make_error_code (std::errc::not_enough_memory));
      return false;
    }
  return true;
}

bool
dialogue_impl::run_wait_timer (sequence_run_t &run,
                               pdsl::scenario_step_t const &step)
{
  // A wait of the step before that is already on its way changes nothing,
  // also when this step waits for ever.
  run.timer_epoch = ++timer_sequence_;
  std::uint32_t const timeout
      = step.default_timeout ? defaults_->timeout_ms : step.timeout_ms;
  if (timeout == 0)
    return true;
  std::uint64_t const id = run.id;
  try
    {
      if (!run.timer.impl ())
        run.timer = timers_.make_timer (executor_type (strand_));
      step_wait_done_t done
          = { std::weak_ptr<dialogue_impl> (shared_from_this ()), strand_, id,
              run.timer_epoch };
      run.timer.async_wait_for (std::chrono::milliseconds (timeout),
                                std::move (done));
    }
  catch (...)
    {
      end_run (id, make_error_code (std::errc::not_enough_memory));
      return false;
    }
  return true;
}

// The steps that need no wait run here, a few at a time: a loop of branches
// gives way to the other handlers of the strand.
void
dialogue_impl::sequence_advance (std::uint64_t id)
{
  for (unsigned turn = 0; turn < krun_turn; ++turn)
    {
      sequence_run_t *const run = find_run (id);
      if (run == nullptr || run->waiting != sequence_run_t::waiting_t::nothing)
        return;
      if (run->step >= run->scenario.steps.size ())
        {
          end_run (id, std::error_code ());
          return;
        }
      pdsl::scenario_step_t const &step = run->scenario.steps[run->step];
      switch (step.kind)
        {
        case pdsl::scenario_step_kind::label:
          ++run->step;
          break;
        case pdsl::scenario_step_kind::send:
          run_send_step (*run, step);
          return;
        case pdsl::scenario_step_kind::expect:
          {
            pindex::row_t const *const row = index_now ().find_name (step.row);
            if (row == nullptr)
              {
                end_run (id, dialogue_error (protocol_errc::not_supported));
                return;
              }
            bool const held = run->has_held && run->held_row == row;
            run->has_held = false;
            run->held_row = nullptr;
            if (held)
              {
                ++run->step;
                break;
              }
            if (!run_wait_timer (*run, step))
              return;
            run->expected = row;
            run->waiting = sequence_run_t::waiting_t::frame;
            return;
          }
        case pdsl::scenario_step_kind::wait_state:
          {
            std::int64_t state = 0;
            if (read_state (state) && state == step.state)
              {
                ++run->step;
                break;
              }
          }
          if (!run_wait_timer (*run, step))
            return;
          run->waiting = sequence_run_t::waiting_t::state;
          return;
        case pdsl::scenario_step_kind::branch:
          {
            bool taken = true;
            if (step.decide)
              {
                pindex::row_t const *const row
                    = index_now ().find_name (step.row);
                link_frame_t const *last = nullptr;
                for (std::size_t i = 0; i < run->replies.size (); ++i)
                  if (run->replies[i].first == row)
                    last = &run->replies[i].second;
                if (last == nullptr)
                  {
                    end_run (id,
                             dialogue_error (protocol_errc::unexpected_reply));
                    return;
                  }
                ::lumex::core::protocol::field::decode_context_t const context
                    = session_.decode_context ();
                std::error_code decided;
                taken = false;
                try
                  {
                    decided = step.decide (
                        ::lumex::core::protocol::layout::bytes_view_t (
                            last->data (), last->size ()),
                        context, taken);
                  }
                catch (...)
                  {
                    // The run ends first; the exception then leaves the run ()
                    // of the executor.
                    std::exception_ptr const thrown
                        = std::current_exception ();
                    end_run (id, make_error_code (std::errc::interrupted));
                    std::rethrow_exception (thrown);
                  }
                if (decided)
                  {
                    end_run (id, decided);
                    return;
                  }
              }
            run->step = taken ? run->scenario.find_label (step.label)
                              : run->step + 1;
            break;
          }
        default:
          ++run->step;
          break;
        }
    }
  resume_step_t resume = { shared_from_this (), id };
  asio::post (strand_, std::move (resume));
}

void
dialogue_impl::sequence_exchange_ended (std::uint64_t id,
                                        std::uint64_t exchange,
                                        std::error_code error,
                                        link_frame_t frame)
{
  sequence_run_t *const run = find_run (id);
  if (run == nullptr || run->waiting != sequence_run_t::waiting_t::exchange
      || run->exchange_id != exchange)
    return;
  run->waiting = sequence_run_t::waiting_t::nothing;
  if (error)
    {
      end_run (id, error);
      return;
    }
  if (!frame.empty ())
    {
      // Under the row of the request and the row of the frame (the reply of
      // reply_row, an acknowledgement row).
      remember_reply (
          *run, index_now ().find_name (run->scenario.steps[run->step].row),
          frame);
      remember_reply (
          *run,
          index_now ()
              .match (::lumex::core::protocol::layout::bytes_view_t (
                  frame.data (), frame.size ()))
              .row,
          frame);
    }
  ++run->step;
  sequence_advance (id);
}

void
dialogue_impl::sequence_timer_done (std::uint64_t id, std::uint64_t epoch,
                                    std::error_code error)
{
  if (error)
    return;
  sequence_run_t *const run = find_run (id);
  if (run == nullptr || run->timer_epoch != epoch
      || (run->waiting != sequence_run_t::waiting_t::frame
          && run->waiting != sequence_run_t::waiting_t::state))
    return;
  end_run (id, dialogue_error (protocol_errc::timeout));
}

bool
dialogue_impl::offer_to_runs (pindex::row_t const *row,
                              link_frame_t const &frame,
                              std::vector<std::uint64_t> &ready)
{
  if (row == nullptr)
    return false;
  // Before the state of a run moves: a run that took a frame always goes on.
  ready.reserve (runs_.size ());
  bool taken = false;
  for (std::size_t i = 0; i < runs_.size (); ++i)
    {
      sequence_run_t &run = *runs_[i];
      if (run.waiting == sequence_run_t::waiting_t::frame
          && run.expected == row)
        {
          remember_reply (run, row, frame);
          run.waiting = sequence_run_t::waiting_t::nothing;
          run.expected = nullptr;
          run.timer.cancel ();
          ++run.step;
          ready.push_back (run.id);
          taken = true;
        }
      else if (run.waiting == sequence_run_t::waiting_t::exchange
               && run.held_row == row)
        {
          remember_reply (run, row, frame);
          run.has_held = true;
          taken = true;
        }
    }
  return taken;
}

void
dialogue_impl::wake_state_waiters (std::int64_t state)
{
  std::vector<std::uint64_t> ready;
  ready.reserve (runs_.size ());
  for (std::size_t i = 0; i < runs_.size (); ++i)
    {
      sequence_run_t &run = *runs_[i];
      if (run.waiting != sequence_run_t::waiting_t::state
          || run.scenario.steps[run.step].state != state)
        continue;
      run.waiting = sequence_run_t::waiting_t::nothing;
      run.timer.cancel ();
      ++run.step;
      ready.push_back (run.id);
    }
  std::exception_ptr thrown;
  for (std::size_t i = 0; i < ready.size (); ++i)
    try
      {
        sequence_advance (ready[i]);
      }
    catch (...)
      {
        if (!thrown)
          thrown = std::current_exception ();
      }
  if (thrown)
    std::rethrow_exception (thrown);
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
