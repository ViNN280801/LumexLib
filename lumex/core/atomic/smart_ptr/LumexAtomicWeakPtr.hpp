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
 * @file LumexAtomicWeakPtr.hpp
 * @brief `atomic_weak_ptr<T>` and its engines (`atomic_weak_ptr_lock_free`,
 * `atomic_weak_ptr_lock_based`, `atomic_weak_ptr_std_backed`, and
 * `atomic_weak_ptr_lock_free_split_count` over
 * `lumex::core::smart_ptr::weak_ptr`): the interface
 * of the C++20 `std::atomic<std::weak_ptr<T>>` for C++11 and later.
 * @details Every engine is a class template of its own over the same
 * interface; `atomic_weak_ptr<T>` is an alias template of one of them (see
 * `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp`): the
 * lock-free engine where it exists, the lock-based one otherwise, never the
 * wrapper of the standard library's type. The member
 * functions follow [util.smartptr.atomic.weak]; unlike `atomic_shared_ptr`
 * there is no construction from or assignment of `nullptr`.
 *
 * Two weak pointers are equivalent when they store the same pointer and
 * either share ownership or are both empty. `std::weak_ptr` does not expose
 * its stored pointer, so the lock-based and lock-free engines, and `wait` in
 * every engine, read it through `lock ()`. When the object is already
 * gone, two weak pointers with the same owner count as equivalent even if
 * they were made from aliasing shared pointers to different subobjects; the
 * standard types compare those stored pointers.
 *
 * The templates are declared only in `lumex::core::atomic::smart_ptr`;
 * nothing is added to the global namespace.
 *
 * @code
 * using lumex::core::atomic::smart_ptr::atomic_weak_ptr;
 *
 * atomic_weak_ptr<Session> active;
 * active.store (session);                                // keeps no owner
 * if (std::shared_ptr<Session> s = active.load ().lock ())
 *   s->ping ();
 * @endcode
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_WEAK_PTR_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_WEAK_PTR_HPP

#include <atomic>
#include <memory>
#include <utility>

#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrCell.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrLockFreeCell.hpp"
#endif
#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrLumexFamily.hpp"
#include "lumex/core/smart_ptr/weak/LumexWeakPtr.hpp"
#endif
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
#include "lumex/core/atomic/smart_ptr/split_count/LumexSplitCountCell.hpp"
#endif
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
// diagnose_if (the memory order checks) is a Clang extension.
#pragma clang diagnostic ignored "-Wgcc-compat"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
// operator= returns void, as the standard specifies.
#pragma GCC diagnostic ignored "-Weffc++"
#endif

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
 * @brief Atomic `std::weak_ptr<T>`, as `std::atomic<std::weak_ptr<T>>`.
 * @details Every member function may be called concurrently with any other
 * on the same object, except the constructors and the destructor. The object
 * holds a weak reference only: it keeps the control block, not the managed
 * object, alive.
 * @tparam T The element type of the weak pointer.
 * @tparam Cell The engine: `lock_based_cell`, `lock_free_cell` or
 * `std_backed_cell` of `std::weak_ptr<T>`.
 */
