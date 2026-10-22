/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

#include "lumex/applied/logger/logger/LumexLogger.hpp"
#include "lumex/applied/logger/logger/LumexTriggerFileWatcher.hpp"

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
namespace logger
{
namespace logger
{
namespace
{
/// Watcher threads started and not finished (trigger_file_watcher::
/// running_threads).
std::atomic<std::size_t> g_running_threads (0);
} // namespace

/// The thread of a watcher and what it shares with the owner.
struct trigger_file_watcher::state_t
{
  explicit state_t (std::chrono::milliseconds every)
      : period (every), stop (false)
  {
  }

  std::chrono::milliseconds const period; ///< Time between two checks
  std::mutex mutex;                       ///< Guards stop
  std::condition_variable wake;           ///< Signalled when stop is set
  bool stop;                              ///< The owner asks the thread to end
  std::thread thread;                     ///< The thread itself
};

trigger_file_watcher::trigger_file_watcher (std::chrono::milliseconds period)
    : watcher_state (new state_t (period < std::chrono::milliseconds (1)
                                      ? std::chrono::milliseconds (1)
                                      : period))
{
  state_t *const state = watcher_state;
  // The singleton first: it then outlives a watcher that is a static object
  // (static objects are destroyed in the reverse order of their creation).
  try
    {
      (void)LumexLogger::get_instance ();
    }
  catch (...)
    {
      delete state; // NOLINT(cppcoreguidelines-owning-memory)
      watcher_state = nullptr;
      throw;
    }
  // Counted before the start, so a caller sees the thread as soon as the
  // constructor returns; the thread itself takes it back when it ends.
  g_running_threads.fetch_add (1);
  try
    {
      state->thread = std::thread (
          [state] ()
            {
              std::unique_lock<std::mutex> lock (state->mutex);
              while (!state->wake.wait_for (lock, state->period, [state] ()
                                              { return state->stop; }))
                {
                  // The check takes the logger's own locks: not under ours.
                  lock.unlock ();
                  try
                    {
                      LumexLogger::get_instance ().force_reread ();
                    }
                  catch (...) // NOLINT(bugprone-empty-catch)
                    {
                      // The singleton could not be created; next period
                    }
                  lock.lock ();
                }
              lock.unlock ();
              g_running_threads.fetch_sub (1);
            });
    }
  catch (...)
    {
      g_running_threads.fetch_sub (1);
      delete state; // NOLINT(cppcoreguidelines-owning-memory)
      watcher_state = nullptr;
      throw;
    }
}

trigger_file_watcher::~trigger_file_watcher () { _stop (); }

trigger_file_watcher::trigger_file_watcher (trigger_file_watcher &&other)
    LUMEX_NOEXCEPT : watcher_state (other.watcher_state)
{
  other.watcher_state = nullptr;
}

trigger_file_watcher &
trigger_file_watcher::operator= (trigger_file_watcher &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      _stop ();
      watcher_state = other.watcher_state;
      other.watcher_state = nullptr;
    }
  return *this;
}

bool
trigger_file_watcher::is_running () const LUMEX_NOEXCEPT
{
  return watcher_state != nullptr;
}

std::chrono::milliseconds
trigger_file_watcher::period () const LUMEX_NOEXCEPT
{
  return watcher_state != nullptr ? watcher_state->period
                                  : std::chrono::milliseconds (0);
}

std::size_t
trigger_file_watcher::running_threads () LUMEX_NOEXCEPT
{
  return g_running_threads.load ();
}

void
trigger_file_watcher::_stop () LUMEX_NOEXCEPT
{
  state_t *const state = watcher_state;
  if (state == nullptr)
    return;
  watcher_state = nullptr;

  try
    {
      {
        std::lock_guard<std::mutex> const lock (state->mutex);
        state->stop = true;
      }
      state->wake.notify_all ();
      state->thread.join ();
    }
  catch (...)
    {
      // join () refuses only when called from the watcher's own thread (the
      // level change callback destroyed the watcher, which the header
      // forbids): the thread sees stop and ends by itself; its state is
      // left to it rather than freed under it.
      if (state->thread.joinable ())
        state->thread.detach ();
      return;
    }
  delete state; // NOLINT(cppcoreguidelines-owning-memory)
}
} // namespace logger
} // namespace logger
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
