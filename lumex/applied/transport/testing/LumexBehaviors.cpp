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
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/testing/LumexBehaviors.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wlifetime-safety-invalidation")
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#if __has_warning("-Wlifetime-safety-cross-tu-suggestions")
#pragma clang diagnostic ignored "-Wlifetime-safety-cross-tu-suggestions"
#endif
#if __has_warning("-Wlifetime-safety-suggestions")
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
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
namespace
{
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using steps_t = std::vector<plan_step_t>;

// A duration in milliseconds, never negative.
long long
millis (ms value)
{
  return std::max<long long> (static_cast<long long> (value.count ()), 0);
}

// --- the generator

// splitmix64: the same numbers on every platform and standard library.
std::uint64_t
next_random (std::uint64_t &state)
{
  state += 0x9E3779B97F4A7C15ULL;
  std::uint64_t value = state;
  value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31);
}

// A number in [0, bound); 0 for a bound of 0.
std::size_t
below (std::uint64_t &state, std::size_t bound)
{
  std::uint64_t const value = next_random (state);
  if (bound == 0)
    return 0;
  return static_cast<std::size_t> (value % static_cast<std::uint64_t> (bound));
}
} // namespace

// What a factory of `behavior` was given. The meaning of the fields is per
// kind and is set out where the stage is applied.
struct behavior_stage_t
{
  behavior_kind kind = behavior_kind::perfect;
  std::uint32_t seed = 0;
  std::size_t count = 0;
  std::size_t limit = 0;
  ms period = ms (0);
  std::uint8_t byte = 0;
  malicious_mode mode = malicious_mode::mixed;
  std::error_code error;
};

// The state of one stage in an applier.
struct behavior_state_t
{
  std::uint64_t generator = 0;
  std::size_t count = 0;
  bool spent = false;
  timing::time_point_t until;
};

// The stages of a behaviour (shared by its copies, never changed).
class behavior_impl
{
public:
  std::vector<behavior_stage_t> stages;
};

// The state of an applier: one entry per stage.
class behavior_applier_state
{
public:
  std::vector<behavior_state_t> states;
};

namespace
{
std::shared_ptr<behavior_impl const>
one_stage (behavior_stage_t const &stage)
{
  std::shared_ptr<behavior_impl> made = std::make_shared<behavior_impl> ();
  made->stages.push_back (stage);
  return made;
}

// --- steps

plan_step_t
make_step (plan_step_kind kind, ms at, bytes_t const &bytes, bool from_reply)
{
  plan_step_t step;
  step.kind = kind;
  step.at = at;
  step.bytes = bytes;
  step.from_reply = from_reply;
  return step;
}

// What a stage sees besides the steps.
struct plan_context_t
{
  bytes_t const *request;
  // The row has a reply (the request was not for a command without one).
  bool has_reply;
  timing::time_point_t now;
  bool datagram;
};

bool
is_reply_bytes (plan_step_t const &step)
{
  return step.kind == plan_step_kind::bytes && step.from_reply;
}

bool
has_reply_bytes (steps_t const &steps)
{
  for (std::size_t index = 0; index < steps.size (); ++index)
    if (is_reply_bytes (steps[index]))
      return true;
  return false;
}

std::size_t
total_reply_bytes (steps_t const &steps)
{
  std::size_t total = 0;
  for (std::size_t index = 0; index < steps.size (); ++index)
    if (is_reply_bytes (steps[index]))
      total += steps[index].bytes.size ();
  return total;
}

ms
last_bytes_time (steps_t const &steps)
{
  ms latest (0);
  for (std::size_t index = 0; index < steps.size (); ++index)
    if (steps[index].kind == plan_step_kind::bytes && steps[index].at > latest)
      latest = steps[index].at;
  return latest;
}

// Keeps the first `keep` bytes of the reply (steps of the reply that lose all
// their bytes go; the other steps stay).
void
keep_first_reply_bytes (steps_t &steps, std::size_t keep)
{
  steps_t kept;
  std::size_t remaining = keep;
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      plan_step_t step = steps[index];
      if (is_reply_bytes (step))
        {
          if (remaining == 0)
            continue;
          if (step.bytes.size () > remaining)
            step.bytes.resize (remaining);
          remaining -= step.bytes.size ();
        }
      kept.push_back (step);
    }
  steps.swap (kept);
}

void
put_first (steps_t &steps, plan_step_t const &step)
{
  steps.insert (steps.begin (), step);
}

// Puts the steps in the order of a line. A stream plan is one line: no step
// happens before the one in front of it. A datagram plan keeps the times of
// its messages (they may overlap), but a step that is no message does not
// happen before a message in front of it.
void
settle (steps_t &steps, bool datagram)
{
  ms floor (0);
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      plan_step_t &step = steps[index];
      if (step.at < ms (0))
        step.at = ms (0);
      if (datagram && step.kind == plan_step_kind::bytes)
        {
          floor = std::max (floor, step.at);
          continue;
        }
      if (step.at < floor)
        step.at = floor;
      if (!datagram)
        floor = step.at;
    }
}

