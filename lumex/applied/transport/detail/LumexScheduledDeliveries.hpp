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
 * owner guards it with its own mutex.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_SCHEDULED_DELIVERIES_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_SCHEDULED_DELIVERIES_HPP

#include <chrono>
#include <cstdint>
#include <list>
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
template <typename Owner, typename Item> class scheduled_deliveries
{
public:
  /// @brief One delivery: the wait and what to deliver.
  struct entry
  {
    entry (std::uint64_t entry_id, timing::timer entry_timer, Item entry_item)
        : id (entry_id), timer (std::move (entry_timer)),
          item (std::move (entry_item))
    {
    }

    std::uint64_t id;
    timing::timer timer;
    Item item;
  };

  using entries_t = std::list<entry>;

  scheduled_deliveries () : last_id_ (0) {}

  /// @brief Starts the wait of `delay` on `timers` (the timer is made for
  /// `executor`, the strand of the owner).
  void
  schedule (std::shared_ptr<Owner> const &owner,
            timing::timer_source const &timers, executor_type const &executor,
            std::chrono::milliseconds delay, Item item)
  {
    std::uint64_t const id = ++last_id_;
    entries_.push_back (
        entry (id, timers.make_timer (executor), std::move (item)));
    try
      {
        entries_.back ().timer.async_wait_for (delay,
                                               fire_handler (owner, id));
      }
    catch (...)
      {
        entries_.pop_back ();
        throw;
      }
  }

  /// @brief Takes the delivery `id` out: its item, and its timer so that the
  /// caller destroys it outside the owner's lock. False when it is gone.
  bool
  take (std::uint64_t id, Item &item, timing::timer &timer)
  {
    for (typename entries_t::iterator it = entries_.begin ();
         it != entries_.end (); ++it)
      {
        if (it->id != id)
          continue;
        item = std::move (it->item);
        timer = std::move (it->timer);
        entries_.erase (it);
        return true;
      }
    return false;
  }

private:
  // The end of a wait: delivered when the time came, ignored when the wait
  // was cancelled (the timer died with its delivery).
  struct fire_handler
  {
    fire_handler (std::shared_ptr<Owner> const &owner_ptr,
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
