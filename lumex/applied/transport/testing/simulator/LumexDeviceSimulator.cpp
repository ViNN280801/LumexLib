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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/testing/simulator/LumexDeviceSimulator.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"

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
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#if __has_warning("-Wlifetime-safety-invalidation")
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#if __has_warning("-Wlifetime-safety-suggestions")
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
#if __has_warning("-Wlifetime-safety-cross-tu-suggestions")
#pragma clang diagnostic ignored "-Wlifetime-safety-cross-tu-suggestions"
#endif
#if __has_warning("-Wlifetime-safety-intra-tu-constructor-suggestions")
#pragma clang diagnostic ignored                                              \
    "-Wlifetime-safety-intra-tu-constructor-suggestions"
#endif
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace testing
{
std::uint64_t
simulator_statistics_t::requests_of (char const *identifier) const
{
  for (std::size_t i = 0; i < by_row.size (); ++i)
    if (by_row[i].row == identifier)
      return by_row[i].requests;
  return 0;
}

namespace simulator_detail
{
simulator_wire_t::~simulator_wire_t () {}

simulator_api::~simulator_api () {}

namespace
{
bool
acceptable (registry::registry_entry_t const &entry, std::int64_t raw)
{
  return registry::raw_in_range (entry, raw)
         && !registry::check_limits (entry,
                                     registry::to_engineering (entry, raw));
}
} // namespace

std::int64_t
default_raw (registry::registry_entry_t const &entry)
{
  if (acceptable (entry, 0))
    return 0;
  std::int64_t raw = 0;
  if (entry.has_limits && registry::to_raw (entry, entry.lower, raw)
      && acceptable (entry, raw))
    return raw;
  if (entry.has_limits && registry::to_raw (entry, entry.upper, raw)
      && acceptable (entry, raw))
    return raw;
  return 0;
}

std::int64_t
stored_or_default (registry_store_t const &store, registry_slot_t const &slot,
                   registry::registry_entry_t const &entry)
{
  registry_store_t::const_iterator const found = store.find (slot);
  return found != store.end () ? found->second : default_raw (entry);
}

bool
raw_of (registry::registry_entry_t const &entry,
        registry::parameter_value_t const &value, std::int64_t &raw)
{
  if (value.has_raw)
    {
      raw = value.raw;
      return registry::raw_in_range (entry, raw);
    }
  return registry::to_raw (entry, value.engineering, raw);
}

void
set_raw (registry::parameter_value_t &value,
         registry::registry_entry_t const &entry, std::int64_t raw)
{
  value = registry::parameter_value_t::of_raw (raw);
  value.engineering = registry::to_engineering (entry, raw);
  value.entry = &entry;
}

namespace
{
using ms = std::chrono::milliseconds;
using strand_t = asio::strand<executor_type>;

// Thrown inside the code that makes a reply when the simulator was
// destroyed meanwhile: the request is dropped, nothing is sent.
struct simulator_gone_t
{
};

// Why a message is sent: the reply to a request, a stream message, or a
// message a played scenario makes the device send by itself.
enum class send_kind : std::uint8_t
{
  request,
  stream,
  scripted
};

// How many jumps back, one after the other with no `send` between, a script
// may take: a loop that never waits would not end (it would flood the mock).
LUMEX_CONSTEXPR std::size_t kmax_idle_loops = 1000;

class simulator_core;

// The mock the simulator sits on: a stream or a datagram transport.
class target_t
{
public:
  target_t () = default;
  virtual ~target_t () {}
  target_t (target_t const &) = delete;
  target_t &operator= (target_t const &) = delete;

  virtual strand_t strand () const = 0;
  virtual void observe (std::weak_ptr<simulator_core> const &core) = 0;
  virtual void unobserve () = 0;
  // The lazy respond of the applier; true when the device processed.
  virtual bool respond (behavior_applier &applier, bytes_t const &request,
                        bool has_reply, reply_maker_t const &make,
                        bool *reached)
      = 0;

  virtual void attach (behavior_applier &applier) = 0;
};

// The engine, over a stream or a datagram transport.
class simulator_core final
    : public simulator_api,
      public ::lumex::applied::transport::detail::handler_keeper,
      public std::enable_shared_from_this<simulator_core>
{
public:
  simulator_core (index::protocol_index_t const *table, row_ops_t const *ops,
                  std::size_t op_count, std::unique_ptr<simulator_wire_t> wire,
                  std::unique_ptr<target_t> target, virtual_clock &clock,
                  simulator_options_t const &options, std::uint8_t report_id)
      : table_ (table), wire_ (std::move (wire)), target_ (std::move (target)),
        timers_ (clock.source ()), options_ (options), report_id_ (report_id),
        dead_ (false), state_ (0), next_id_ (0)
  {
    if (table_ != nullptr)
      {
        std::size_t const rows = table_->rows ().size ();
        ops_.assign (rows, static_cast<row_ops_t const *> (nullptr));
        answers_.assign (rows, std::shared_ptr<answer_fn_t const> ());
        messages_.assign (rows, std::shared_ptr<answer_fn_t const> ());
        refusing_.assign (rows, 0);
        row_requests_.assign (rows, 0);
        for (std::size_t i = 0; i < rows; ++i)
          for (std::size_t n = 0; n < op_count; ++n)
            if (std::strcmp (ops[n].identifier,
                             table_->rows ().data ()[i].entry->identifier)
                == 0)
              {
                ops_[i] = ops + n;
                break;
              }
      }
    applier_
        = std::make_shared<behavior_applier> (behavior::perfect (), timers_);
  }

  // The mock the simulator sits on; null once the io_context is gone.
  std::shared_ptr<target_t>
  target () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return target_;
  }

  // Called once, on the made object: enlists with the shutdown service and
  // listens to the mock.
  void
  start ()
  {
    if (table_ == nullptr)
      return;
    ::lumex::applied::transport::detail::enlist_keeper_with (
        asio::query (target_->strand (), asio::execution::context),
        std::static_pointer_cast<
            ::lumex::applied::transport::detail::handler_keeper> (
            shared_from_this ()));
    target_->observe (shared_from_this ());
  }

  void
  shutdown () override
  {
    answers_t answers;
    answers_t messages;
    streams_t streams;
    scheduled_t scheduled;
    std::shared_ptr<play_t> played;
    std::shared_ptr<target_t> target;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.exchange (true))
        return;
      answers.swap (answers_);
      messages.swap (messages_);
      streams.swap (streams_);
      scheduled.swap (scheduled_);
      played.swap (play_);
      cancel_playback ();
      target = target_;
    }
    if (table_ != nullptr && target)
      target->unobserve ();
    // The handlers, generators and timers of the test die here, outside the
    // lock and after the mock cannot call them any more.
  }

  // The io_context is being destroyed: the functions of the test go, so that
  // one that holds the simulator does not keep it alive for ever, and so does
  // the mock (its strand belongs to the context and must not outlive it). The
  // simulator is dead from here on: its streams and steps are refused. The
  // timers stay (a virtual timer outlives its io_context, and goes with the
  // simulator).
  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    answers_t answers;
    answers_t messages;
    std::shared_ptr<play_t> played;
    std::vector<std::shared_ptr<answer_fn_t const>> makes;
    std::vector<std::function<void ()>> actions;
    std::shared_ptr<target_t> target;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      dead_.store (true);
      target.swap (target_);
      answers.swap (answers_);
      messages.swap (messages_);
      played.swap (play_);
      cancel_playback ();
      for (streams_t::iterator at = streams_.begin (); at != streams_.end ();
           ++at)
        {
          makes.push_back (std::shared_ptr<answer_fn_t const> ());
          makes.back ().swap (at->second->make);
        }
      for (scheduled_t::iterator at = scheduled_.begin ();
           at != scheduled_.end (); ++at)
        {
          actions.push_back (std::function<void ()> ());
          actions.back ().swap (at->second->action);
        }
      answers_.assign (answers.size (), std::shared_ptr<answer_fn_t const> ());
      messages_.assign (messages.size (),
                        std::shared_ptr<answer_fn_t const> ());
    }
    // The functions die here, outside the lock.
  }

  bool
  valid () const override
  {
    return table_ != nullptr;
  }

  index::row_t const *
  find_row (char const *identifier) const override
  {
    if (table_ == nullptr)
      return nullptr;
    index::row_t const *const rows = table_->rows ().data ();
    for (std::size_t i = 0; i < table_->rows ().size (); ++i)
      if (std::strcmp (rows[i].entry->identifier, identifier) == 0)
        return rows + i;
    return nullptr;
  }

  void
  set_answer (index::row_t const *row, answer_fn_t answer) override
  {
    std::shared_ptr<answer_fn_t const> kept;
    if (answer)
      kept = std::make_shared<answer_fn_t const> (std::move (answer));
    std::shared_ptr<answer_fn_t const> old;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.load ())
        return;
      old = answers_[position (row)];
      answers_[position (row)] = kept;
    }
  }

  void
  set_message (index::row_t const *row, answer_fn_t message) override
  {
    std::shared_ptr<answer_fn_t const> kept;
    if (message)
      kept = std::make_shared<answer_fn_t const> (std::move (message));
    std::shared_ptr<answer_fn_t const> old;
    std::lock_guard<std::mutex> const lock (mutex_);
    if (dead_.load ())
      return;
    old = messages_[position (row)];
    messages_[position (row)] = kept;
    // `old` dies after the lock: declared before it.
  }

  bool
  play (dsl::scenario_t scenario, char const *state_row) override
  {
    std::shared_ptr<target_t> const mock = target ();
    if (table_ == nullptr || !mock)
      return false;
    std::error_code const invalid = scenario.check ();
    // Both die after the lock: the predicates of the test are in them.
    std::shared_ptr<play_t> fresh = std::make_shared<play_t> ();
    std::shared_ptr<play_t> replaced;
    fresh->scenario = std::move (scenario);
    fresh->state_row = state_row;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.load () || play_status_.state == playback_state::playing)
        return false;
      replaced.swap (play_);
      play_status_ = playback_status_t ();
      play_status_.steps = fresh->scenario.steps.size ();
      if (invalid)
        {
          play_status_.state = playback_state::failed;
          play_status_.error = invalid;
          return false;
        }
      play_status_.state = playback_state::playing;
      fresh->id = ++next_id_;
      play_ = fresh;
    }
    std::weak_ptr<simulator_core> const weak = shared_from_this ();
    std::uint64_t const id = fresh->id;
    // The first steps run on the strand, also when play () is called from
    // another thread or from a handler of the simulator.
    asio::post (mock->strand (),
                [weak, id] ()
                  {
                    std::shared_ptr<simulator_core> const self = weak.lock ();
                    if (self)
                      self->play_advance (id);
                  });
    return true;
  }

  playback_status_t
  playback () const override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return play_status_;
  }

  void
  set_refusing (index::row_t const *row, bool refusing) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (!dead_.load ())
      refusing_[position (row)] = refusing ? 1 : 0;
  }

  bool
  start_stream (index::row_t const *row, ms interval,
                answer_fn_t make) override
  {
    std::shared_ptr<target_t> const mock = target ();
    if (interval.count () <= 0 || !make || !mock)
      return false;
    std::unique_ptr<stream_t> fresh (
        new stream_t (timers_.make_timer (executor_type (mock->strand ()))));
    fresh->interval = interval;
    fresh->make = std::make_shared<answer_fn_t const> (std::move (make));
    std::unique_ptr<stream_t> replaced;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.load ())
        return false;
      fresh->id = ++next_id_;
      std::size_t const at = position (row);
      std::unique_ptr<stream_t> &slot = streams_[at];
      replaced.swap (slot);
      slot.swap (fresh);
      arm_stream (at, *slot);
    }
    return true;
  }

  std::size_t
  start_default_streams () override
  {
    std::size_t started = 0;
    if (table_ == nullptr)
      return 0;
    for (std::size_t i = 0; i < table_->rows ().size (); ++i)
      {
        row_ops_t const *const ops = ops_[i];
        index::row_t const &row = table_->rows ().data ()[i];
        if (ops == nullptr || ops->stream == nullptr
            || row.entry->options.stream_interval_ms == 0)
          continue;
        if (start_stream (&row, ms (row.entry->options.stream_interval_ms),
                          answer_fn_t (ops->stream)))
          ++started;
      }
    return started;
  }

  void
  stop_stream (index::row_t const *row) override
  {
    std::unique_ptr<stream_t> stopped;
    std::lock_guard<std::mutex> const lock (mutex_);
    std::map<std::size_t, std::unique_ptr<stream_t>>::iterator const found
        = streams_.find (position (row));
    if (found == streams_.end ())
      return;
    stopped.swap (found->second);
    streams_.erase (found);
    // `stopped` dies after the lock: declared before it.
  }

  void
  stop_streams () override
  {
    streams_t stopped;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      stopped.swap (streams_);
    }
  }

  ms
  stream_interval (index::row_t const *row) const override
  {
    return row == nullptr ? ms (0)
                          : ms (row->entry->options.stream_interval_ms);
  }

  void
  set_state (std::int64_t state) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    state_ = state;
  }

  std::int64_t
  state () const override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return state_;
  }

  void
  set_behavior (behavior const &shaped) override
  {
    std::shared_ptr<behavior_applier> fresh
        = std::make_shared<behavior_applier> (shaped, timers_);
    std::shared_ptr<target_t> const mock = target ();
    if (table_ != nullptr && mock)
      mock->attach (*fresh);
    std::shared_ptr<behavior_applier> old;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      old = applier_;
      applier_ = fresh;
    }
  }

  void
  after (ms delay, std::function<void ()> action) override
  {
    std::shared_ptr<target_t> const mock = target ();
    if (!action || table_ == nullptr || !mock)
      return;
    std::unique_ptr<step_t> step (
        new step_t (timers_.make_timer (executor_type (mock->strand ()))));
    step->action = std::move (action);
    std::lock_guard<std::mutex> const lock (mutex_);
    if (dead_.load ())
      return;
    std::uint64_t const id = ++next_id_;
    std::weak_ptr<simulator_core> const weak = shared_from_this ();
    step->timer.async_wait_for (delay,
                                [weak, id] (std::error_code error)
                                  {
                                    if (error)
                                      return;
                                    std::shared_ptr<simulator_core> const self
                                        = weak.lock ();
                                    if (self)
                                      self->run_step (id);
                                  });
    scheduled_[id] = std::move (step);
  }

  index::session_context_t &
  session () override
  {
    return session_;
  }

  simulator_statistics_t
  statistics () const override
  {
    simulator_statistics_t copy;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      copy = stats_;
      for (std::size_t i = 0; i < row_requests_.size (); ++i)
        if (row_requests_[i] != 0)
          {
            simulator_row_count_t entry;
            entry.row = table_->rows ().data ()[i].entry->identifier;
            entry.requests = row_requests_[i];
            copy.by_row.push_back (entry);
          }
    }
    return copy;
  }

  void
  reset_statistics () override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    stats_ = simulator_statistics_t ();
    std::fill (row_requests_.begin (), row_requests_.end (),
               static_cast<std::uint64_t> (0));
  }

  // --- what the mock tells (on its strand)

  void
  take_chunk (bytes_t const &chunk)
  {
    if (dead_.load () || chunk.empty ())
      return;
    std::vector<bytes_t> frames;
    std::size_t const dropped
        = wire_->decode (chunk.data (), chunk.size (), frames);
    if (dropped != 0)
      {
        std::lock_guard<std::mutex> const lock (mutex_);
        stats_.framing_errors += dropped;
      }
    serve_all (frames);
  }

  void
  take_datagram (datagram::message_t const &message)
  {
    if (dead_.load ())
      return;
    if (message.report_id != report_id_)
      {
        std::lock_guard<std::mutex> const lock (mutex_);
        ++stats_.foreign_reports;
        return;
      }
    // A datagram is a unit of its own: the codec starts afresh.
    wire_->reset ();
    std::vector<bytes_t> frames;
    std::size_t dropped = wire_->decode (message.payload.data (),
                                         message.payload.size (), frames);
    // A datagram of ours with no frame in it (empty or cut) is a framing
    // error.
    if (frames.empty () && dropped == 0)
      dropped = 1;
    if (dropped != 0)
      {
        std::lock_guard<std::mutex> const lock (mutex_);
        stats_.framing_errors += dropped;
      }
    serve_all (frames);
  }