template <typename T, typename Cell> class basic_atomic_weak_ptr
{
  using cell_type = Cell;

public:
  /// The type of the stored value.
  using value_type = typename cell_type::value_type;

  /// Whether every object of this type is lock-free.
  static LUMEX_CONSTEXPR bool is_always_lock_free
      = cell_type::is_always_lock_free;

  /**
   * @brief Creates an object that holds an empty weak pointer.
   */
  LUMEX_CONSTEXPR
  basic_atomic_weak_ptr () LUMEX_NOEXCEPT : cell_ () {}

  /**
   * @brief Creates an object that holds @p desired.
   * @param desired The initial value.
   */
  basic_atomic_weak_ptr (value_type desired) LUMEX_NOEXCEPT
      : cell_ (std::move (desired))
  {
  }

  basic_atomic_weak_ptr (basic_atomic_weak_ptr const &) = delete;
  void operator= (basic_atomic_weak_ptr const &) = delete;

  /**
   * @brief Tells whether the operations on this object are lock-free.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "is_lock_free () only reports a property and has no other effect")
  bool
  is_lock_free () const LUMEX_NOEXCEPT
  {
    return cell_.is_lock_free ();
  }

  /**
   * @brief Replaces the stored value with @p desired.
   * @param desired The new value.
   * @param order `relaxed`, `release` or `seq_cst`.
   */
  void
  store (value_type desired,
         std::memory_order order = std::memory_order_seq_cst) LUMEX_NOEXCEPT
      LUMEX_ATOMIC_SMART_PTR_CHECK_STORE_ORDER (order)
  {
    cell_.store (std::move (desired), order);
  }

  /**
   * @brief Returns a copy of the stored value.
   * @param order `relaxed`, `consume`, `acquire` or `seq_cst`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "load () returns a copy of the stored weak pointer; discarding it only "
      "adds and drops a weak reference")
  value_type
  load (std::memory_order order
        = std::memory_order_seq_cst) const LUMEX_NOEXCEPT
      LUMEX_ATOMIC_SMART_PTR_CHECK_LOAD_ORDER (order)
  {
    return cell_.load (order);
  }

  /**
   * @brief Returns `load ()`.
   */
  operator value_type () const LUMEX_NOEXCEPT { return load (); }

  /**
   * @brief Same as `store (desired)`.
   */
  void
  operator= (value_type desired) LUMEX_NOEXCEPT
  {
    store (std::move (desired));
  }

  /**
   * @brief Replaces the stored value with @p desired and returns the
   * previous one.
   * @param desired The new value.
   * @param order Any memory order.
   */
  value_type
  exchange (value_type desired,
            std::memory_order order = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    return cell_.exchange (std::move (desired), order);
  }

  /**
   * @brief Stores @p desired if the stored value is equivalent to
   * @p expected, otherwise copies the stored value into @p expected.
   * @details The weak form may fail although the values are equivalent; the
   * lock-based implementation never does.
   * @param expected The value the caller expects; receives the stored value
   * on failure.
   * @param desired The value to store on success.
   * @param success Order on success (any).
   * @param failure Order on failure: neither `release` nor `acq_rel`.
   * @return Whether @p desired was stored.
   */
  bool
  compare_exchange_weak (value_type &expected, value_type desired,
                         std::memory_order success,
                         std::memory_order failure) LUMEX_NOEXCEPT
      LUMEX_ATOMIC_SMART_PTR_CHECK_FAILURE_ORDER (failure)
  {
    return cell_.compare_exchange_weak (expected, std::move (desired), success,
                                        failure);
  }

  /**
   * @brief Stores @p desired if the stored value is equivalent to
   * @p expected, otherwise copies the stored value into @p expected.
   * @details Never fails while the values are equivalent.
   * @param expected The value the caller expects; receives the stored value
   * on failure.
   * @param desired The value to store on success.
   * @param success Order on success (any).
   * @param failure Order on failure: neither `release` nor `acq_rel`.
   * @return Whether @p desired was stored.
   */
  bool
  compare_exchange_strong (value_type &expected, value_type desired,
                           std::memory_order success,
                           std::memory_order failure) LUMEX_NOEXCEPT
      LUMEX_ATOMIC_SMART_PTR_CHECK_FAILURE_ORDER (failure)
  {
    return cell_.compare_exchange_strong (expected, std::move (desired),
                                          success, failure);
  }

  /**
   * @brief As the four-argument form, with @p order on success and the
   * matching order on failure (`acq_rel` becomes `acquire`, `release`
   * becomes `relaxed`).
   */
  bool
  compare_exchange_weak (value_type &expected, value_type desired,
                         std::memory_order order
                         = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    return cell_.compare_exchange_weak (expected, std::move (desired), order);
  }

  /// @copydoc compare_exchange_weak(value_type&,value_type,std::memory_order)
  bool
  compare_exchange_strong (value_type &expected, value_type desired,
                           std::memory_order order
                           = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    return cell_.compare_exchange_strong (expected, std::move (desired),
                                          order);
  }

  /**
   * @brief Blocks while the stored value is equivalent to @p old.
   * @details Returns at once if the stored value is not equivalent to
   * @p old. Otherwise sleeps until `notify_one` or `notify_all` wakes it and
   * checks again. The comparison locks both weak pointers for a moment; it
   * keeps no owner while the thread sleeps.
   * @param old The value the caller last saw.
   * @param order `relaxed`, `consume`, `acquire` or `seq_cst`.
   */
  void
  wait (value_type old, std::memory_order order
                        = std::memory_order_seq_cst) const LUMEX_NOEXCEPT
      LUMEX_ATOMIC_SMART_PTR_CHECK_LOAD_ORDER (order)
  {
    cell_.wait (old, order);
  }

  /**
   * @brief Wakes at least one thread blocked in `wait` on this object.
   */
  void
  notify_one () LUMEX_NOEXCEPT
  {
    cell_.notify_one ();
  }

  /**
   * @brief Wakes every thread blocked in `wait` on this object.
   */
  void
  notify_all () LUMEX_NOEXCEPT
  {
    cell_.notify_all ();
  }

private:
  cell_type cell_;
};