// --- the attack patterns

bytes_t
attack_unit (malicious_mode mode)
{
  bytes_t unit;
  switch (mode)
    {
    case malicious_mode::end_flood:
      unit.push_back (0xC0);
      break;
    case malicious_mode::nul_flood:
      unit.push_back (0x00);
      break;
    case malicious_mode::ff_flood:
      unit.push_back (0xFF);
      break;
    case malicious_mode::control_chars:
      for (std::uint8_t value = 0; value < 0x20; ++value)
        unit.push_back (value);
      break;
    case malicious_mode::alternating_extremes:
      unit.push_back (0x00);
      unit.push_back (0xFF);
      break;
    case malicious_mode::bad_escape:
      {
        std::uint8_t const pattern[] = { 0xC0, 0xDB, 0xAA, 0xC0, 0xDB, 0xC0 };
        unit.assign (pattern, pattern + sizeof (pattern));
      }
      break;
    case malicious_mode::long_unterminated:
      unit.push_back (0xAB);
      break;
    case malicious_mode::max_length_prefix:
      {
        std::uint8_t const pattern[]
            = { 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF };
        unit.assign (pattern, pattern + sizeof (pattern));
      }
      break;
    case malicious_mode::dense_escapes:
      unit.push_back (0xDB);
      unit.push_back (0xDC);
      break;
    case malicious_mode::mixed:
      unit.push_back (0x00);
      break;
    default:
      break;
    }
  return unit;
}

bytes_t
attack_bytes (malicious_mode mode, std::size_t count)
{
  bytes_t const unit = attack_unit (mode);
  bytes_t bytes;
  bytes.reserve (count);
  for (std::size_t index = 0; index < count; ++index)
    bytes.push_back (unit[index % unit.size ()]);
  return bytes;
}

// --- the stages

// echo: `period` is the delay. The echoed bytes are no reply.
void
apply_echo (behavior_stage_t const &stage, steps_t &steps, bool &processed,
            plan_context_t const &context)
{
  steps.clear ();
  processed = false;
  if (!context.request->empty ())
    steps.push_back (make_step (plan_step_kind::bytes, stage.period,
                                *context.request, false));
}

// frozen: `count` is the number of replies answered.
void
apply_frozen (behavior_stage_t const &stage, behavior_state_t &state,
              steps_t &steps, bool &processed, plan_context_t const &context)
{
  if (state.count >= stage.count)
    {
      steps.clear ();
      processed = false;
      return;
    }
  if (context.has_reply)
    ++state.count;
}

// partial_then_frozen: `count` is the number of replies answered whole,
// `limit` the bytes of the next one.
void
apply_partial_then_frozen (behavior_stage_t const &stage,
                           behavior_state_t &state, steps_t &steps,
                           bool &processed, plan_context_t const &context)
{
  if (state.count > stage.count)
    {
      steps.clear ();
      processed = false;
      return;
    }
  if (!context.has_reply)
    return;
  if (state.count == stage.count)
    {
      std::size_t const total = total_reply_bytes (steps);
      keep_first_reply_bytes (
          steps, std::min (stage.limit, total > 0 ? total - 1 : 0));
    }
  ++state.count;
}

// slow_trickle: `period` is the time per byte. A stream is one line, so the
// bytes of all the steps follow each other; a datagram is delayed whole.
void
apply_slow_trickle (behavior_stage_t const &stage, steps_t &steps,
                    plan_context_t const &context)
{
  long long const per_byte = millis (stage.period);
  long long cursor = 0;
  steps_t out;
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      plan_step_t const &step = steps[index];
      if (step.kind != plan_step_kind::bytes)
        {
          out.push_back (step);
          continue;
        }
      if (context.datagram)
        {
          plan_step_t late = step;
          late.at
              += ms (per_byte * static_cast<long long> (step.bytes.size ()));
          out.push_back (late);
          continue;
        }
      for (std::size_t offset = 0; offset < step.bytes.size (); ++offset)
        {
          cursor = std::max (cursor, static_cast<long long> (step.at.count ()))
                   + per_byte;
          out.push_back (make_step (plan_step_kind::bytes, ms (cursor),
                                    bytes_t (1, step.bytes[offset]),
                                    step.from_reply));
        }
    }
  steps.swap (out);
}