private:
  struct stream_t
  {
    explicit stream_t (timing::timer made) : timer (std::move (made)) {}

    std::uint64_t id = 0;
    ms interval;
    std::shared_ptr<answer_fn_t const> make;
    timing::timer timer;
  };

  struct step_t
  {
    explicit step_t (timing::timer made) : timer (std::move (made)) {}

    timing::timer timer;
    std::function<void ()> action;
  };

  // The scenario being played; the strand owns everything but `id`.
  struct play_t
  {
    dsl::scenario_t scenario;
    // The identifier of the row of `state_from`, null without one.
    char const *state_row = nullptr;
    std::uint64_t id = 0;
    // The step it stands at.
    std::size_t step = 0;
    // It ended or failed.
    bool over = false;
    // The last message of each row, as the dialogue keeps the last reply.
    std::vector<std::pair<index::row_t const *, bytes_t>> last;
  };

  using answers_t = std::vector<std::shared_ptr<answer_fn_t const>>;
  using streams_t = std::map<std::size_t, std::unique_ptr<stream_t>>;
  using scheduled_t = std::map<std::uint64_t, std::unique_ptr<step_t>>;

  std::size_t
  position (index::row_t const *row) const
  {
    return static_cast<std::size_t> (row - table_->rows ().data ());
  }

  // Waits one interval of the stream (the lock is held).
  void
  arm_stream (std::size_t at, stream_t &stream)
  {
    std::weak_ptr<simulator_core> const weak = shared_from_this ();
    std::uint64_t const id = stream.id;
    stream.timer.async_wait_for (stream.interval,
                                 [weak, at, id] (std::error_code error)
                                   {
                                     if (error)
                                       return;
                                     std::shared_ptr<simulator_core> const self
                                         = weak.lock ();
                                     if (self)
                                       self->tick (at, id);
                                   });
  }

  // The interval of a stream passed (on the strand).
  void
  tick (std::size_t at, std::uint64_t id)
  {
    std::shared_ptr<answer_fn_t const> make;
    std::shared_ptr<behavior_applier> applier;
    std::int64_t state = 0;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.load ())
        return;
      streams_t::iterator const found = streams_.find (at);
      if (found == streams_.end () || !found->second
          || found->second->id != id)
        return;
      // Wait again first: a generator that throws does not end the stream.
      arm_stream (at, *found->second);
      make = found->second->make;
      applier = applier_;
      state = state_;
    }
    if (!make)
      return;
    // The session may have changed since the stream started: a row outside
    // it is not on the device, and sends nothing while it is outside.
    if (!session_.available (*table_->rows ().data ()[at].entry))
      return;
    request_call_t call;
    field::decode_context_t const context = session_.decode_context ();
    call.context = &context;
    call.session = &session_;
    call.state = state;
    call.store = &store_;
    bytes_t const no_request;
    deliver (*applier, no_request, true, call, *make, send_kind::stream);
  }

  // --- the scenario (on the strand)

  // A playback that is still on the shutdown is cancelled (the lock is held).
  void
  cancel_playback () LUMEX_NOEXCEPT
  {
    if (play_status_.state != playback_state::playing)
      return;
    play_status_.state = playback_state::failed;
    play_status_.error = std::make_error_code (std::errc::operation_canceled);
  }

  // The playback `id` when it is still on (0: the current one), else null.
  std::shared_ptr<play_t>
  running_play (std::uint64_t id)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (dead_.load () || play_status_.state != playback_state::playing
        || !play_ || (id != 0 && play_->id != id))
      return std::shared_ptr<play_t> ();
    return play_;
  }

  // The first steps of a playback came due.
  void
  play_advance (std::uint64_t id)
  {
    std::shared_ptr<play_t> const run = running_play (id);
    if (run)
      run_script (run);
  }

  // The script is at a `send` and a request of the row came in, was
  // processed and not refused: it goes on.
  void
  play_request (index::row_t const &row, request_call_t const &call)
  {
    std::shared_ptr<play_t> const run = running_play (0);
    if (!run || run->over || run->step >= run->scenario.steps.size ())
      return;
    dsl::scenario_step_t const &step = run->scenario.steps[run->step];
    if (step.kind != dsl::scenario_step_kind::send || step.row == nullptr
        || std::strcmp (step.row, row.entry->identifier) != 0)
      return;
    if (!call.messages.empty ())
      remember (*run, &row, call.messages.back ());
    ++run->step;
    run_script (run);
  }

  void
  run_script (std::shared_ptr<play_t> const &run)
  {
    try
      {
        advance_script (*run);
      }
    catch (simulator_gone_t const &)
      {
      }
    catch (...)
      {
        end_play (*run, playback_state::failed,
                  std::make_error_code (std::errc::interrupted));
        throw;
      }
  }

  void
  end_play (play_t &run, playback_state state, std::error_code error)
  {
    run.over = true;
    std::lock_guard<std::mutex> const lock (mutex_);
    if (!play_ || play_->id != run.id)
      return;
    play_status_.state = state;
    play_status_.step = run.step;
    play_status_.error = error;
  }

  // Runs the steps that do not wait, up to the next `send` or the end.
  void
  advance_script (play_t &run)
  {
    std::size_t loops = 0;
    for (;;)
      {
        if (dead_.load () || run.over)
          return;
        if (run.step >= run.scenario.steps.size ())
          {
            end_play (run, playback_state::finished, std::error_code ());
            return;
          }
        dsl::scenario_step_t const &step = run.scenario.steps[run.step];
        switch (step.kind)
          {
          case dsl::scenario_step_kind::send:
            {
              std::lock_guard<std::mutex> const lock (mutex_);
              if (play_ && play_->id == run.id)
                play_status_.step = run.step;
              return;
            }
          case dsl::scenario_step_kind::expect:
            send_scripted (run, step.row, false);
            ++run.step;
            break;
          case dsl::scenario_step_kind::wait_state:
            set_state (step.state);
            send_scripted (run, run.state_row, true);
            ++run.step;
            break;
          case dsl::scenario_step_kind::branch:
            if (!branch_step (run, step, loops))
              return;
            break;
          default:
            ++run.step;
            break;
          }
      }
  }

  // Decides a branch as the client does, from the last message the device
  // sent for the row; false when the playback ended.
  bool
  branch_step (play_t &run, dsl::scenario_step_t const &step,
               std::size_t &loops)
  {
    namespace perror = ::lumex::core::protocol::error;
    bool taken = true;
    if (step.decide)
      {
        index::row_t const *const row
            = step.row == nullptr ? nullptr : find_row (step.row);
        bytes_t const *last = nullptr;
        for (std::size_t i = 0; i < run.last.size (); ++i)
          if (run.last[i].first == row)
            last = &run.last[i].second;
        if (last == nullptr)
          {
            end_play (run, playback_state::failed,
                      perror::make_error_code (
                          perror::protocol_errc::unexpected_reply));
            return false;
          }
        field::decode_context_t const context = session_.decode_context ();
        std::error_code decided;
        taken = false;
        try
          {
            decided = step.decide (
                layout::bytes_view_t (last->data (), last->size ()), context,
                taken);
          }
        catch (...)
          {
            {
              std::lock_guard<std::mutex> const lock (mutex_);
              ++stats_.handler_errors;
            }
            end_play (run, playback_state::failed,
                      std::make_error_code (std::errc::interrupted));
            throw;
          }
        if (dead_.load ())
          return false;
        if (decided)
          {
            end_play (run, playback_state::failed, decided);
            return false;
          }
      }
    std::size_t const target
        = taken ? run.scenario.find_label (step.label) : run.step + 1;
    if (taken && target <= run.step && ++loops > kmax_idle_loops)
      {
        end_play (
            run, playback_state::failed,
            std::make_error_code (std::errc::resource_deadlock_would_occur));
        return false;
      }
    run.step = target;
    return true;
  }

  // Remembers `payload` as the last message of `row` and of the row it
  // matches (the way the client keeps a reply under both).
  void
  remember (play_t &run, index::row_t const *row, bytes_t const &payload)
  {
    index::row_t const *const matched
        = table_
              ->match (layout::bytes_view_t (payload.data (), payload.size ()))
              .row;
    index::row_t const *const rows[2] = { row, matched };
    for (std::size_t n = 0; n < 2; ++n)
      {
        if (rows[n] == nullptr || (n == 1 && rows[1] == rows[0]))
          continue;
        bool found = false;
        for (std::size_t i = 0; i < run.last.size (); ++i)
          if (run.last[i].first == rows[n])
            {
              run.last[i].second = payload;
              found = true;
            }
        if (!found)
          run.last.push_back (std::make_pair (rows[n], payload));
      }
  }

  // The device sends a message of the row by itself: the configured one, or
  // the sample (`sample_only`: the state shown with the state set).
  void
  send_scripted (play_t &run, char const *identifier, bool sample_only)
  {
    index::row_t const *const row
        = identifier == nullptr ? nullptr : find_row (identifier);
    if (row == nullptr)
      return;
    std::size_t const at = position (row);
    row_ops_t const *const ops = ops_[at];
    if (ops == nullptr || ops->message == nullptr
        || !session_.available (*row->entry))
      return;
    std::shared_ptr<answer_fn_t const> configured;
    std::shared_ptr<behavior_applier> applier;
    std::int64_t state = 0;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.load ())
        return;
      if (!sample_only)
        configured = messages_[at];
      applier = applier_;
      state = state_;
    }
    request_call_t call;
    field::decode_context_t const context = session_.decode_context ();
    call.context = &context;
    call.session = &session_;
    call.state = state;
    call.store = &store_;
    answer_fn_t const sample (ops->message);
    answer_fn_t const &make = configured ? *configured : sample;
    bytes_t const no_request;
    deliver (*applier, no_request, true, call, make, send_kind::scripted);
    if (!call.messages.empty () && !call.encode_failed)
      remember (run, row, call.messages.back ());
  }

  // A scheduled step came due (on the strand).
  void
  run_step (std::uint64_t id)
  {
    std::function<void ()> action;
    std::unique_ptr<step_t> done;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.load ())
        return;
      scheduled_t::iterator const found = scheduled_.find (id);
      if (found == scheduled_.end ())
        return;
      done.swap (found->second);
      scheduled_.erase (found);
      action = std::move (done->action);
    }
    try
      {
        action ();
      }
    catch (...)
      {
        {
          std::lock_guard<std::mutex> const lock (mutex_);
          ++stats_.handler_errors;
        }
        throw;
      }
  }

  // Serves the frames of one chunk; a handler that throws does not take the
  // other frames with it: the first exception goes on at the end.
  void
  serve_all (std::vector<bytes_t> const &frames)
  {
    std::exception_ptr first;
    for (std::size_t i = 0; i < frames.size () && !dead_.load (); ++i)
      {
        try
          {
            serve (frames[i]);
          }
        catch (simulator_gone_t const &)
          {
            return;
          }
        catch (...)
          {
            if (!first)
              first = std::current_exception ();
          }
      }
    if (first)
      std::rethrow_exception (first);
  }

  // One request frame.
  void
  serve (bytes_t const &frame)
  {
    layout::bytes_view_t const view (frame.data (), frame.size ());
    index::frame_match_t const found = table_->match (view);
    std::size_t at = 0;
    row_ops_t const *ops = nullptr;
    if (found.row != nullptr)
      {
        at = position (found.row);
        ops = ops_[at];
      }
    bytes_t request_wire;
    if (!wire_->encode (frame, request_wire))
      request_wire = frame;
    std::shared_ptr<answer_fn_t const> answer;
    std::shared_ptr<behavior_applier> applier;
    std::int64_t state = 0;
    bool refuse = false;
    bool known = ops != nullptr && session_.available (*found.row->entry);
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (dead_.load ())
        return;
      applier = applier_;
      state = state_;
      if (known)
        {
          ++stats_.requests;
          ++row_requests_[at];
          answer = answers_[at];
          refuse = refusing_[at] != 0;
        }
      else
        ++stats_.unknown_requests;
    }
    request_call_t call;
    field::decode_context_t const context = session_.decode_context ();
    call.context = &context;
    call.session = &session_;
    call.state = state;
    call.store = &store_;
    call.message = view;
    if (!known)
      {
        // No such row: nothing, or the frame back.
        deliver (*applier, request_wire, options_.answer_unknown, call,
                 answer_fn_t (), send_kind::request);
        return;
      }
    dsl::states_t const accepted = found.row->entry->options.accepted_states;
    if (accepted.size () != 0)
      {
        bool listed = false;
        for (std::size_t i = 0; i < accepted.size (); ++i)
          if (accepted.data ()[i] == state)
            listed = true;
        if (!listed)
          refuse = true;
      }
    call.refuse = refuse;
    row_ops_t const *const chosen = ops;
    // Called through the reference: a function with a state of its own keeps
    // it from one request to the next (`answer` keeps it alive meanwhile).
    answer_fn_t const standard (chosen->answer);
    answer_fn_t const &run = answer ? *answer : standard;
    bool const processed = deliver (*applier, request_wire,
                                    chosen->replies || options_.answer_unknown,
                                    call, run, send_kind::request);
    // A scripted `send` goes on for a request the device processed and did
    // not refuse.
    if (processed && !call.refused && !call.malformed && !call.encode_failed)
      play_request (*found.row, call);
  }

  // Lets the behaviour decide, makes the reply of a request the device
  // processes, sends it and what follows; the counters.
  bool
  deliver (behavior_applier &applier, bytes_t const &request_wire,
           bool has_reply, request_call_t &call, answer_fn_t const &run,
           send_kind kind)
  {
    std::shared_ptr<target_t> const mock = target ();
    if (!mock)
      return false;
    bool made_first = false;
    bool echo_back = false;
    reply_maker_t const make = [&] () -> bytes_t
      {
        if (run)
          run (call);
        if (dead_.load ())
          throw simulator_gone_t ();
        if (!run || call.malformed)
          {
            // No row for the frame, or a request that does not decode.
            echo_back = options_.answer_unknown;
            return echo_back ? request_wire : bytes_t ();
          }
        if (call.encode_failed || call.messages.empty ())
          return bytes_t ();
        bytes_t wire;
        if (!wire_->encode (call.messages[0], wire))
          {
            call.encode_failed = true;
            return bytes_t ();
          }
        made_first = true;
        return wire;
      };
    bool processed = false;
    bool reached = false;
    try
      {
        processed
            = mock->respond (applier, request_wire, has_reply, make, &reached);
      }
    catch (simulator_gone_t const &)
      {
        // Destroyed inside the code that makes the reply: nothing is sent.
        return false;
      }
    catch (...)
      {
        {
          std::lock_guard<std::mutex> const lock (mutex_);
          ++stats_.handler_errors;
        }
        throw;
      }
    // Messages that reach the mock: a reply the behaviour dropped does not.
    std::size_t sent = ((made_first || echo_back) && reached) ? 1u : 0u;
    if (processed && made_first)
      for (std::size_t i = 1; i < call.messages.size () && !dead_.load (); ++i)
        {
          bytes_t wire;
          if (!wire_->encode (call.messages[i], wire))
            {
              call.encode_failed = true;
              continue;
            }
          // A further message meets the behaviour too (a frozen device
          // acknowledged and said no more).
          bytes_t const further = wire;
          bool further_reached = false;
          if (mock->respond (
                  applier, request_wire, true,
                  [&further] () { return further; }, &further_reached)
              && further_reached)
            ++sent;
        }
    std::lock_guard<std::mutex> const lock (mutex_);
    if (!processed)
      {
        // A request the device did not process; a stream message it did not
        // send is no request and is not counted.
        if (kind == send_kind::request)
          ++stats_.not_processed;
        return false;
      }
    if (kind == send_kind::request)
      {
        stats_.replies += sent;
        stats_.registry_writes += call.kept;
        if (call.refused)
          ++stats_.refused;
        if (call.malformed)
          ++stats_.malformed_requests;
      }
    else if (kind == send_kind::scripted)
      stats_.scripted_messages += sent;
    else
      stats_.stream_messages += sent;
    if (call.encode_failed)
      ++stats_.encode_errors;
    return true;
  }

  index::protocol_index_t const *table_;
  std::vector<row_ops_t const *> ops_;
  std::unique_ptr<simulator_wire_t> wire_;
  std::shared_ptr<target_t> target_;
  timing::timer_source timers_;
  simulator_options_t options_;
  std::uint8_t report_id_;
  index::session_context_t session_;
  registry_store_t store_;

  mutable std::mutex mutex_;
  std::atomic<bool> dead_;
  std::int64_t state_;
  std::uint64_t next_id_;
  answers_t answers_;
  // The message of each row for a scenario (the value of `set_reply`).
  answers_t messages_;
  std::vector<char> refusing_;
  std::vector<std::uint64_t> row_requests_;
  std::shared_ptr<behavior_applier> applier_;
  streams_t streams_;
  scheduled_t scheduled_;
  simulator_statistics_t stats_;
  std::shared_ptr<play_t> play_;
  playback_status_t play_status_;
};