#if __cplusplus < 201703L
template <typename T, typename Cell>
LUMEX_CONSTEXPR bool basic_atomic_weak_ptr<T, Cell>::is_always_lock_free;
#endif
} // namespace Detail

/**
 * @brief Atomic `std::weak_ptr<T>` over a lock (the lock-based engine).
 * @details Keeps an ordinary `std::weak_ptr<T>` behind a two-bit lock. Works
 * with any standard library from C++11 on; not lock-free
 * (`is_always_lock_free` and `is_lock_free ()` are false). The members are
 * those of `std::atomic<std::weak_ptr<T>>` (see the file description).
 * @tparam T The element type of the weak pointer.
 */
template <typename T>
class atomic_weak_ptr_lock_based
    : public Detail::basic_atomic_weak_ptr<
          T, Detail::lock_based_cell<std::weak_ptr<T>>>
{
  using base_type = Detail::basic_atomic_weak_ptr<
      T, Detail::lock_based_cell<std::weak_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty weak pointer.
  LUMEX_CONSTEXPR
  atomic_weak_ptr_lock_based () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds @p desired.
  atomic_weak_ptr_lock_based (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};

#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
/**
 * @brief Atomic `std::weak_ptr<T>` that wraps the standard library's
 * `std::atomic<std::weak_ptr<T>>` (explicit opt-in name).
 * @details See `atomic_shared_ptr_std_backed`. Declared only when the library
 * has the standard type (`LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED`); no common
 * name resolves to it.
 * @tparam T The element type of the weak pointer.
 */
template <typename T>
class atomic_weak_ptr_std_backed
    : public Detail::basic_atomic_weak_ptr<
          T, Detail::std_backed_cell<std::weak_ptr<T>>>
{
  using base_type = Detail::basic_atomic_weak_ptr<
      T, Detail::std_backed_cell<std::weak_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty weak pointer.
  LUMEX_CONSTEXPR
  atomic_weak_ptr_std_backed () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds @p desired.
  atomic_weak_ptr_std_backed (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED

#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
/**
 * @brief Lock-free atomic `std::weak_ptr<T>` (hazard-protected box engine).
 * @details See `atomic_shared_ptr_lock_free`. Declared only when the engine
 * exists (`LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE`).
 * @tparam T The element type of the weak pointer.
 * @tparam Reclaim When a replaced value is destroyed: `reclaim::immediate`
 * (the default) or `reclaim::deferred`.
 */
template <typename T, typename Reclaim = reclaim::immediate>
class atomic_weak_ptr_lock_free
    : public Detail::basic_atomic_weak_ptr<
          T, Detail::lock_free_cell<std::weak_ptr<T>, Reclaim>>
{
  using base_type = Detail::basic_atomic_weak_ptr<
      T, Detail::lock_free_cell<std::weak_ptr<T>, Reclaim>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty weak pointer.
  LUMEX_CONSTEXPR
  atomic_weak_ptr_lock_free () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds @p desired.
  atomic_weak_ptr_lock_free (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
/**
 * @brief Atomic `lumex::core::smart_ptr::weak_ptr<T>` over a lock (the
 * lock-based engine over the module's own pointer family).
 * @details See `atomic_shared_ptr_lock_based_lumex`. The equivalence of two
 * weak pointers compares their stored pointers directly (the family exposes
 * them), so it is exact also for expired aliases.
 * @tparam T The element type of the weak pointer.
 */
template <typename T>
class atomic_weak_ptr_lock_based_lumex
    : public Detail::basic_atomic_weak_ptr<
          T, Detail::lock_based_cell<::lumex::core::smart_ptr::weak_ptr<T>>>
{
  using base_type = Detail::basic_atomic_weak_ptr<
      T, Detail::lock_based_cell<::lumex::core::smart_ptr::weak_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty weak pointer.
  LUMEX_CONSTEXPR
  atomic_weak_ptr_lock_based_lumex () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds @p desired.
  atomic_weak_ptr_lock_based_lumex (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
/**
 * @brief Lock-free atomic `lumex::core::smart_ptr::weak_ptr<T>` (the
 * split-count engine).
 * @details See `atomic_shared_ptr_lock_free_split_count`: the same engine on
 * the weak ledger of the control block. The object holds one weak reference;
 * a load ticks the word, takes a weak reference and settles. Weak loads do
 * not change `use_count ()` of the owners. Declared only where the engine
 * exists (`LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT`).
 * @tparam T The element type of the weak pointer.
 */
template <typename T>
class atomic_weak_ptr_lock_free_split_count
    : public Detail::basic_atomic_weak_ptr<
          T, Detail::split_count_cell<::lumex::core::smart_ptr::weak_ptr<T>>>
{
  using base_type = Detail::basic_atomic_weak_ptr<
      T, Detail::split_count_cell<::lumex::core::smart_ptr::weak_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty weak pointer.
  LUMEX_CONSTEXPR
  atomic_weak_ptr_lock_free_split_count () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds @p desired.
  atomic_weak_ptr_lock_free_split_count (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

/**
 * @brief The common name: `std::atomic<std::weak_ptr<T>>` from C++11 on.
 * @details An alias template of `atomic_weak_ptr_lock_free<T>` where the
 * lock-free engine exists (and `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is
 * not defined), of `atomic_weak_ptr_lock_based<T>` otherwise; never of the
 * `_std_backed` wrapper. Being an alias template it cannot be forward
 * declared, partially specialized or befriended: name the engine to do that.
 */
#if LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE
template <typename T> using atomic_weak_ptr = atomic_weak_ptr_lock_free<T>;
#else
template <typename T> using atomic_weak_ptr = atomic_weak_ptr_lock_based<T>;
#endif

} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
namespace lumex
{
namespace core
{
namespace smart_ptr
{
/**
 * @brief The atomic `weak_ptr` of the module's own pointer family.
 * @details `atomic_weak_ptr_lock_free_split_count<T>` where
 * `LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT` is 1,
 * `atomic_weak_ptr_lock_based_lumex<T>` otherwise (see
 * `lumex::core::smart_ptr::atomic_shared_ptr`).
 */
#if LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT
template <typename T>
using atomic_weak_ptr
    = ::lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_free_split_count<
        T>;
#else
template <typename T>
using atomic_weak_ptr
    = ::lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_based_lumex<T>;
#endif
} // namespace smart_ptr
} // namespace core
} // namespace lumex
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_WEAK_PTR_HPP