// fragmented: `count` is the largest piece, `period` the gap. On a stream
// the pieces of all the steps are one line, `gap` apart; datagrams are
// spaced within their own step.
void
apply_fragmented (behavior_stage_t const &stage, behavior_state_t &state,
                  steps_t &steps, plan_context_t const &context)
{
  std::size_t const largest = std::max<std::size_t> (stage.count, 1);
  long long const gap = millis (stage.period);
  long long previous = -1;
  steps_t out;
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      plan_step_t const &step = steps[index];
      if (step.kind != plan_step_kind::bytes)
        {
          out.push_back (step);
          continue;
        }
      std::size_t offset = 0;
      long long piece = 0;
      while (offset < step.bytes.size ())
        {
          std::size_t size = 1 + below (state.generator, largest);
          size = std::min (size, step.bytes.size () - offset);
          bytes_t const part (
              step.bytes.begin () + static_cast<std::ptrdiff_t> (offset),
              step.bytes.begin ()
                  + static_cast<std::ptrdiff_t> (offset + size));
          long long at = static_cast<long long> (step.at.count ());
          if (context.datagram)
            at += gap * piece;
          else if (previous >= 0)
            at = std::max (at, previous + gap);
          previous = at;
          out.push_back (make_step (plan_step_kind::bytes, ms (at), part,
                                    step.from_reply));
          offset += size;
          ++piece;
        }
    }
  steps.swap (out);
}

// flood: `count` bytes of the counting pattern.
void
apply_flood (behavior_stage_t const &stage, steps_t &steps)
{
  if (stage.count == 0)
    return;
  bytes_t pattern (stage.count);
  for (std::size_t index = 0; index < pattern.size (); ++index)
    pattern[index] = static_cast<std::uint8_t> (index & 0xFFu);
  put_first (steps, make_step (plan_step_kind::bytes, ms (0), pattern, false));
}

// repeating: `count` copies of the reply, `period` the gap between their
// starts. A copy of a stream does not start before the one in front of it
// ended.
void
apply_repeating (behavior_stage_t const &stage, steps_t &steps,
                 plan_context_t const &context)
{
  if (!context.has_reply || !has_reply_bytes (steps))
    return;
  std::size_t first = steps.size ();
  std::size_t last = 0;
  for (std::size_t index = 0; index < steps.size (); ++index)
    if (is_reply_bytes (steps[index]))
      {
        if (first == steps.size ())
          first = index;
        last = index;
      }
  steps_t group;
  for (std::size_t index = first; index <= last; ++index)
    if (is_reply_bytes (steps[index]))
      group.push_back (steps[index]);
  long long const begin = static_cast<long long> (group.front ().at.count ());
  long long previous_end = static_cast<long long> (group.back ().at.count ());
  steps_t copies;
  for (std::size_t copy = 1; copy <= stage.count; ++copy)
    {
      long long start
          = begin + millis (stage.period) * static_cast<long long> (copy);
      if (!context.datagram)
        start = std::max (start, previous_end);
      for (std::size_t index = 0; index < group.size (); ++index)
        {
          plan_step_t again = group[index];
          again.at = ms (
              start
              + (static_cast<long long> (group[index].at.count ()) - begin));
          previous_end = static_cast<long long> (again.at.count ());
          copies.push_back (again);
        }
    }
  steps.insert (steps.begin () + static_cast<std::ptrdiff_t> (last + 1),
                copies.begin (), copies.end ());
}

// garbage: `count` random bytes.
void
apply_garbage (behavior_stage_t const &stage, behavior_state_t &state,
               steps_t &steps)
{
  if (stage.count == 0)
    return;
  bytes_t noise (stage.count);
  for (std::size_t index = 0; index < noise.size (); ++index)
    noise[index]
        = static_cast<std::uint8_t> (next_random (state.generator) >> 56);
  put_first (steps, make_step (plan_step_kind::bytes, ms (0), noise, false));
}

// malicious: `mode`, `count` bytes.
void
apply_malicious (behavior_stage_t const &stage, behavior_state_t &state,
                 steps_t &steps)
{
  if (stage.count == 0)
    return;
  malicious_mode mode = stage.mode;
  if (mode == malicious_mode::mixed)
    mode = static_cast<malicious_mode> (below (state.generator, 9));
  put_first (steps, make_step (plan_step_kind::bytes, ms (0),
                               attack_bytes (mode, stage.count), false));
}

// corrupted_crc: `count` is the rate in thousandths, `limit` the number of
// frames to damage (0: any). Only the bytes of the reply are damaged.
void
apply_corrupted_crc (behavior_stage_t const &stage, behavior_state_t &state,
                     steps_t &steps, plan_context_t const &context)
{
  if (!context.has_reply || !has_reply_bytes (steps))
    return;
  if (stage.limit != 0 && state.count >= stage.limit)
    return;
  if (below (state.generator, 1000) >= stage.count)
    return;
  std::size_t const total = total_reply_bytes (steps);
  if (total == 0)
    return;
  std::size_t const low = total >= 3 ? 1 : 0;
  std::size_t const high = total >= 3 ? total - 2 : total - 1;
  std::size_t position = low + below (state.generator, high - low + 1);
  std::uint8_t const mask
      = static_cast<std::uint8_t> (1 + below (state.generator, 255));
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      if (!is_reply_bytes (steps[index]))
        continue;
      bytes_t &bytes = steps[index].bytes;
      if (position < bytes.size ())
        {
          bytes[position] = static_cast<std::uint8_t> (bytes[position] ^ mask);
          break;
        }
      position -= bytes.size ();
    }
  ++state.count;
}