// The device on a mock stream.
class stream_target_t final : public target_t
{
public:
  explicit stream_target_t (mock_stream_control control)
      : control_ (std::move (control))
  {
  }

  strand_t
  strand () const override
  {
    return control_.get_strand ();
  }

  void
  observe (std::weak_ptr<simulator_core> const &core) override
  {
    // The mock keeps the observer: it holds the simulator weakly.
    control_.on_write (
        [core] (bytes_t const &chunk)
          {
            std::shared_ptr<simulator_core> const self = core.lock ();
            if (self)
              self->take_chunk (chunk);
          });
  }

  void
  unobserve () override
  {
    control_.on_write (write_observer_t ());
  }

  bool
  respond (behavior_applier &applier, bytes_t const &request, bool has_reply,
           reply_maker_t const &make, bool *reached) override
  {
    return applier.respond (control_, request, has_reply, make, reached);
  }

  void
  attach (behavior_applier &applier) override
  {
    applier.attach (control_);
  }

private:
  mock_stream_control control_;
};

// The device on a mock datagram transport.
class datagram_target_t final : public target_t
{
public:
  datagram_target_t (mock_datagram_control control, std::uint8_t report_id)
      : control_ (std::move (control)), report_id_ (report_id)
  {
  }

  strand_t
  strand () const override
  {
    return control_.get_strand ();
  }

