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
 * @file LumexSessionTypes.hpp
 * @brief The types of the transport sessions: the channel states, the
 * reconnect policy and its delay, the state change a session reports, the
 * options of a session and its counters.
 * @details A session (`stream_session`, `message_session`) keeps one
 * transport made by a factory open: it opens it, sees the link go (an I/O
 * error that `is_link_lost` accepts, or the removal of the device), and opens
 * a new one per `reconnect_policy_t`. This is the transport level of spec G1;
 * repeating `identify ()` and resending rows after a reconnect belongs to the
 * protocol client (phases 4 and 5).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_SESSION_TYPES_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_SESSION_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <system_error>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
namespace session
{
/**
 * @brief The state of a session's channel.
 * @details `closed` until `start ()`; `connecting` through the attempts of
 * the first open; `open` while the transport works; `reconnecting` through
 * the attempts after the link went; `faulted` when a cycle used up
 * `max_attempts` (`start ()` begins a new cycle). `close ()` returns to
 * `closed` from any state.
 */
enum class channel_state : int
{
  closed,
  connecting,
  open,
  reconnecting,
  faulted
};

/// @brief "closed", "connecting", "open", "reconnecting", "faulted".
char const *to_string (channel_state state) LUMEX_NOEXCEPT;

/**
 * @brief How a session retries.
 * @details The first attempt of a cycle runs at once. After the failed
 * attempt `n` (1, 2, ...) the session waits `reconnect_delay (policy, n, r)`:
 * `initial_delay * multiplier^(n - 1)`, at most `max_delay`, then scaled by a
 * factor drawn from `[1 - jitter, 1 + jitter]` and kept within
 * `[0, max_delay]`. The session waits at least 1 ms between two attempts,
 * also when that is 0 (`initial_delay` or `max_delay` 0).
 *
 * A link counts as established once it stayed open `initial_delay` (at least
 * 1 ms); one that goes later starts a new cycle (`reconnecting`) whose first
 * attempt runs at once. One that goes earlier always makes the next attempt
 * wait its delay (no back-to-back reconnects through a link that dies at
 * once):
 * - when it moved no data (no read or write of at least one byte, no
 *   message received or sent; a peer that accepts and closes, a device that
 *   resets on open), it is a failed attempt of its cycle: the session waits
 *   the delay of that attempt, and `max_attempts` ends such a cycle in
 *   `faulted`;
 * - when it moved data, it worked: it starts a new cycle (`reconnecting`,
 *   not a failed attempt) whose first attempt waits the delay of attempt 1.
 */
struct reconnect_policy_t
{
  /// The wait after the first failed attempt.
  std::chrono::milliseconds initial_delay = std::chrono::milliseconds (100);
  /// The longest wait.
  std::chrono::milliseconds max_delay = std::chrono::milliseconds (30000);
  /// The growth of the wait per failed attempt (values below 1 count as 1).
  double multiplier = 2.0;
  /// The attempts of one cycle before `faulted`; 0: unlimited.
  std::uint32_t max_attempts = 0;
  /// The random share of the wait, 0 (none) to 1 (0 to twice the wait).
  double jitter = 0.1;
};

/**
 * @brief The wait after the failed attempt `failed_attempt` (from 1).
 * @param random01 A random number in [0, 1) (the session's random source);
 * 0.5 gives the wait without jitter.
 */
std::chrono::milliseconds reconnect_delay (reconnect_policy_t const &policy,
                                           std::uint32_t failed_attempt,
                                           double random01) LUMEX_NOEXCEPT;

/**
 * @brief True when an I/O error means the link is gone and the session
 * reconnects: every error except `cancelled`, `timeout`, `would_deadlock`,
 * `report_too_large` and `unknown_report_id` (which end one operation, not
 * the link).
 */
bool is_link_lost (std::error_code const &error) LUMEX_NOEXCEPT;

/**
 * @brief The state of a session with the number of its changes so far, read
 * together; `async_wait_state_change` from a snapshot never misses a change.
 */
struct state_snapshot_t
{
  /// The state.
  channel_state state = channel_state::closed;
  /// The state changes of the session before this snapshot (0 for a new
  /// session).
  std::uint64_t sequence = 0;
};

/// @brief A change of a session's state.
struct state_change_t
{
  /// The state before.
  channel_state previous = channel_state::closed;
  /// The state now.
  channel_state current = channel_state::closed;
  /// Why: the error of the last attempt (`faulted`), the error that ended
  /// the link (`reconnecting`), `cancelled` for a `cancel ()`; empty
  /// otherwise.
  std::error_code error;
  /// The attempts of the current cycle so far.
  std::uint32_t attempt = 0;
  /// The state changes of the session so far, this one included (the
  /// `sequence` of `state_snapshot_t`).
  std::uint64_t sequence = 0;
};

/// @brief Called on the session's strand at every state change.
using state_callback_t = std::function<void (state_change_t const &)>;

/// @brief A random number in [0, 1) for the jitter (called on the strand).
using random_source_t = std::function<double ()>;

/// @brief Tells a session whether its device is present (any thread); a
/// `device_watcher` calls it (see `device_watcher::watch_presence`).
using presence_sink_t = std::function<void (bool present)>;

/// @brief The settings of a session.
struct session_options_t
{
  /// The name for logs ("" gives "session").
  std::string name;
  /// How the session retries.
  reconnect_policy_t policy;
  /// The longest one open attempt may take: then the attempt fails with
  /// `timeout` and its transport is closed (one whose open cannot be
  /// cancelled, such as the serial stream's bounded open, closes what it
  /// opens late); 0: no limit.
  std::chrono::milliseconds open_timeout = std::chrono::milliseconds (10000);
  /// Called at every state change, on the strand; never after the session
  /// was destroyed (a call already running on another thread may still
  /// finish). An exception it throws is logged and dropped.
  state_callback_t on_state_change;
  /// The random source of the jitter; empty: a generator of the session.
  random_source_t random;
};

/// @brief What a session counts besides its transport's statistics.
struct session_statistics_t
{
  /// Open attempts (each call of the factory).
  std::uint64_t attempts = 0;
  /// Attempts that failed (the factory threw, the open failed or timed
  /// out, or the link went before it was established without moving data;
  /// see `reconnect_policy_t`).
  std::uint64_t failed_attempts = 0;
  /// Opens after a lost link (`reconnecting` -> `open`).
  std::uint64_t reconnects = 0;
};

/// @brief The completion of `async_wait_state_change`: an error
/// (`cancelled`) or the new state.
using state_handler_t
    = asio::any_completion_handler<void (std::error_code, channel_state)>;

/// @brief The completion of `async_wait_state_change` from a snapshot: an
/// error (`cancelled`) or the snapshot after the change.
using snapshot_handler_t
    = asio::any_completion_handler<void (std::error_code, state_snapshot_t)>;
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_SESSION_TYPES_HPP