// oversized: `count` bytes of `byte` in place of the reply.
void
apply_oversized (behavior_stage_t const &stage, steps_t &steps,
                 plan_context_t const &context)
{
  if (!context.has_reply || !has_reply_bytes (steps))
    return;
  steps_t out;
  bool placed = false;
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      if (!is_reply_bytes (steps[index]))
        {
          out.push_back (steps[index]);
          continue;
        }
      if (!placed)
        {
          out.push_back (make_step (plan_step_kind::bytes, steps[index].at,
                                    bytes_t (stage.count, stage.byte), true));
          placed = true;
        }
    }
  steps.swap (out);
}

// zero_read: `count` empty reads before each piece of data (stream only).
void
apply_zero_read (behavior_stage_t const &stage, steps_t &steps,
                 plan_context_t const &context)
{
  if (context.datagram)
    return;
  steps_t out;
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      if (steps[index].kind == plan_step_kind::bytes)
        for (std::size_t empty = 0; empty < stage.count; ++empty)
          out.push_back (make_step (plan_step_kind::empty_read,
                                    steps[index].at, bytes_t (), false));
      out.push_back (steps[index]);
    }
  steps.swap (out);
}

// intermittent: `count` is the loss rate in thousandths, `limit` the number
// of outages (0: any), `period` the length of one.
void
apply_intermittent (behavior_stage_t const &stage, behavior_state_t &state,
                    steps_t &steps, plan_context_t const &context)
{
  if (!context.has_reply)
    return;
  if (context.now < state.until)
    {
      steps.clear ();
      return;
    }
  if (stage.limit != 0 && state.count >= stage.limit)
    return;
  if (below (state.generator, 1000) >= stage.count)
    return;
  ++state.count;
  state.until = ::lumex::applied::transport::detail::saturating_add (
      context.now, stage.period);
  steps.clear ();
}

// disconnecting: `count` is the byte budget over all replies; state.count is
// what was delivered. The bytes of the reply are counted; what else is on
// the line in front of the cut stays, what is behind it goes.
void
apply_disconnecting (behavior_stage_t const &stage, behavior_state_t &state,
                     steps_t &steps, plan_context_t const &context)
{
  if (state.spent || !context.has_reply)
    return;
  std::size_t remaining = stage.count - std::min (stage.count, state.count);
  steps_t kept;
  bool fired = false;
  ms last (0);
  for (std::size_t index = 0; index < steps.size (); ++index)
    {
      plan_step_t step = steps[index];
      if (fired)
        continue;
      if (!is_reply_bytes (step))
        {
          kept.push_back (step);
          continue;
        }
      last = step.at;
      if (step.bytes.size () <= remaining)
        {
          remaining -= step.bytes.size ();
          kept.push_back (step);
          fired = remaining == 0;
          continue;
        }
      fired = true;
      if (remaining > 0)
        {
          step.bytes.resize (remaining);
          kept.push_back (step);
        }
      remaining = 0;
    }
  state.count = stage.count - remaining;
  if (fired)
    {
      kept.push_back (
          make_step (plan_step_kind::disconnect, last, bytes_t (), false));
      state.spent = true;
    }
  steps.swap (kept);
}

// works_then_disconnects: `count` replies, then the hang-up.
void
apply_works_then_disconnects (behavior_stage_t const &stage,
                              behavior_state_t &state, steps_t &steps,
                              plan_context_t const &context)
{
  if (state.spent || !context.has_reply)
    return;
  if (state.count >= stage.count)
    {
      steps.clear ();
      steps.push_back (
          make_step (plan_step_kind::disconnect, ms (0), bytes_t (), false));
      state.spent = true;
      return;
    }
  ++state.count;
  if (state.count == stage.count)
    {
      steps.push_back (make_step (plan_step_kind::disconnect,
                                  last_bytes_time (steps), bytes_t (), false));
      state.spent = true;
    }
}

// cable_unplugged: state.until is the end of the window.
void
apply_cable_unplugged (behavior_state_t const &state, steps_t &steps,
                       bool &processed, plan_context_t const &context)
{
  if (context.now < state.until)
    {
      steps.clear ();
      processed = false;
    }
}