  void
  observe (std::weak_ptr<simulator_core> const &core) override
  {
    control_.on_send (
        [core] (datagram::message_t const &message)
          {
            std::shared_ptr<simulator_core> const self = core.lock ();
            if (self)
              self->take_datagram (message);
          });
  }

  void
  unobserve () override
  {
    control_.on_send (send_observer_t ());
  }

  bool
  respond (behavior_applier &applier, bytes_t const &request, bool has_reply,
           reply_maker_t const &make, bool *reached) override
  {
    return applier.respond (control_, report_id_, request, has_reply, make,
                            reached);
  }

  void
  attach (behavior_applier &applier) override
  {
    applier.attach (control_);
  }

private:
  mock_datagram_control control_;
  std::uint8_t report_id_;
};
} // namespace

std::shared_ptr<simulator_api>
make_stream_simulator (index::protocol_index_t const *table,
                       row_ops_t const *ops, std::size_t op_count,
                       std::unique_ptr<simulator_wire_t> wire,
                       mock_stream_control control, virtual_clock &clock,
                       simulator_options_t const &options)
{
  std::shared_ptr<simulator_core> const made
      = std::make_shared<simulator_core> (
          table, ops, op_count, std::move (wire),
          std::unique_ptr<target_t> (
              new stream_target_t (std::move (control))),
          clock, options, 0);
  made->start ();
  return made;
}

std::shared_ptr<simulator_api>
make_datagram_simulator (index::protocol_index_t const *table,
                         row_ops_t const *ops, std::size_t op_count,
                         std::unique_ptr<simulator_wire_t> wire,
                         mock_datagram_control control, std::uint8_t report_id,
                         virtual_clock &clock,
                         simulator_options_t const &options)
{
  std::shared_ptr<simulator_core> const made
      = std::make_shared<simulator_core> (
          table, ops, op_count, std::move (wire),
          std::unique_ptr<target_t> (
              new datagram_target_t (std::move (control), report_id)),
          clock, options, report_id);
  made->start ();
  return made;
}
} // namespace simulator_detail
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
