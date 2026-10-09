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
 * @file LumexAtomicSmartPtrCell.hpp
 * @brief Storage and operations behind `atomic_shared_ptr_lock_based`,
 * `atomic_weak_ptr_lock_based` and the `*_std_backed` class templates: a
 * lock-based cell for every standard library, and a cell over the standard
 * atomic smart pointers.
 * @details The public class templates forward every operation to one of the
 * cells below (the lock-free cell is in
 * `LumexAtomicSmartPtrLockFreeCell.hpp`); the equivalence traits are shared by
 * all of them.
 *
 * `lock_based_cell` is the port of the lock-based method of the author's own
 * libc++ implementation (llvm-project pull request 194215; see
 * `LumexAtomicSmartPtrConfig.hpp`). It keeps an ordinary `std::shared_ptr` or
 * `std::weak_ptr` behind a `bit_lock`. Every operation holds the lock only to
 * move, swap or copy the smart pointer; a previous value is released after the
 * lock, so a deleter that uses the same atomic object does not deadlock. All
 * operations are serialized by the lock and therefore behave as `seq_cst`; the
 * memory order arguments are accepted and not needed.
 *
 * `std_backed_cell` forwards loads, stores, exchanges and comparisons to
 * `std::atomic<std::shared_ptr<T>>` or `std::atomic<std::weak_ptr<T>>`. Its
 * `wait` and `notify_*` do not forward. The standard `wait` must return only
 * after it observes a value that is not equivalent to the old one, and it
 * must observe a change of the stored pointer alone (an aliasing
 * `std::shared_ptr` with the same owner). libstdc++ 13 does neither: its
 * `wait` sleeps on the word that holds the control block pointer, so it
 * returns when another thread merely locks that word and keeps sleeping
 * after a store that changes only the stored pointer. Both cells therefore
 * implement waiting the same way: `notify_*` advances a 32-bit counter and
 * wakes the threads that sleep on it, and `wait` compares the current value
 * with the old one and sleeps on the counter only while they are
 * equivalent.
 *
 * Two smart pointers are equivalent when they store the same pointer and
 * either share ownership or are both empty ([util.smartptr.atomic]).
 * `std::shared_ptr` exposes both parts (`get ()`, `owner_before ()`).
 * `std::weak_ptr` exposes ownership but not its stored pointer, so the
 * lock-based cell, and `wait` of both cells, read the stored pointer of a weak
 * pointer through `lock ()`: two weak pointers with the same owner are
 * equivalent when the object is alive and both lock to the same pointer, or
 * when the object is gone (or both are empty), because the stored pointers
 * can no longer be observed. Only weak pointers made from aliasing shared
 * pointers can differ in that last case.
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CELL_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CELL_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/atomic/sync/LumexAtomicWait.hpp"
#include "lumex/core/atomic/sync/LumexBitLock.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace atomic
{
namespace smart_ptr
{
inline namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
{
namespace Detail
{
/**
 * @brief Equivalence of two smart pointers of one kind.
 * @details `equivalent (current, expected, expected_probe, current_probe)`
 * may store a new owner of the object in @p current_probe; the caller
 * releases it after the cell lock, like every other old value.
 * @tparam Pointer `std::shared_ptr<T>` or `std::weak_ptr<T>`.
 */
template <typename Pointer> struct smart_ptr_traits_t;

/**
 * @brief Equivalence of `std::shared_ptr`: same stored pointer and same
 * owner (or both empty).
 */
template <typename T> struct smart_ptr_traits_t<std::shared_ptr<T>>
{
  /// A shared pointer needs no extra state to be compared.
  struct probe_t
  {
  };

  static probe_t
  probe (std::shared_ptr<T> const &) LUMEX_NOEXCEPT
  {
    return probe_t ();
  }

  static bool
  equivalent (std::shared_ptr<T> const &current,
              std::shared_ptr<T> const &expected, probe_t const &,
              probe_t &) LUMEX_NOEXCEPT
  {
    return current.get () == expected.get ()
           && !current.owner_before (expected)
           && !expected.owner_before (current);
  }
};

/**
 * @brief Equivalence of `std::weak_ptr`: same owner, and the same stored
 * pointer as far as `lock ()` can observe it.
 */
template <typename T> struct smart_ptr_traits_t<std::weak_ptr<T>>
{
  /// The stored pointer of a weak pointer, read through `lock ()`.
  using probe_t = std::shared_ptr<T>;

  static probe_t
  probe (std::weak_ptr<T> const &pointer) LUMEX_NOEXCEPT
  {
    return pointer.lock ();
  }

  static bool
  equivalent (std::weak_ptr<T> const &current,
              std::weak_ptr<T> const &expected, probe_t const &expected_probe,
              probe_t &current_probe) LUMEX_NOEXCEPT
  {
    if (current.owner_before (expected) || expected.owner_before (current))
      return false;
    // Same owner. An empty probe means that the object was gone when it was
    // taken (or that both pointers are empty): the stored pointers can no
    // longer be observed. use_count () rather than operator bool, because a
    // live owner may store a null pointer.
    if (expected_probe.use_count () == 0)
      return true;
    // The probe keeps the object alive, so the current pointer, which has
    // the same owner, locks as well.
    current_probe = current.lock ();
    return current_probe.get () == expected_probe.get ();
  }
};

/**
 * @brief Lock-based atomic cell over an ordinary smart pointer.
 * @tparam Pointer `std::shared_ptr<T>` or `std::weak_ptr<T>`.
 */
template <typename Pointer> class lock_based_cell
{
  using traits_type = smart_ptr_traits_t<Pointer>;
  using probe_type = typename traits_type::probe_t;
  using lock_guard_type = sync::Detail::bit_lock_guard;

public:
  static LUMEX_CONSTEXPR bool is_always_lock_free = false;

  LUMEX_CONSTEXPR
  lock_based_cell () LUMEX_NOEXCEPT : lock_ (), epoch_ (0u), value_ () {}

  explicit lock_based_cell (Pointer desired) LUMEX_NOEXCEPT
      : lock_ (),
        epoch_ (0u),
        value_ (std::move (desired))
  {
  }

  lock_based_cell (lock_based_cell const &) = delete;
  lock_based_cell &operator= (lock_based_cell const &) = delete;

  bool
  is_lock_free () const LUMEX_NOEXCEPT
  {
    return false;
  }

  Pointer
  load (std::memory_order) const LUMEX_NOEXCEPT
  {
    lock_guard_type const guard (lock_);
    return value_;
  }

  void
  store (Pointer desired, std::memory_order) LUMEX_NOEXCEPT
  {
    lock_guard_type const guard (lock_);
    // desired takes the previous value and releases it when the caller's
    // argument is destroyed, after the lock.
    value_.swap (desired);
  }

  Pointer
  exchange (Pointer desired, std::memory_order) LUMEX_NOEXCEPT
  {
    {
      lock_guard_type const guard (lock_);
      value_.swap (desired);
    }
    return desired;
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order, std::memory_order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, std::move (desired));
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, std::move (desired));
  }

  /// Never fails spuriously, so it is the strong form.
  bool
  compare_exchange_weak (Pointer &expected, Pointer desired, std::memory_order,
                         std::memory_order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, std::move (desired));
  }

  /// @copydoc compare_exchange_weak
  bool
  compare_exchange_weak (Pointer &expected, Pointer desired,
                         std::memory_order) LUMEX_NOEXCEPT
  {
    return compare_exchange (expected, std::move (desired));
  }

  void
  wait (Pointer const &old, std::memory_order) const LUMEX_NOEXCEPT
  {
    for (;;)
      {
        // Read the counter before the value: a notification that follows a
        // change made after this comparison then changes the counter that
        // this thread sleeps on.
        std::uint32_t const epoch = epoch_.load (std::memory_order_acquire);
        if (!holds_equivalent (old))
          return;
        sync::Detail::wait_until_changed (epoch_, epoch);
      }
  }

  void
  notify_one () LUMEX_NOEXCEPT
  {
    epoch_.fetch_add (1u, std::memory_order_release);
    sync::Detail::notify_one (epoch_);
  }

  void
  notify_all () LUMEX_NOEXCEPT
  {
    epoch_.fetch_add (1u, std::memory_order_release);
    sync::Detail::notify_all (epoch_);
  }

private:
  bool
  compare_exchange (Pointer &expected, Pointer desired) LUMEX_NOEXCEPT
  {
    // Declared before the guard, so they are released after the lock.
    probe_type const expected_probe = traits_type::probe (expected);
    probe_type current_probe;
    Pointer current;
    bool exchanged = false;
    {
      lock_guard_type const guard (lock_);
      if (traits_type::equivalent (value_, expected, expected_probe,
                                   current_probe))
        {
          value_.swap (desired);
          exchanged = true;
        }
      else
        current = value_;
    }
    if (!exchanged)
      expected = std::move (current);
    return exchanged;
  }

  bool
  holds_equivalent (Pointer const &old) const LUMEX_NOEXCEPT
  {
    probe_type const old_probe = traits_type::probe (old);
    probe_type current_probe;
    lock_guard_type const guard (lock_);
    return traits_type::equivalent (value_, old, old_probe, current_probe);
  }

  sync::Detail::bit_lock lock_;
  std::atomic<std::uint32_t> epoch_;
  Pointer value_;
};

#if __cplusplus < 201703L
template <typename Pointer>
LUMEX_CONSTEXPR bool lock_based_cell<Pointer>::is_always_lock_free;
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED

/**
 * @brief Atomic cell over `std::atomic<std::shared_ptr<T>>` or
 * `std::atomic<std::weak_ptr<T>>`, with a conforming `wait`.
 * @tparam Pointer `std::shared_ptr<T>` or `std::weak_ptr<T>`.
 */
template <typename Pointer> class std_backed_cell
{
  using traits_type = smart_ptr_traits_t<Pointer>;
  using probe_type = typename traits_type::probe_t;

public:
  static LUMEX_CONSTEXPR bool is_always_lock_free
      = std::atomic<Pointer>::is_always_lock_free;

  LUMEX_CONSTEXPR
  std_backed_cell () LUMEX_NOEXCEPT : value_ (), epoch_ (0u) {}

  explicit std_backed_cell (Pointer desired) LUMEX_NOEXCEPT
      : value_ (std::move (desired)),
        epoch_ (0u)
  {
  }

  std_backed_cell (std_backed_cell const &) = delete;
  std_backed_cell &operator= (std_backed_cell const &) = delete;

  bool
  is_lock_free () const LUMEX_NOEXCEPT
  {
    return value_.is_lock_free ();
  }

  Pointer
  load (std::memory_order order) const LUMEX_NOEXCEPT
  {
    return value_.load (order);
  }

  void
  store (Pointer desired, std::memory_order order) LUMEX_NOEXCEPT
  {
    value_.store (std::move (desired), order);
  }

  Pointer
  exchange (Pointer desired, std::memory_order order) LUMEX_NOEXCEPT
  {
    return value_.exchange (std::move (desired), order);
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order success,
                           std::memory_order failure) LUMEX_NOEXCEPT
  {
    return value_.compare_exchange_strong (expected, std::move (desired),
                                           success, failure);
  }

  bool
  compare_exchange_strong (Pointer &expected, Pointer desired,
                           std::memory_order order) LUMEX_NOEXCEPT
  {
    return value_.compare_exchange_strong (expected, std::move (desired),
                                           order);
  }

  bool
  compare_exchange_weak (Pointer &expected, Pointer desired,
                         std::memory_order success,
                         std::memory_order failure) LUMEX_NOEXCEPT
  {
    return value_.compare_exchange_weak (expected, std::move (desired),
                                         success, failure);
  }

  bool
  compare_exchange_weak (Pointer &expected, Pointer desired,
                         std::memory_order order) LUMEX_NOEXCEPT
  {
    return value_.compare_exchange_weak (expected, std::move (desired), order);
  }

  void
  wait (Pointer const &old, std::memory_order order) const LUMEX_NOEXCEPT
  {
    for (;;)
      {
        std::uint32_t const epoch = epoch_.load (std::memory_order_acquire);
        if (!holds_equivalent (old, order))
          return;
        sync::Detail::wait_until_changed (epoch_, epoch);
      }
  }

  void
  notify_one () LUMEX_NOEXCEPT
  {
    epoch_.fetch_add (1u, std::memory_order_release);
    sync::Detail::notify_one (epoch_);
  }

  void
  notify_all () LUMEX_NOEXCEPT
  {
    epoch_.fetch_add (1u, std::memory_order_release);
    sync::Detail::notify_all (epoch_);
  }

private:
  bool
  holds_equivalent (Pointer const &old,
                    std::memory_order order) const LUMEX_NOEXCEPT
  {
    Pointer const current = value_.load (order);
    probe_type const old_probe = traits_type::probe (old);
    probe_type current_probe;
    return traits_type::equivalent (current, old, old_probe, current_probe);
  }

  std::atomic<Pointer> value_;
  std::atomic<std::uint32_t> epoch_;
};

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED

} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CELL_HPP
