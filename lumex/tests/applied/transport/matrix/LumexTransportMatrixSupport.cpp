// The non-template part of the transport matrix harness: the driver of the
// context, the classes of what a step saw and their comparison with the
// expected outcome, the parameters of each behaviour, the checks of the
// counters, the log guard and the registration of the cells.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/matrix/LumexTransportMatrixSupport.hpp"

namespace lumex_matrix_test
{
namespace
{
using lumex::core::protocol::error::protocol_errc;

// A short pause of a threaded driver, and how many pauses in a row the
// bench must stay still before the clock moves to its next wait (many more
// before it moves to the end of a budget with no wait pending: a handler
// chain on its way may start one yet). Still: the same progress of the bench,
// the same waits of the clock and, where `std::clock` is processor time, no
// processor time spent by the runner threads.
std::chrono::microseconds const ksettle (1000);
int const kstill_pauses = 5;
int const kidle_pauses = 50;
int const kmost_pauses = 2000;

std::clock_t
processor_time ()
{
#if defined(_WIN32)
  // `std::clock` is wall time there.
  return 0;
#else
  return std::clock ();
#endif
}

bool
is_protocol (std::error_code const &error)
{
  return error.category ()
         == std::error_code (protocol_errc::timeout).category ();
}

bool
moved (std::uint64_t before, std::uint64_t after)
{
  return after > before;
}

// Something was dropped and counted between the two snapshots.
bool
dropped (snapshot_t const &before, snapshot_t const &after)
{
  return moved (before.wire.errors, after.wire.errors)
         || moved (before.engine.unknown_frames, after.engine.unknown_frames)
         || moved (before.engine.unexpected_replies,
                   after.engine.unexpected_replies)
         || moved (before.engine.late_replies, after.engine.late_replies)
         || moved (before.engine.stream_gaps, after.engine.stream_gaps);
}

bool
reconnected (snapshot_t const &before, snapshot_t const &after)
{
  return moved (before.engine.reconnects, after.engine.reconnects);
}
} // namespace

// --- the driver

driver_t::driver_t (bool threaded)
{
  if (threaded)
    runner_.reset (new tr::stream::io_runner (4));
  else
    context_.reset (new asio::io_context ());
}

driver_t::~driver_t () { stop (); }

tr::executor_type
driver_t::executor ()
{
  if (runner_)
    return runner_->get_executor ();
  return context_->get_executor ();
}

bool
driver_t::threaded () const LUMEX_NOEXCEPT
{
  return static_cast<bool> (runner_);
}

bool
driver_t::run_until (tr::testing::virtual_clock &clock,
                     std::function<bool ()> const &done, ms budget)
{
  if (!runner_)
    return tr::testing::run_until (*context_, clock, done, budget);
  tr::timing::time_point_t const end = clock.now () + budget;
  for (;;)
    {
      if (done ())
        return true;
      int still = 0;
      int pauses = 0;
      std::uint64_t last = ~std::uint64_t (0);
      std::clock_t last_time = processor_time ();
      for (;;)
        {
          std::this_thread::sleep_for (ksettle);
          if (done ())
            return true;
          std::uint64_t const now_progress
              = (progress_ ? progress_ () : 0) + clock.pending () * 1000003u
                + static_cast<std::uint64_t> (
                    clock.next_expiry ().time_since_epoch ().count ());
          std::clock_t const now_time = processor_time ();
          bool const idle = now_time - last_time < static_cast<std::clock_t> (
                                CLOCKS_PER_SEC / 2000);
          still = now_progress == last && idle ? still + 1 : 0;
          last = now_progress;
          last_time = now_time;
          // A bench that never goes still (a busy machine) moves on after a
          // while all the same.
          if (still >= (clock.pending () != 0 ? kstill_pauses : kidle_pauses)
              || ++pauses > kmost_pauses)
            break;
        }
      tr::timing::time_point_t const now = clock.now ();
      if (now >= end)
        {
          // The waits due at the end of the budget, once.
          std::promise<void> moved_now;
          asio::post (clock_executor_ ? clock_executor_ : executor (),
                      [&clock, &moved_now] ()
                        {
                          clock.advance (ms (0));
                          moved_now.set_value ();
                        });
          moved_now.get_future ().wait ();
          for (int i = 0; i < kstill_pauses && !done (); ++i)
            std::this_thread::sleep_for (ksettle);
          return done ();
        }
      // The clock moves on a runner thread (a thread sanitizer does not see
      // the fences of the executor's count from another one).
      std::promise<void> step;
      asio::post (clock_executor_ ? clock_executor_ : executor (),
                  [&clock, &step, end] ()
                    {
                      tr::timing::time_point_t const at = clock.now ();
                      tr::timing::time_point_t const next
                          = clock.next_expiry ();
                      if (next <= at)
                        clock.advance (ms (0));
                      else if (next >= end)
                        {
                          // Rounded up: the clock reaches the end.
                          ms left = std::chrono::duration_cast<ms> (end - at);
                          if (left < end - at)
                            left += ms (1);
                          clock.advance (left);
                        }
                      else
                        clock.advance_to_next ();
                      step.set_value ();
                    });
      step.get_future ().wait ();
    }
}

void
driver_t::set_progress (std::function<std::uint64_t ()> progress)
{
  progress_ = std::move (progress);
}

void
driver_t::set_clock_executor (tr::executor_type executor)
{
  clock_executor_ = std::move (executor);
}

void
driver_t::stop ()
{
  if (runner_)
    runner_->stop ();
}

// --- classes

char const *
seen_class_name (seen_class seen)
{
  switch (seen)
    {
    case seen_class::hang:
      return "hang";
    case seen_class::other_error:
      return "other_error";
    case seen_class::success:
      return "success";
    case seen_class::success_after_retries:
      return "success_after_retries";
    case seen_class::timeout:
      return "timeout";
    case seen_class::retries_exhausted:
      return "retries_exhausted";
    case seen_class::link_lost:
      return "link_lost";
    case seen_class::protocol_error:
      return "protocol_error";
    case seen_class::dropped_counted:
      return "dropped_counted";
    }
  return "?";
}

seen_class
classify_call (observed_t const &observed)
{
  if (!observed.ended)
    return seen_class::hang;
  std::error_code const &error = observed.error;
  if (error)
    {
      if (error == protocol_errc::timeout)
        return seen_class::timeout;
      if (error == protocol_errc::retries_exhausted)
        return seen_class::retries_exhausted;
      if (error == protocol_errc::link_lost)
        return seen_class::link_lost;
      if (is_protocol (error))
        return seen_class::protocol_error;
      return seen_class::other_error;
    }
  if (moved (observed.before.engine.retries, observed.after.engine.retries)
      || reconnected (observed.before, observed.after))
    return seen_class::success_after_retries;
  if (dropped (observed.before, observed.after))
    return seen_class::dropped_counted;
  return seen_class::success;
}

seen_class
classify_stream (observed_t const &observed, std::size_t messages_late_half)
{
  if (messages_late_half == 0)
    return seen_class::timeout;
  if (reconnected (observed.before, observed.after))
    return seen_class::success_after_retries;
  if (dropped (observed.before, observed.after))
    return seen_class::dropped_counted;
  return seen_class::success;
}

bool
matches (behavior_kind kind, outcome_class expected, seen_class seen,
         bool stream)
{
  if (seen == seen_class::hang)
    return false;
  if (kind == behavior_kind::echo && !stream
      && expected == outcome_class::protocol_error)
    // A protocol-category error, or success when the echoed request decodes
    // as the reply (the step then compares the value with the echo).
    return seen == seen_class::protocol_error || seen == seen_class::success;
  if (kind == behavior_kind::malicious)
    // No worse than a framing or a protocol code: the attack may cost the
    // reply that follows it, never the link or anything outside the
    // protocol category.
    return seen != seen_class::link_lost && seen != seen_class::other_error;
  switch (expected)
    {
    case outcome_class::success:
      return seen == seen_class::success;
    case outcome_class::success_after_retries:
      return seen == seen_class::success_after_retries;
    case outcome_class::timeout:
      return seen == seen_class::timeout;
    case outcome_class::retries_exhausted:
      return seen == seen_class::retries_exhausted;
    case outcome_class::link_lost:
      return seen == seen_class::link_lost;
    case outcome_class::protocol_error:
      return seen == seen_class::protocol_error;
    case outcome_class::dropped_counted:
      return seen == seen_class::dropped_counted;
    }
  return false;
}

// --- behaviours

behavior
matrix_behavior (behavior_kind kind, std::uint8_t oversized_fill)
{
  switch (kind)
    {
    case behavior_kind::perfect:
      return behavior::perfect ();
    case behavior_kind::echo:
      return behavior::echo ();
    case behavior_kind::silent:
      return behavior::silent ();
    case behavior_kind::frozen:
      // One reply, then the freeze: a step asks twice.
      return behavior::frozen (1);
    case behavior_kind::partial_then_frozen:
      return behavior::partial_then_frozen (1, 2);
    case behavior_kind::slow_trickle:
      // Inside every timeout of the tables.
      return behavior::slow_trickle (ms (1));
    case behavior_kind::fragmented:
      return behavior::fragmented (59, 3, ms (1));
    case behavior_kind::flood:
      return behavior::flood (64);
    case behavior_kind::repeating:
      return behavior::repeating (1, ms (10));
    case behavior_kind::garbage:
      return behavior::garbage (59, 16);
    case behavior_kind::corrupted_crc:
      // The first frame only: a retry gets a whole one.
      return behavior::corrupted_crc (59, 1000, 1);
    case behavior_kind::oversized:
      return behavior::oversized (KOVERSIZED, oversized_fill);
    case behavior_kind::zero_read:
      return behavior::zero_read (2);
    case behavior_kind::intermittent:
      // The first reply only.
      return behavior::intermittent (59, 1000, 1);
    case behavior_kind::disconnecting:
      return behavior::disconnecting (2);
    case behavior_kind::works_then_disconnects:
      return behavior::works_then_disconnects (0);
    case behavior_kind::cable_unplugged:
      return behavior::cable_unplugged (ms (300));
    case behavior_kind::malicious:
      return behavior::malicious (59, tr::testing::malicious_mode::mixed, 64);
    }
  return behavior::perfect ();
}

behavior
stream_behavior (behavior_kind kind, std::uint8_t oversized_fill)
{
  if (kind == behavior_kind::frozen)
    return behavior::frozen (0);
  if (kind == behavior_kind::partial_then_frozen)
    return behavior::partial_then_frozen (0, 2);
  return matrix_behavior (kind, oversized_fill);
}

std::vector<behavior_kind>
kinds_for (bool datagram)
{
  std::vector<behavior_kind> out;
  std::vector<behavior_kind> const all = tr::testing::all_behavior_kinds ();
  for (std::size_t i = 0; i < all.size (); ++i)
    {
      if (datagram
          && (!behavior::of (all[i]).applies_to_datagrams ()
              || all[i] == behavior_kind::fragmented))
        continue;
      out.push_back (all[i]);
    }
  return out;
}

// --- counters

void
check_counters (behavior_kind kind, outcome_class expected,
                observed_t const &observed, std::size_t attempts,
                std::string const &where)
{
  snapshot_t const &before = observed.before;
  snapshot_t const &after = observed.after;
  switch (kind)
    {
    case behavior_kind::perfect:
    case behavior_kind::slow_trickle:
    case behavior_kind::fragmented:
    case behavior_kind::zero_read:
      EXPECT_EQ (after.wire.errors, before.wire.errors)
          << where << ": framing errors on a whole reply";
      EXPECT_EQ (after.engine.unknown_frames, before.engine.unknown_frames)
          << where << ": unknown frames on a whole reply";
      EXPECT_EQ (after.engine.retries, before.engine.retries)
          << where << ": retries on a whole reply";
      break;
    case behavior_kind::silent:
    case behavior_kind::frozen:
    case behavior_kind::partial_then_frozen:
    case behavior_kind::oversized:
      if (expected == outcome_class::timeout
          || expected == outcome_class::retries_exhausted)
        {
          EXPECT_EQ (after.engine.requests - before.engine.requests, attempts)
              << where << ": attempts";
          EXPECT_EQ (after.engine.timeouts - before.engine.timeouts, attempts)
              << where << ": timeouts";
        }
      break;
    case behavior_kind::flood:
    case behavior_kind::garbage:
      if (expected == outcome_class::dropped_counted)
        {
          // The noise itself: a framing error, or a frame of no row.
          EXPECT_GT (after.wire.errors + after.engine.unknown_frames,
                     before.wire.errors + before.engine.unknown_frames)
              << where << ": the noise was not counted";
        }
      break;
    case behavior_kind::corrupted_crc:
      if (expected != outcome_class::success)
        {
          EXPECT_GE (after.wire.errors - before.wire.errors, 1u)
              << where << ": the damaged frame was not counted";
        }
      break;
    case behavior_kind::intermittent:
      if (expected != outcome_class::success)
        {
          EXPECT_GE (after.engine.timeouts - before.engine.timeouts, 1u)
              << where << ": the lost reply was not a timeout";
        }
      break;
    case behavior_kind::disconnecting:
    case behavior_kind::works_then_disconnects:
    case behavior_kind::cable_unplugged:
      if (expected != outcome_class::success)
        {
          EXPECT_GE (after.engine.reconnects - before.engine.reconnects, 1u)
              << where << ": no reconnect";
        }
      if (kind == behavior_kind::cable_unplugged
          && expected != outcome_class::success)
        {
          EXPECT_GE (after.session.attempts - before.session.attempts, 2u)
              << where << ": the opens did not go on while unplugged";
        }
      break;
    default:
      break;
    }
}

// --- known gaps

namespace
{
/// A step whose class differs from the expected one for a reason the
/// matrix reports (see the README of the transport, "Матрица"): its class is
/// pinned, so that the cell fails when the gap closes or moves.
struct known_gap_t
{
  /// The cell, `device_<x>.<y>.<transport>.<kind>`.
  char const *cell;
  /// The step: query, command, dangerous, stream.
  char const *step;
  /// The class it ends in.
  seen_class seen;
  /// Why.
  char const *note;
};

char const *const ktext_resync
    = "TODO(matrix): a text frame with a start character (delimiter_codec and "
      "prefix_char_stage) does not resynchronise on the start character: "
      "noise in front of a frame takes the frame with it, on every attempt";

char const *const ktext_partial
    = "TODO(matrix): the cut reply of the step before waits in the text "
      "decoder and glues to the next frame, which is lost (the start "
      "character does not resynchronise)";

char const *const kcobs_noise
    = "TODO(matrix): noise in front of a COBS frame takes the frame with it "
      "(no delimiter in front of a frame), on every attempt";

char const *const kmodbus_echo
    = "an echoed Modbus RTU request is no reply frame for the master codec "
      "(the third byte reads as the byte count of a reply): nothing is "
      "delivered, the call times out";

char const *const kheader_noise
    = "TODO(matrix): header_codec skips the bytes in front of a preamble "
      "without a report: noise on the line is dropped but counted nowhere";

known_gap_t const kknown_gaps[] = {
  { "device_j.a.stream.flood", "query", seen_class::retries_exhausted,
    ktext_resync },
  { "device_j.a.stream.flood", "dangerous", seen_class::retries_exhausted,
    ktext_resync },
  { "device_j.a.stream.garbage", "query", seen_class::retries_exhausted,
    ktext_resync },
  { "device_j.a.stream.garbage", "dangerous", seen_class::retries_exhausted,
    ktext_resync },
  { "device_k.a.stream.flood", "query", seen_class::timeout, ktext_resync },
  { "device_k.a.stream.flood", "dangerous", seen_class::timeout,
    ktext_resync },
  { "device_k.a.stream.garbage", "query", seen_class::timeout, ktext_resync },
  { "device_k.a.stream.garbage", "dangerous", seen_class::timeout,
    ktext_resync },
  { "device_m.a.stream.flood", "query", seen_class::retries_exhausted,
    ktext_resync },
  { "device_m.a.stream.garbage", "query", seen_class::retries_exhausted,
    ktext_resync },
  { "device_j.a.stream.partial_then_frozen", "dangerous",
    seen_class::retries_exhausted, ktext_partial },
  { "device_k.a.stream.partial_then_frozen", "dangerous", seen_class::timeout,
    ktext_partial },
  { "device_i.b.stream.flood", "query", seen_class::retries_exhausted,
    kcobs_noise },
  { "device_i.b.stream.flood", "stream", seen_class::timeout, kcobs_noise },
  { "device_i.b.stream.garbage", "query", seen_class::retries_exhausted,
    kcobs_noise },
  { "device_i.b.stream.garbage", "stream", seen_class::timeout, kcobs_noise },
  { "device_i.b.stream.malicious", "query", seen_class::retries_exhausted,
    kcobs_noise },
  { "device_i.b.stream.malicious", "stream", seen_class::timeout,
    kcobs_noise },
  { "device_l.b.stream.echo", "query", seen_class::timeout, kmodbus_echo },
  { "device_m.b.stream.flood", "query", seen_class::success, kheader_noise },
  { "device_m.b.stream.flood", "command", seen_class::success, kheader_noise },
  { "device_m.b.stream.flood", "dangerous", seen_class::success,
    kheader_noise },
  { "device_m.b.stream.garbage", "query", seen_class::success, kheader_noise },
  { "device_m.b.stream.garbage", "command", seen_class::success,
    kheader_noise },
  { "device_m.b.stream.garbage", "dangerous", seen_class::success,
    kheader_noise },
};

known_gap_t const *
find_gap (std::string const &cell, std::string const &step)
{
  for (std::size_t i = 0; i < sizeof (kknown_gaps) / sizeof (kknown_gaps[0]);
       ++i)
    if (cell == kknown_gaps[i].cell && step == kknown_gaps[i].step)
      return &kknown_gaps[i];
  return nullptr;
}
} // namespace

bool
is_known_gap (std::string const &cell, std::string const &step)
{
  return find_gap (cell, step) != nullptr;
}

bool
check_class (std::string const &cell, std::string const &step, char const *row,
             behavior_kind kind, outcome_class expected, seen_class seen,
             std::error_code const &error, bool stream)
{
  std::string const tail
      = error ? std::string (": ") + error.message () : std::string ();
  known_gap_t const *const gap = find_gap (cell, step);
  if (gap != nullptr)
    {
      EXPECT_EQ (seen_class_name (seen),
                 std::string (seen_class_name (gap->seen)))
          << cell << " " << step << " (" << row
          << "): the known gap moved (expected by the table "
          << tr::testing::outcome_class_name (expected) << ")" << tail;
      ::testing::Test::RecordProperty ("known_gap", step + ": " + gap->note);
      return true;
    }
  EXPECT_TRUE (matches (kind, expected, seen, stream))
      << cell << " " << step << " (" << row << "): expected "
      << tr::testing::outcome_class_name (expected) << ", seen "
      << seen_class_name (seen) << tail;
  return false;
}

// --- the log

namespace
{
std::mutex &
log_mutex ()
{
  static std::mutex instance;
  return instance;
}

std::vector<std::string> &
log_lines ()
{
  static std::vector<std::string> instance;
  return instance;
}

void
capture_debug (tr::log::log_record_t const &item)
{
  if (item.level != tr::log::log_level::debug
      || std::string (item.component) != "transport.dialogue")
    return;
  std::lock_guard<std::mutex> const lock (log_mutex ());
  log_lines ().push_back (item.message);
}

void
drop_record (tr::log::log_record_t const &)
{
}
} // namespace

log_guard_t::log_guard_t (bool enabled)
    : level_ (tr::log::get_log_level ()), sink_ (tr::log::get_log_sink ())
{
  {
    std::lock_guard<std::mutex> const lock (log_mutex ());
    log_lines ().clear ();
  }
  tr::log::set_log_sink (enabled ? &capture_debug : &drop_record);
  if (enabled)
    tr::log::set_log_level (tr::log::log_level::debug);
}

log_guard_t::~log_guard_t ()
{
  tr::log::set_log_sink (sink_);
  tr::log::set_log_level (level_);
}

std::vector<std::string>
log_guard_t::lines (std::string const &prefix)
{
  std::lock_guard<std::mutex> const lock (log_mutex ());
  std::vector<std::string> out;
  for (std::size_t i = 0; i < log_lines ().size (); ++i)
    if (log_lines ()[i].compare (0, prefix.size (), prefix) == 0)
      out.push_back (log_lines ()[i]);
  return out;
}

// --- the bench

ss::session_options_t
session_options (tr::testing::virtual_clock &clock)
{
  ss::session_options_t options;
  options.name = "matrix";
  options.policy.initial_delay = ms (100);
  options.policy.max_delay = ms (1000);
  options.policy.max_attempts = 0;
  options.policy.jitter = 0;
  options.open_timeout = ms (1000);
  options.timers = clock.source ();
  return options;
}

// --- registration

namespace
{
class cell_test_t : public ::testing::Test
{
public:
  explicit cell_test_t (std::function<void ()> body) : body_ (std::move (body))
  {
  }

  void
  TestBody () override
  {
    body_ ();
  }

private:
  std::function<void ()> body_;
};

std::vector<std::string> &
names ()
{
  static std::vector<std::string> instance;
  return instance;
}
} // namespace

void
register_cells (std::vector<cell_t> const &cells)
{
  for (std::size_t i = 0; i < cells.size (); ++i)
    {
      std::function<void ()> const body = cells[i].body;
      ::testing::RegisterTest (cells[i].suite.c_str (), cells[i].name.c_str (),
                               nullptr, nullptr, __FILE__, __LINE__,
                               [body] () -> ::testing::Test *
                                 { return new cell_test_t (body); });
      names ().push_back (cells[i].suite + "." + cells[i].name);
    }
}

std::vector<std::string>
registered_cells ()
{
  return names ();
}
} // namespace lumex_matrix_test
