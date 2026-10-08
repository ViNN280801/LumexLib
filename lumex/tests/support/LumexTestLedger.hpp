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

/**
 * @file LumexTestLedger.hpp
 * @brief Construction and destruction ledger of numbered test objects.
 * @details `ObjectLedger` hands out an id at construction and counts the
 * destructions by id, so a test proves that every object that was made was
 * destroyed exactly once: a missing destruction is a leaked reference, a
 * second one a double release. Unlike a bare live counter it names the
 * offending ids and cannot be fooled by one leak cancelling one extra
 * release. `LedgerEntry` is the member a test object embeds: it registers in
 * its constructor, unregisters in its destructor and keeps a canary that is
 * `alive_canary` while the object lives and `dead_canary` after its
 * destructor ran, so a reader that touches a destroyed (or recycled and
 * poisoned) object sees a canary that is not `alive_canary` and reports it.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_LEDGER_HPP
#define LUMEX_TESTS_SUPPORT_TEST_LEDGER_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace lumex_test
{
/// Canary of a live object.
enum : std::uint32_t
{
  alive_canary = 0xA11CE5E7u,
  dead_canary = 0xDEADC0DEu
};

class ObjectLedger
{
public:
  /// A ledger for at most @p capacity objects (further ones are counted in
  /// `overflow ()` and make `balanced ()` false).
  explicit ObjectLedger (std::size_t capacity)
      : destroyed_ (new std::atomic<int>[capacity == 0 ? 1 : capacity]),
        capacity_ (capacity == 0 ? 1 : capacity), constructed_ (0),
        overflow_ (0), events_ (0)
  {
    for (std::size_t i = 0; i < capacity_; ++i)
      destroyed_[i].store (0, std::memory_order_relaxed);
  }

  ObjectLedger (ObjectLedger const &) = delete;
  ObjectLedger &operator= (ObjectLedger const &) = delete;

  /// Registers a construction and returns the id of the new object.
  std::size_t
  construct ()
  {
    std::size_t const id
        = constructed_.fetch_add (1, std::memory_order_relaxed);
    if (id >= capacity_)
      overflow_.fetch_add (1, std::memory_order_relaxed);
    return id;
  }

  /// Registers the destruction of object @p id.
  void
  destroy (std::size_t id)
  {
    events_.fetch_add (1, std::memory_order_relaxed);
    if (id < capacity_)
      destroyed_[id].fetch_add (1, std::memory_order_relaxed);
  }

  /// Objects constructed so far.
  std::size_t
  constructed () const
  {
    return constructed_.load (std::memory_order_relaxed);
  }

  /// Destructions recorded so far (a double one counts twice).
  std::size_t
  destructions () const
  {
    return events_.load (std::memory_order_relaxed);
  }

  /// Objects constructed and not yet destroyed (never negative).
  std::size_t
  alive () const
  {
    std::size_t const made = constructed ();
    std::size_t const gone = destructions ();
    return gone >= made ? 0 : made - gone;
  }

  /// Ids destroyed more than once.
  std::size_t
  destroyed_twice () const
  {
    std::size_t count = 0;
    std::size_t const limit = bounded (constructed ());
    for (std::size_t i = 0; i < limit; ++i)
      if (destroyed_[i].load (std::memory_order_relaxed) > 1)
        ++count;
    return count;
  }

  /// Ids constructed and never destroyed.
  std::size_t
  never_destroyed () const
  {
    std::size_t count = 0;
    std::size_t const limit = bounded (constructed ());
    for (std::size_t i = 0; i < limit; ++i)
      if (destroyed_[i].load (std::memory_order_relaxed) == 0)
        ++count;
    return count;
  }

  /// Constructions beyond the capacity.
  std::size_t
  overflow () const
  {
    return overflow_.load (std::memory_order_relaxed);
  }

  /// True when every constructed object was destroyed exactly once.
  bool
  balanced () const
  {
    // Every id destroyed at least once, no destruction without a
    // construction (which a double destruction would be): both ids and
    // events are counted.
    return overflow () == 0 && never_destroyed () == 0
           && destructions () == constructed ();
  }

  /// One-line summary for failure messages.
  std::string
  report () const
  {
    return "constructed=" + std::to_string (constructed ())
           + " destructions=" + std::to_string (destructions ())
           + " alive=" + std::to_string (alive ())
           + " destroyed_twice=" + std::to_string (destroyed_twice ())
           + " never_destroyed=" + std::to_string (never_destroyed ())
           + " overflow=" + std::to_string (overflow ());
  }

private:
  std::size_t
  bounded (std::size_t n) const
  {
    return n < capacity_ ? n : capacity_;
  }

  std::unique_ptr<std::atomic<int>[]> destroyed_;
  std::size_t capacity_;
  std::atomic<std::size_t> constructed_;
  std::atomic<std::size_t> overflow_;
  std::atomic<std::size_t> events_;
};

/// Member of a test object: registers it in a ledger and carries the canary.
class LedgerEntry
{
public:
  explicit LedgerEntry (ObjectLedger &ledger)
      : ledger_ (&ledger), id_ (ledger.construct ()), canary_ (alive_canary)
  {
  }

  ~LedgerEntry ()
  {
    canary_ = dead_canary;
    ledger_->destroy (id_);
  }

  LedgerEntry (LedgerEntry const &) = delete;
  LedgerEntry &operator= (LedgerEntry const &) = delete;

  /// True while the object is alive and its memory was not overwritten.
  bool
  intact () const
  {
    return canary_ == alive_canary;
  }

  /// The id the ledger gave this object.
  std::size_t
  id () const
  {
    return id_;
  }

private:
  ObjectLedger *ledger_;
  std::size_t id_;
  volatile std::uint32_t canary_;
};
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_LEDGER_HPP
