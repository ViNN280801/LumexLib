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
 * @file LumexBehaviors.hpp
 * @brief The 18 device behaviours of the simulator and the matrix as
 * composable values, the applier that shapes a device's output and drives a
 * mock with it, and the expected outcome of each behaviour for a row.
 * @details A `behavior` is a value: a list of stages (`then` appends), each
 * made by a factory of its kind with explicit parameters and, where it draws
 * random numbers, an explicit seed. A `behavior_applier` holds the state of
 * one device (reply counters, the generator of every seeded stage, the
 * window of an outage) and turns each request/reply pair into a
 * `reply_plan_t`: timed steps (bytes, an empty read, a hang-up) relative to
 * the moment of the request, on the clock of the `timing::timer_source` the
 * applier was made with. The same source must drive the mock: its
 * `feed_after`, `deliver_after`, `empty_read_after` and `disconnect_after`
 * carry the plan out, so the plan runs on a `virtual_clock` with no real
 * waiting. `respond` does both in one call and is what a simulator calls from
 * the write (or send) observer of its mock.
 *
 * Order. The reply (or nothing, for a row without one) enters the first
 * stage, each stage rewrites the steps of the one before, and the steps are
 * sorted by time at the end (stable: steps of equal time keep the order the
 * stages gave them). `fragmented ().then (corrupted_crc ())` therefore flips
 * a byte of the last fragment's frame as a whole, `corrupted_crc ().then
 * (fragmented ())` splits an already damaged frame. The stages that count
 * replies (`frozen`, `partial_then_frozen`, `intermittent`, `disconnecting`,
 * `works_then_disconnects`) and those that damage a reply (`corrupted_crc`,
 * `oversized`, `repeating`) look at the row: a request whose reply is empty
 * is not a reply for them. The stages that put bytes on the line (`flood`,
 * `garbage`, `malicious`, `echo`) act on every request. Stages without a
 * datagram meaning change nothing for a datagram (`zero_read`); the others
 * have a twin: `fragmented` splits across datagrams, `partial_then_frozen`
 * truncates the datagram, `slow_trickle` delays the whole datagram by one
 * period per byte.
 *
 * Determinism. Every random choice comes from `splitmix64` seeded with the
 * stage's seed, so equal seeds give equal bytes on every platform and
 * standard library (no `<random>` distribution is used). The applier is
 * thread-safe; to keep the delivery order of `respond`, call it from one
 * thread or strand, as a mock's observer does. A behaviour that has fired
 * (`disconnecting`, `works_then_disconnects`) is spent: the device is
 * plugged back by a reopen and answers whole from then on; `reset` starts it
 * over. Available with `LUMEX_TRANSPORT_WITH_TESTING`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_BEHAVIORS_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_BEHAVIORS_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace testing
{
/// @brief The behaviours of a simulated device (one factory of `behavior`
/// each).
enum class behavior_kind : std::uint8_t
{
  /// The reply, whole and at once.
  perfect,
  /// The request comes back in place of the reply.
  echo,
  /// No reply, ever; the requests are written but not processed.
  silent,
  /// Answers some replies, then stops answering without hanging up.
  frozen,
  /// Answers some replies, then sends a prefix of the next one and freezes.
  partial_then_frozen,
  /// One byte per period.
  slow_trickle,
  /// The reply in small pieces at seeded positions.
  fragmented,
  /// A burst of a counting pattern before the reply.
  flood,
  /// The reply again, later, one or more times.
  repeating,
  /// Seeded noise before the reply.
  garbage,
  /// One byte inside the frame changed.
  corrupted_crc,
  /// One block larger than any frame instead of the reply.
  oversized,
  /// Empty reads before the data.
  zero_read,
  /// Some replies lost, seeded, optionally with a silent window after each.
  intermittent,
  /// A prefix of the replies (counted in bytes), then a hang-up.
  disconnecting,
  /// Whole replies, then a hang-up.
  works_then_disconnects,
  /// The cable is out: a hang-up now and failing opens until it is back.
  cable_unplugged,
  /// Structured attack patterns before the reply.
  malicious
};

/// @brief The attack patterns of `behavior::malicious` (byte values common
/// to the framings of `core/framing`: a delimiter, an escape, a length).
enum class malicious_mode : std::uint8_t
{
  /// 0xC0 repeated.
  end_flood,
  /// 0x00 repeated.
  nul_flood,
  /// 0xFF repeated.
  ff_flood,
  /// The bytes 0x00 to 0x1F in turn.
  control_chars,
  /// 0x00 and 0xFF in turn.
  alternating_extremes,
  /// A delimiter, an escape followed by a byte that is no escape code, and
  /// an escape before a delimiter.
  bad_escape,
  /// 0xAB repeated (a header that never ends).
  long_unterminated,
  /// The largest signed length in both byte orders, repeated.
  max_length_prefix,
  /// Pairs of an escape and the code of a delimiter, repeated.
  dense_escapes,
  /// One of the above chosen by the seed for each request.
  mixed
};

/// @brief What a row reply looks like to the behaviours.
enum class reply_shape : std::uint8_t
{
  /// A command without a reply.
  none,
  /// One reply per request.
  one,
  /// A stream of replies after the request.
  stream,
  /// One reply that acknowledges a command of a dialogue (a status).
  dialogue_ack
};

/// @brief How a call ends, as the matrix asserts it.
enum class outcome_class : std::uint8_t
{
  /// The call returned the reply.
  success,
  /// It returned the reply after a retry or after a reconnect that re-sent
  /// the request.
  success_after_retries,
  /// The only attempt timed out.
  timeout,
  /// Every attempt of a row with retries timed out.
  retries_exhausted,
  /// The link was lost (and not recovered for this row).
  link_lost,
  /// The reply was not a valid answer (a protocol-category error).
  protocol_error,
  /// The call succeeded and the engine counted what it dropped (noise,
  /// duplicates, damaged frames, stream gaps).
  dropped_counted
};

/// @brief The part of a row that decides the outcome of a behaviour.
struct row_shape_t
{
  /// Retries after the first attempt.
  std::size_t retries = 0;
  /// The row may be sent again (after a retry or a reconnect).
  bool idempotent = false;
  /// What the reply looks like.
  reply_shape reply = reply_shape::one;
  /// The client has a session that reconnects and re-sends idempotent rows
  /// (and re-enables streams).
  bool reconnects = false;
  /// The protocol names a watchdog that ends a stream that went quiet.
  bool watchdog = false;
};

/// @brief What one step of a plan does.
enum class plan_step_kind : std::uint8_t
{
  /// Bytes (a datagram) reach the reader.
  bytes,
  /// The next read ends with no bytes and no error.
  empty_read,
  /// The link goes down.
  disconnect
};

/// @brief One thing a behaviour does, `at` after the request.
struct plan_step_t
{
  plan_step_kind kind = plan_step_kind::bytes;
  /// The offset from the moment of the request (never negative).
  std::chrono::milliseconds at = std::chrono::milliseconds (0);
  /// The bytes of a `bytes` step.
  std::vector<std::uint8_t> bytes;
};

/// @brief What a device does for one request, in time order.
struct reply_plan_t
{
  std::vector<plan_step_t> steps;
  /// False when the device did not process the request (a silent, frozen or
  /// unplugged device, or one that echoes): a simulator then skips the
  /// handler of the row and its state change.
  bool request_processed = true;
};

class behavior_applier;

/// @brief What a factory of `behavior` was given. The meaning of the fields
/// is per kind and is set out where the stage is applied.
struct behavior_stage_t
{
  behavior_kind kind = behavior_kind::perfect;
  std::uint32_t seed = 0;
  std::size_t count = 0;
  std::size_t limit = 0;
  std::chrono::milliseconds period = std::chrono::milliseconds (0);
  std::uint8_t byte = 0;
  malicious_mode mode = malicious_mode::mixed;
  std::error_code error;
};

/// @brief The state of one stage in an applier.
struct behavior_state_t
{
  std::uint64_t generator = 0;
  std::size_t count = 0;
  bool spent = false;
  timing::time_point_t until;
};

/// @brief How long the opens still fail after an unplugged cable, and with
/// what.
struct behavior_window_t
{
  std::chrono::milliseconds remaining = std::chrono::milliseconds (0);
  std::error_code error;
};

/// @brief A behaviour: a value that is cheap to copy.
class behavior
{
public:
  /// @brief `perfect ()`.
  behavior ();

  /// @brief The reply whole and at once.
  static behavior perfect ();
  /// @brief The request in place of the reply, `delay` later.
  static behavior echo (std::chrono::milliseconds delay
                        = std::chrono::milliseconds (0));
  /// @brief No reply, ever.
  static behavior silent ();
  /// @brief `after_replies` replies are answered, the following requests are
  /// neither processed nor answered.
  static behavior frozen (std::size_t after_replies);
  /// @brief `after_replies` replies are answered; the next one is cut to its
  /// first `partial_bytes` bytes (at most all but one: a frame is never
  /// complete) and the device freezes.
  static behavior partial_then_frozen (std::size_t after_replies,
                                       std::size_t partial_bytes);
  /// @brief Each byte of the reply arrives `per_byte` after the one before
  /// (the first after `per_byte`); a datagram arrives whole after `per_byte`
  /// times its size.
  static behavior slow_trickle (std::chrono::milliseconds per_byte);
  /// @brief The reply in pieces of 1 to `max_fragment` bytes (0 reads as 1),
  /// sizes drawn from the seed, the pieces `gap` apart (a datagram: one
  /// datagram per piece).
  static behavior fragmented (std::uint32_t seed, std::size_t max_fragment = 3,
                              std::chrono::milliseconds gap
                              = std::chrono::milliseconds (1));
  /// @brief `total_bytes` of the pattern 0, 1, ..., 255, 0, ... at once
  /// before the reply (and before no reply, too).
  static behavior flood (std::size_t total_bytes);
  /// @brief The reply `copies` more times, the n-th copy `n` times `gap`
  /// after the reply.
  static behavior repeating (std::size_t copies,
                             std::chrono::milliseconds gap);
  /// @brief `bytes` pseudo-random bytes at once before the reply (and
  /// before no reply, too); the next request gets the next bytes.
  static behavior garbage (std::uint32_t seed, std::size_t bytes);
  /// @brief With probability `rate_permille` / 1000 (at most 1000) one byte
  /// strictly inside the frame (not the first or the last, unless the frame
  /// has three bytes or fewer) is XORed with a non-zero seeded value;
  /// `max_frames` limits how many frames are damaged (0: no limit).
  static behavior corrupted_crc (std::uint32_t seed,
                                 std::size_t rate_permille = 1000,
                                 std::size_t max_frames = 0);
  /// @brief The reply is replaced by `total_bytes` copies of `fill` (choose a
  /// size above the codec's frame limit and a fill that is no delimiter).
  static behavior oversized (std::size_t total_bytes,
                             std::uint8_t fill = 0xAB);
  /// @brief `empty_reads` empty reads before each piece of data (stream
  /// only).
  static behavior zero_read (std::size_t empty_reads = 1);
  /// @brief With probability `lose_permille` / 1000 a reply is lost and the
  /// device is silent for `outage` (replies in that window are lost too);
  /// at most `max_outages` outages (0: no limit).
  static behavior intermittent (std::uint32_t seed, std::size_t lose_permille,
                                std::size_t max_outages,
                                std::chrono::milliseconds outage
                                = std::chrono::milliseconds (0));
  /// @brief After `bytes_before` bytes of replies (counted over all of them,
  /// the last one cut) the device hangs up, once.
  static behavior disconnecting (std::size_t bytes_before);
  /// @brief After `after_replies` whole replies the device hangs up, once
  /// (0: it hangs up instead of the first reply).
  static behavior works_then_disconnects (std::size_t after_replies);
  /// @brief The cable is out for `unplugged_for` from the creation of the
  /// applier (or its `reset`): the device processes and answers nothing, and
  /// `attach` hangs the link up and makes the opens fail with `open_error`
  /// for the rest of that time.
  static behavior
  cable_unplugged (std::chrono::milliseconds unplugged_for,
                   std::error_code open_error
                   = make_error_code (error::transport_errc::disconnected));
  /// @brief `bytes` bytes of an attack pattern before the reply (and before
  /// no reply, too).
  static behavior malicious (std::uint32_t seed,
                             malicious_mode mode = malicious_mode::mixed,
                             std::size_t bytes = 64);
  /// @brief The behaviour of `kind` with parameters for a smoke run (seed 1,
  /// small counts); the matrix passes its own for the cells that need them.
  static behavior of (behavior_kind kind);

  /// @brief This behaviour, then `next` (see the file description). A
  /// `perfect ()` on either side adds nothing.
  behavior then (behavior const &next) const;

  /// @brief The kind of the first stage.
  behavior_kind kind () const;
  /// @brief How many stages (at least one).
  std::size_t stage_count () const;
  /// @brief The kind of stage `index` (`index` below `stage_count ()`).
  behavior_kind stage_kind (std::size_t index) const;
  /// @brief False when a stage has no datagram meaning (`zero_read`).
  bool applies_to_datagrams () const;

private:
  friend class behavior_applier;

  static behavior single (behavior_stage_t const &stage);

  std::vector<behavior_stage_t> stages_;
};

/// @brief The name of a kind, as the CTest names of the matrix spell it
/// (`"works_then_disconnects"`).
char const *behavior_kind_name (behavior_kind kind);

/// @brief The name of an outcome class (`"success_after_retries"`).
char const *outcome_class_name (outcome_class outcome);

/// @brief The 18 kinds, in the order of the enumeration.
std::vector<behavior_kind> all_behavior_kinds ();

/**
 * @brief The class a call ends in under the behaviour `kind` on a row of
 * `shape`, with the behaviour's parameters chosen so that it damages what its
 * description says (a frozen device is asked after the freeze, an
 * `intermittent` or `corrupted_crc` one loses only the first attempt).
 * @details A row without a reply sees only the bytes that are put on the line
 * (`echo`, `flood`, `garbage`, `malicious`: `dropped_counted`) and an
 * unplugged cable. A dialogue acknowledgement is a reply. `malicious` may
 * also end as `protocol_error`: the class says what the engine must not do
 * worse than.
 */
outcome_class expected_outcome (behavior_kind kind, row_shape_t const &shape);

/// @brief The state of one device under a behaviour, and the carrier of its
/// plans to a mock.
class behavior_applier
{
public:
  /// @brief A device under `shaped`; `timers` tells the time of the plans
  /// (give it the source of the mock's options).
  explicit behavior_applier (behavior const &shaped,
                             timing::timer_source timers
                             = timing::timer_source ());

  behavior_applier (behavior_applier const &) = delete;
  behavior_applier &operator= (behavior_applier const &) = delete;

  /// @brief The behaviour.
  behavior const &get () const;

  /// @brief Starts over: counters, generators, spent behaviours and the
  /// unplugged window as at creation (the time is taken again).
  void reset ();

  /// @brief What the device does for `request` with `reply` (empty for a
  /// row without one); `datagram` selects the datagram twins.
  reply_plan_t plan (std::vector<std::uint8_t> const &request,
                     std::vector<std::uint8_t> const &reply,
                     bool datagram = false);

  /// @brief For a `cable_unplugged` stage: hangs the link up and makes the
  /// opens fail for the rest of its window. Other stages do nothing.
  void attach (mock_stream_control control);
  /// @brief As for a stream.
  void attach (mock_datagram_control control);

  /// @brief `plan` for a mock stream, carried out on `control`.
  void respond (mock_stream_control control,
                std::vector<std::uint8_t> const &request,
                std::vector<std::uint8_t> const &reply);
  /// @brief `plan` for a mock datagram transport (`datagram` true), each
  /// message with `report_id`.
  void respond (mock_datagram_control control, std::uint8_t report_id,
                std::vector<std::uint8_t> const &request,
                std::vector<std::uint8_t> const &reply);

private:
  void start (timing::time_point_t now);
  std::vector<behavior_window_t> unplugged_windows ();

  behavior behavior_;
  timing::timer_source timers_;
  std::mutex mutex_;
  std::vector<behavior_state_t> states_;
};
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_BEHAVIORS_HPP
