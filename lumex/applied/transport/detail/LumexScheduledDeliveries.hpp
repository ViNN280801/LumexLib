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
 * @file LumexScheduledDeliveries.hpp
 * @brief The deliveries a scripted mock has scheduled on a timer source
 * (`feed_after`, `deliver_after`).
 * @details Internal to the mocks of `testing/`. Each delivery owns one
 * `timing::timer` (one pending wait per timer) and waits for its delay; when
 * the wait ends in time the owner's `deliver_scheduled (id)` is called on the
 * timer's strand. The wait handler holds the owner only weakly, so a mock that
 * is gone takes its deliveries with it. The class is not thread-safe: the
 * owner guards `add`, `attach`, `remove` and `take` with its own mutex;
 * `start` is static and needs no lock.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_SCHEDULED_DELIVERIES_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_SCHEDULED_DELIVERIES_HPP

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
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
namespace detail
{
/// @brief The deliveries of one mock, each an `Item` waiting on its timer.
/// @details Scheduling takes three steps so that the owner makes the timer
/// and starts the wait outside its lock: `add` (under the lock) reserves the
/// id and keeps the item, `start` (no lock) makes the timer and starts the
/// wait, `attach` (under the lock) hands the timer to the delivery. A wait
/// that ends before `attach` finds the delivery by its id and takes it; the
/// timer `attach` then gets is dropped by the caller.
template <typename Owner, typename Item> class scheduled_deliveries
{
public:
  /// @brief One delivery: the wait and what to deliver.
  struct entry_t
  {
    explicit entry_t (Item entry_item) : item (std::move (entry_item)) {}

    timing::timer timer;
    Item item;
  };

  using entries_t = std::map<std::uint64_t, entry_t>;

  scheduled_deliveries () : last_id_ (0) {}

  /// @brief Keeps `item` for a delivery and returns its id (under the
  /// owner's lock).
  std::uint64_t
  add (Item item)
  {
    std::uint64_t const id = ++last_id_;
    entries_.insert (std::make_pair (id, entry_t (std::move (item))));
    return id;
  }

  /// @brief Makes the timer for `executor` (the strand of the owner) and
  /// starts the wait of `delay` for the delivery `id` (no lock held).
  static timing::timer
  start (std::shared_ptr<Owner> const &owner,
         timing::timer_source const &timers, executor_type const &executor,
         std::chrono::milliseconds delay, std::uint64_t id)
  {
    timing::timer timer = timers.make_timer (executor);
    timer.async_wait_for (delay, fire_handler_t (owner, id));
    return timer;
  }

  /// @brief Gives the started timer to the delivery `id` (under the owner's
  /// lock); the timer stays with the caller when the delivery is gone.
  void
  attach (std::uint64_t id, timing::timer &timer)
  {
    typename entries_t::iterator const it = entries_.find (id);
    if (it != entries_.end ())
      it->second.timer = std::move (timer);
  }

  /// @brief Forgets the delivery `id` (its wait never started).
  void
  remove (std::uint64_t id)
  {
    entries_.erase (id);
  }

  /// @brief Takes the delivery `id` out: its item, and its timer so that the
  /// caller destroys it outside the owner's lock. False when it is gone.
  bool
  take (std::uint64_t id, Item &item, timing::timer &timer)
  {
    typename entries_t::iterator const it = entries_.find (id);
    if (it == entries_.end ())
      return false;
    item = std::move (it->second.item);
    timer = std::move (it->second.timer);
    entries_.erase (it);
    return true;
  }

private:
  // The end of a wait: delivered when the time came, ignored when the wait
  // was cancelled (the clock was destroyed).
  struct fire_handler_t
  {
    fire_handler_t (std::shared_ptr<Owner> const &owner_ptr,
                    std::uint64_t entry_id)
        : owner (owner_ptr), id (entry_id)
    {
    }

    void
    operator() (std::error_code const &error) const
    {
      if (error)
        return;
      std::shared_ptr<Owner> const alive = owner.lock ();
      if (alive)
        alive->deliver_scheduled (id);
    }

    std::weak_ptr<Owner> owner;
    std::uint64_t id;
  };

  entries_t entries_;
  std::uint64_t last_id_;
};
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_SCHEDULED_DELIVERIES_HPP