void
apply_stage (behavior_stage_t const &stage, behavior_state_t &state,
             steps_t &steps, bool &processed, plan_context_t const &context)
{
  switch (stage.kind)
    {
    case behavior_kind::perfect:
      break;
    case behavior_kind::echo:
      apply_echo (stage, steps, processed, context);
      break;
    case behavior_kind::silent:
      steps.clear ();
      processed = false;
      break;
    case behavior_kind::frozen:
      apply_frozen (stage, state, steps, processed, context);
      break;
    case behavior_kind::partial_then_frozen:
      apply_partial_then_frozen (stage, state, steps, processed, context);
      break;
    case behavior_kind::slow_trickle:
      apply_slow_trickle (stage, steps, context);
      break;
    case behavior_kind::fragmented:
      apply_fragmented (stage, state, steps, context);
      break;
    case behavior_kind::flood:
      apply_flood (stage, steps);
      break;
    case behavior_kind::repeating:
      apply_repeating (stage, steps, context);
      break;
    case behavior_kind::garbage:
      apply_garbage (stage, state, steps);
      break;
    case behavior_kind::corrupted_crc:
      apply_corrupted_crc (stage, state, steps, context);
      break;
    case behavior_kind::oversized:
      apply_oversized (stage, steps, context);
      break;
    case behavior_kind::zero_read:
      apply_zero_read (stage, steps, context);
      break;
    case behavior_kind::intermittent:
      apply_intermittent (stage, state, steps, context);
      break;
    case behavior_kind::disconnecting:
      apply_disconnecting (stage, state, steps, context);
      break;
    case behavior_kind::works_then_disconnects:
      apply_works_then_disconnects (stage, state, steps, context);
      break;
    case behavior_kind::cable_unplugged:
      apply_cable_unplugged (state, steps, processed, context);
      break;
    case behavior_kind::malicious:
      apply_malicious (stage, state, steps);
      break;
    default:
      break;
    }
}

bool
by_time (plan_step_t const &left, plan_step_t const &right)
{
  return left.at < right.at;
}

// --- carrying a plan to a mock

void
carry_out (reply_plan_t const &plan, mock_stream_control &control)
{
  for (std::size_t index = 0; index < plan.steps.size (); ++index)
    {
      plan_step_t const &step = plan.steps[index];
      bool const now = step.at <= ms (0);
      switch (step.kind)
        {
        case plan_step_kind::bytes:
          if (now)
            control.feed (step.bytes);
          else
            control.feed_after_on_link (step.at, step.bytes);
          break;
        case plan_step_kind::empty_read:
          if (now)
            control.complete_next_read_empty ();
          else
            control.empty_read_after (step.at);
          break;
        case plan_step_kind::disconnect:
          if (now)
            control.disconnect ();
          else
            control.disconnect_after (step.at);
          break;
        default:
          break;
        }
    }
}

void
carry_out (reply_plan_t const &plan, mock_datagram_control &control,
           std::uint8_t report_id)
{
  for (std::size_t index = 0; index < plan.steps.size (); ++index)
    {
      plan_step_t const &step = plan.steps[index];
      bool const now = step.at <= ms (0);
      switch (step.kind)
        {
        case plan_step_kind::bytes:
          {
            datagram::message_t message;
            message.report_id = report_id;
            message.payload = step.bytes;
            if (now)
              control.deliver (message);
            else
              control.deliver_after_on_link (step.at, message);
          }
          break;
        case plan_step_kind::empty_read:
          break;
        case plan_step_kind::disconnect:
          if (now)
            control.disconnect ();
          else
            control.disconnect_after (step.at);
          break;
        default:
          break;
        }
    }
}
} // namespace

// --- names

char const *
behavior_kind_name (behavior_kind kind)
{
  switch (kind)
    {
    case behavior_kind::perfect:
      return "perfect";
    case behavior_kind::echo:
      return "echo";
    case behavior_kind::silent:
      return "silent";
    case behavior_kind::frozen:
      return "frozen";
    case behavior_kind::partial_then_frozen:
      return "partial_then_frozen";
    case behavior_kind::slow_trickle:
      return "slow_trickle";
    case behavior_kind::fragmented:
      return "fragmented";
    case behavior_kind::flood:
      return "flood";
    case behavior_kind::repeating:
      return "repeating";
    case behavior_kind::garbage:
      return "garbage";
    case behavior_kind::corrupted_crc:
      return "corrupted_crc";
    case behavior_kind::oversized:
      return "oversized";
    case behavior_kind::zero_read:
      return "zero_read";
    case behavior_kind::intermittent:
      return "intermittent";
    case behavior_kind::disconnecting:
      return "disconnecting";
    case behavior_kind::works_then_disconnects:
      return "works_then_disconnects";
    case behavior_kind::cable_unplugged:
      return "cable_unplugged";
    case behavior_kind::malicious:
      return "malicious";
    default:
      break;
    }
  return "unknown";
}

