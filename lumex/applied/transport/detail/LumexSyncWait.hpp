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
 * @file LumexSyncWait.hpp
 * @brief How a synchronous transport call waits for its asynchronous
 * operation: a shared state the completion handler fills, a wait with the
 * deadline, the cancellation on the deadline, and the check that the calling
 * thread does not run the transport's executor.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_SYNC_WAIT_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_SYNC_WAIT_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <system_error>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"

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
namespace detail
{
/// @brief What the completion handler of a synchronous call leaves behind.
struct sync_state
{
  std::mutex mutex;
  std::condition_variable done_signal;
  bool done = false;
  std::error_code error;
  std::size_t bytes = 0;
};

/// @brief The result a synchronous call returns.
struct sync_outcome
{
  std::error_code error;
  std::size_t bytes;
};

/// @brief The handler of a call without a byte count.
struct sync_status_handler
{
  std::shared_ptr<sync_state> state;
  void
  operator() (std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (state->mutex);
    state->error = error;
    state->done = true;
    state->done_signal.notify_all ();
  }
};

/// @brief The handler of a read or a write.
struct sync_io_handler
{
  std::shared_ptr<sync_state> state;
  void
  operator() (std::error_code error, std::size_t bytes)
  {
    std::lock_guard<std::mutex> const lock (state->mutex);
    state->error = error;
    state->bytes = bytes;
    state->done = true;
    state->done_signal.notify_all ();
  }
};

/**
 * @brief True when the calling thread runs `executor` (an `io_context`, a
 * `thread_pool` or a strand of either); false for other executor types,
 * which cannot be asked.
 */
bool runs_on (executor_type const &executor) LUMEX_NOEXCEPT;

/**
 * @brief Waits for the operation of `state` until the deadline; then cancels
 * it through `front.cancel ()` and waits for its completion, and reports
 * `timeout` for an operation the cancel ended.
 */
template <typename Front>
sync_outcome
wait (sync_state &state, deadline_t deadline, Front &front)
{
  std::unique_lock<std::mutex> lock (state.mutex);
  bool timed_out = false;
  if (!state.done_signal.wait_for (lock, deadline,
                                   [&state] { return state.done; }))
    {
      timed_out = true;
      lock.unlock ();
      front.cancel ();
      lock.lock ();
      state.done_signal.wait (lock, [&state] { return state.done; });
    }
  sync_outcome outcome = { state.error, state.bytes };
  if (timed_out
      && error::is_transport_error (outcome.error,
                                    error::transport_errc::cancelled))
    outcome.error = error::transport_errc::timeout;
  return outcome;
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_SYNC_WAIT_HPP