char const *
outcome_class_name (outcome_class outcome)
{
  switch (outcome)
    {
    case outcome_class::success:
      return "success";
    case outcome_class::success_after_retries:
      return "success_after_retries";
    case outcome_class::timeout:
      return "timeout";
    case outcome_class::retries_exhausted:
      return "retries_exhausted";
    case outcome_class::link_lost:
      return "link_lost";
    case outcome_class::protocol_error:
      return "protocol_error";
    case outcome_class::dropped_counted:
      return "dropped_counted";
    default:
      break;
    }
  return "unknown";
}

std::vector<behavior_kind>
all_behavior_kinds ()
{
  behavior_kind const kinds[] = {
    behavior_kind::perfect,
    behavior_kind::echo,
    behavior_kind::silent,
    behavior_kind::frozen,
    behavior_kind::partial_then_frozen,
    behavior_kind::slow_trickle,
    behavior_kind::fragmented,
    behavior_kind::flood,
    behavior_kind::repeating,
    behavior_kind::garbage,
    behavior_kind::corrupted_crc,
    behavior_kind::oversized,
    behavior_kind::zero_read,
    behavior_kind::intermittent,
    behavior_kind::disconnecting,
    behavior_kind::works_then_disconnects,
    behavior_kind::cable_unplugged,
    behavior_kind::malicious,
  };
  return std::vector<behavior_kind> (
      kinds, kinds + sizeof (kinds) / sizeof (kinds[0]));
}

// --- the expected outcome

outcome_class
expected_outcome (behavior_kind kind, row_shape_t const &shape)
{
  bool const retried = shape.retries > 0;
  outcome_class const lost
      = retried ? outcome_class::retries_exhausted : outcome_class::timeout;
  outcome_class const damaged = retried ? outcome_class::success_after_retries
                                        : outcome_class::timeout;
  outcome_class const cut = shape.reconnects && shape.idempotent
                                ? outcome_class::success_after_retries
                                : outcome_class::link_lost;
  outcome_class const resumed = shape.reconnects
                                    ? outcome_class::success_after_retries
                                    : outcome_class::link_lost;
  switch (shape.reply)
    {
    case reply_shape::none:
      switch (kind)
        {
        case behavior_kind::echo:
        case behavior_kind::flood:
        case behavior_kind::garbage:
        case behavior_kind::malicious:
          return outcome_class::dropped_counted;
        case behavior_kind::cable_unplugged:
          return cut;
        default:
          return outcome_class::success;
        }
    case reply_shape::stream:
      switch (kind)
        {
        case behavior_kind::echo:
          return outcome_class::protocol_error;
        case behavior_kind::silent:
        case behavior_kind::frozen:
        case behavior_kind::partial_then_frozen:
        case behavior_kind::oversized:
          return shape.watchdog ? outcome_class::link_lost
                                : outcome_class::timeout;
        case behavior_kind::flood:
        case behavior_kind::repeating:
        case behavior_kind::garbage:
        case behavior_kind::corrupted_crc:
        case behavior_kind::intermittent:
        case behavior_kind::malicious:
          return outcome_class::dropped_counted;
        case behavior_kind::disconnecting:
        case behavior_kind::works_then_disconnects:
        case behavior_kind::cable_unplugged:
          return resumed;
        default:
          return outcome_class::success;
        }
    case reply_shape::one:
    case reply_shape::dialogue_ack:
      switch (kind)
        {
        case behavior_kind::echo:
          return outcome_class::protocol_error;
        case behavior_kind::silent:
        case behavior_kind::frozen:
        case behavior_kind::partial_then_frozen:
        case behavior_kind::oversized:
          return lost;
        case behavior_kind::corrupted_crc:
        case behavior_kind::intermittent:
          return damaged;
        case behavior_kind::flood:
        case behavior_kind::repeating:
        case behavior_kind::garbage:
        case behavior_kind::malicious:
          return outcome_class::dropped_counted;
        case behavior_kind::disconnecting:
        case behavior_kind::works_then_disconnects:
        case behavior_kind::cable_unplugged:
          return cut;
        default:
          return outcome_class::success;
        }
    default:
      break;
    }
  return outcome_class::success;
}

// --- behavior

behavior::behavior () : impl_ (one_stage (behavior_stage_t ())) {}

behavior::behavior (std::shared_ptr<behavior_impl const> impl)
    : impl_ (std::move (impl))
{
}

behavior
behavior::perfect ()
{
  return behavior ();
}

behavior
behavior::echo (ms delay)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::echo;
  stage.period = delay;
  return behavior (one_stage (stage));
}

behavior
behavior::silent ()
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::silent;
  return behavior (one_stage (stage));
}

behavior
behavior::frozen (std::size_t after_replies)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::frozen;
  stage.count = after_replies;
  return behavior (one_stage (stage));
}

behavior
behavior::partial_then_frozen (std::size_t after_replies,
                               std::size_t partial_bytes)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::partial_then_frozen;
  stage.count = after_replies;
  stage.limit = partial_bytes;
  return behavior (one_stage (stage));
}

behavior
behavior::slow_trickle (ms per_byte)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::slow_trickle;
  stage.period = per_byte;
  return behavior (one_stage (stage));
}

behavior
behavior::fragmented (std::uint32_t seed, std::size_t max_fragment, ms gap)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::fragmented;
  stage.seed = seed;
  stage.count = max_fragment;
  stage.period = gap;
  return behavior (one_stage (stage));
}

behavior
behavior::flood (std::size_t total_bytes)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::flood;
  stage.count = total_bytes;
  return behavior (one_stage (stage));
}

behavior
behavior::repeating (std::size_t copies, ms gap)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::repeating;
  stage.count = copies;
  stage.period = gap;
  return behavior (one_stage (stage));
}

behavior
behavior::garbage (std::uint32_t seed, std::size_t bytes)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::garbage;
  stage.seed = seed;
  stage.count = bytes;
  return behavior (one_stage (stage));
}

behavior
behavior::corrupted_crc (std::uint32_t seed, std::size_t rate_permille,
                         std::size_t max_frames)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::corrupted_crc;
  stage.seed = seed;
  stage.count = rate_permille;
  stage.limit = max_frames;
  return behavior (one_stage (stage));
}

behavior
behavior::oversized (std::size_t total_bytes, std::uint8_t fill)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::oversized;
  stage.count = total_bytes;
  stage.byte = fill;
  return behavior (one_stage (stage));
}

behavior
behavior::zero_read (std::size_t empty_reads)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::zero_read;
  stage.count = empty_reads;
  return behavior (one_stage (stage));
}

behavior
behavior::intermittent (std::uint32_t seed, std::size_t lose_permille,
                        std::size_t max_outages, ms outage)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::intermittent;
  stage.seed = seed;
  stage.count = lose_permille;
  stage.limit = max_outages;
  stage.period = outage;
  return behavior (one_stage (stage));
}

behavior
behavior::disconnecting (std::size_t bytes_before)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::disconnecting;
  stage.count = bytes_before;
  return behavior (one_stage (stage));
}

behavior
behavior::works_then_disconnects (std::size_t after_replies)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::works_then_disconnects;
  stage.count = after_replies;
  return behavior (one_stage (stage));
}

behavior
behavior::cable_unplugged (ms unplugged_for, std::error_code open_error)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::cable_unplugged;
  stage.period = unplugged_for;
  stage.error = open_error;
  return behavior (one_stage (stage));
}

behavior
behavior::malicious (std::uint32_t seed, malicious_mode mode,
                     std::size_t bytes)
{
  behavior_stage_t stage;
  stage.kind = behavior_kind::malicious;
  stage.seed = seed;
  stage.mode = mode;
  stage.count = bytes;
  return behavior (one_stage (stage));
}

behavior
behavior::of (behavior_kind kind)
{
  switch (kind)
    {
    case behavior_kind::perfect:
      return perfect ();
    case behavior_kind::echo:
      return echo ();
    case behavior_kind::silent:
      return silent ();
    case behavior_kind::frozen:
      return frozen (1);
    case behavior_kind::partial_then_frozen:
      return partial_then_frozen (1, 4);
    case behavior_kind::slow_trickle:
      return slow_trickle (ms (1));
    case behavior_kind::fragmented:
      return fragmented (1);
    case behavior_kind::flood:
      return flood (64);
    case behavior_kind::repeating:
      return repeating (1, ms (10));
    case behavior_kind::garbage:
      return garbage (1, 16);
    case behavior_kind::corrupted_crc:
      return corrupted_crc (1, 1000, 1);
    case behavior_kind::oversized:
      return oversized (70000);
    case behavior_kind::zero_read:
      return zero_read ();
    case behavior_kind::intermittent:
      return intermittent (1, 1000, 1);
    case behavior_kind::disconnecting:
      return disconnecting (4);
    case behavior_kind::works_then_disconnects:
      return works_then_disconnects (1);
    case behavior_kind::cable_unplugged:
      return cable_unplugged (ms (100));
    case behavior_kind::malicious:
      return malicious (1);
    default:
      break;
    }
  return perfect ();
}

behavior
behavior::then (behavior const &next) const
{
  std::vector<behavior_stage_t> const &mine = impl_->stages;
  std::vector<behavior_stage_t> const &theirs = next.impl_->stages;
  bool const mine_is_perfect
      = mine.size () == 1 && mine[0].kind == behavior_kind::perfect;
  bool const next_is_perfect
      = theirs.size () == 1 && theirs[0].kind == behavior_kind::perfect;
  if (mine_is_perfect)
    return next;
  if (next_is_perfect)
    return *this;
  std::shared_ptr<behavior_impl> made = std::make_shared<behavior_impl> ();
  made->stages = mine;
  made->stages.insert (made->stages.end (), theirs.begin (), theirs.end ());
  return behavior (made);
}

behavior_kind
behavior::kind () const
{
  return impl_->stages[0].kind;
}

std::size_t
behavior::stage_count () const
{
  return impl_->stages.size ();
}

behavior_kind
behavior::stage_kind (std::size_t index) const
{
  return impl_->stages[index].kind;
}

bool
behavior::applies_to_datagrams () const
{
  for (std::size_t index = 0; index < impl_->stages.size (); ++index)
    if (impl_->stages[index].kind == behavior_kind::zero_read)
      return false;
  return true;
}

// --- behavior_applier

namespace
{
// The window of the longest unplugged stage that is still open at `now`
// (rounded up to whole milliseconds); false when none is.
bool
longest_window (behavior_impl const &shaped,
                behavior_applier_state const &state, timing::time_point_t now,
                ms &remaining, std::error_code &error)
{
  bool found = false;
  for (std::size_t index = 0; index < state.states.size (); ++index)
    {
      if (shaped.stages[index].kind != behavior_kind::cable_unplugged
          || !(now < state.states[index].until))
        continue;
      long long const nanoseconds = static_cast<long long> (
          (state.states[index].until - now).count ());
      long long const whole
          = nanoseconds / 1000000 + (nanoseconds % 1000000 != 0 ? 1 : 0);
      if (!found || ms (whole) > remaining)
        {
          remaining = ms (whole);
          error = shaped.stages[index].error;
          found = true;
        }
    }
  return found;
}

void
start_states (behavior_impl const &shaped, behavior_applier_state &state,
              timing::time_point_t now)
{
  state.states.assign (shaped.stages.size (), behavior_state_t ());
  for (std::size_t index = 0; index < state.states.size (); ++index)
    {
      behavior_stage_t const &stage = shaped.stages[index];
      behavior_state_t &each = state.states[index];
      each.generator = stage.seed;
      each.until = timing::time_point_t::min ();
      if (stage.kind == behavior_kind::cable_unplugged)
        each.until = ::lumex::applied::transport::detail::saturating_add (
            now, stage.period);
    }
}
} // namespace

behavior_applier::behavior_applier (behavior const &shaped,
                                    timing::timer_source timers)
    : behavior_ (shaped), timers_ (std::move (timers)),
      state_ (new behavior_applier_state ())
{
  start_states (*behavior_.impl_, *state_, timers_.now ());
}

behavior_applier::~behavior_applier () {}

behavior const &
behavior_applier::get () const
{
  return behavior_;
}

void
behavior_applier::reset ()
{
  // The time is read before the lock: a timer source may take its own.
  timing::time_point_t const now = timers_.now ();
  std::lock_guard<std::mutex> const lock (mutex_);
  start_states (*behavior_.impl_, *state_, now);
}

reply_plan_t
behavior_applier::plan (std::vector<std::uint8_t> const &request,
                        std::vector<std::uint8_t> const &reply, bool datagram)
{
  reply_plan_t result;
  plan_context_t context;
  context.request = &request;
  context.has_reply = !reply.empty ();
  context.datagram = datagram;
  context.now = timers_.now ();
  steps_t steps;
  if (!reply.empty ())
    steps.push_back (make_step (plan_step_kind::bytes, ms (0), reply, true));
  bool processed = true;
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    std::vector<behavior_stage_t> const &stages = behavior_.impl_->stages;
    for (std::size_t index = 0; index < stages.size (); ++index)
      {
        apply_stage (stages[index], state_->states[index], steps, processed,
                     context);
        settle (steps, datagram);
      }
  }
  std::stable_sort (steps.begin (), steps.end (), by_time);
  result.steps.swap (steps);
  result.request_processed = processed;
  return result;
}

void
behavior_applier::attach (mock_stream_control control)
{
  timing::time_point_t const now = timers_.now ();
  ms remaining (0);
  std::error_code error;
  bool found = false;
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    found = longest_window (*behavior_.impl_, *state_, now, remaining, error);
  }
  if (!found)
    return;
  control.disconnect ();
  control.fail_opens_for (remaining, error);
}

void
behavior_applier::attach (mock_datagram_control control)
{
  timing::time_point_t const now = timers_.now ();
  ms remaining (0);
  std::error_code error;
  bool found = false;
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    found = longest_window (*behavior_.impl_, *state_, now, remaining, error);
  }
  if (!found)
    return;
  control.disconnect ();
  control.fail_opens_for (remaining, error);
}

void
behavior_applier::respond (mock_stream_control control,
                           std::vector<std::uint8_t> const &request,
                           std::vector<std::uint8_t> const &reply)
{
  carry_out (plan (request, reply, false), control);
}

void
behavior_applier::respond (mock_datagram_control control,
                           std::uint8_t report_id,
                           std::vector<std::uint8_t> const &request,
                           std::vector<std::uint8_t> const &reply)
{
  carry_out (plan (request, reply, true), control, report_id);
}
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
