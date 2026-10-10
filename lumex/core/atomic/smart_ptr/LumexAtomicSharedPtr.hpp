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
 * @file LumexAtomicSharedPtr.hpp
 * @brief `atomic_shared_ptr<T>` and its engines
 * (`atomic_shared_ptr_lock_free`, `atomic_shared_ptr_lock_based`,
 * `atomic_shared_ptr_std_backed`, and
 * `atomic_shared_ptr_lock_free_split_count` over
 * `lumex::core::smart_ptr::shared_ptr`): the interface of the C++20
 * `std::atomic<std::shared_ptr<T>>` for C++11 and later.
 * @details Every engine is a class template of its own over the same
 * interface; `atomic_shared_ptr<T>` is an alias template of one of them
 * (see `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp`): the
 * lock-free engine where it exists, the lock-based one otherwise, never the
 * wrapper of the standard library's type. The member
 * functions, their default memory orders, `noexcept` and return types follow
 * [util.smartptr.atomic.shared], including LWG 3661 (`constexpr` construction
 * from `nullptr`, so the object can be `constinit`) and LWG 3893
 * (`operator= (nullptr_t)`).
 *
 * `wait (old)` returns once the stored value is not equivalent to @c old (a
 * different stored pointer, or a different owner) and a `notify_one` or
 * `notify_all` has been called after the change. As in the standard,
 * `store`, `exchange` and `compare_exchange_*` do not wake waiting threads by
 * themselves.
 *
 * The lock-based engine is not lock-free (`is_always_lock_free` and
 * `is_lock_free ()` are false) and treats every memory order as `seq_cst`;
 * the lock-free engine reports true for both.
 *
 * The templates are declared only in `lumex::core::atomic::smart_ptr`;
 * nothing is added to the global namespace.
 *
 * @code
 * using lumex::core::atomic::smart_ptr::atomic_shared_ptr;
 *
 * atomic_shared_ptr<Config> current (std::make_shared<Config> ());
 * std::shared_ptr<Config> snapshot = current.load ();   // reader
 * current.store (std::make_shared<Config> (next));      // writer
 * current.notify_all ();
 * @endcode
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SHARED_PTR_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SHARED_PTR_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrCell.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrLockFreeCell.hpp"
#endif
#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrLumexFamily.hpp"
#include "lumex/core/smart_ptr/shared/LumexSharedPtr.hpp"
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
 * @brief Atomic `std::shared_ptr<T>`, as `std::atomic<std::shared_ptr<T>>`.
 * @details Every member function may be called concurrently with any other
 * on the same object, except the constructors and the destructor. Two shared
 * pointers are equivalent when they store the same pointer and either share
 * ownership or are both empty; `compare_exchange_*` and `wait` compare that
 * way.
 * @tparam T The element type of the shared pointer (may be incomplete, `void`
 * or `const`; an array type from C++17 on).
 * @tparam Cell The engine: `lock_based_cell`, `lock_free_cell` or
 * `std_backed_cell` of `std::shared_ptr<T>`.
 */
template <typename T, typename Cell> class basic_atomic_shared_ptr
{
  using cell_type = Cell;

public:
  /// The type of the stored value.
  using value_type = typename cell_type::value_type;

  /// Whether every object of this type is lock-free.
  static LUMEX_CONSTEXPR bool is_always_lock_free
      = cell_type::is_always_lock_free;

  /**
   * @brief Creates an object that holds an empty shared pointer.
   */
  LUMEX_CONSTEXPR
  basic_atomic_shared_ptr () LUMEX_NOEXCEPT : cell_ () {}

  /**
   * @brief Creates an object that holds an empty shared pointer; usable for
   * constant initialization (LWG 3661).
   */
  LUMEX_CONSTEXPR
  basic_atomic_shared_ptr (std::nullptr_t) LUMEX_NOEXCEPT : cell_ () {}

  /**
   * @brief Creates an object that holds @p desired.
   * @param desired The initial value; its ownership moves into the object.
   */
  basic_atomic_shared_ptr (value_type desired) LUMEX_NOEXCEPT
      : cell_ (std::move (desired))
  {
  }

  basic_atomic_shared_ptr (basic_atomic_shared_ptr const &) = delete;
  void operator= (basic_atomic_shared_ptr const &) = delete;

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
   * @brief Returns a copy of the stored value (a new owner).
   * @param order `relaxed`, `consume`, `acquire` or `seq_cst`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "load () returns a new owner of the stored value; discarding it only "
      "adds and drops a reference")
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
   * @brief Stores an empty shared pointer (LWG 3893).
   */
  void
  operator= (std::nullptr_t) LUMEX_NOEXCEPT
  {
    store (nullptr);
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
   * checks again; a wake-up that finds an equivalent value sleeps again.
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
LUMEX_CONSTEXPR bool basic_atomic_shared_ptr<T, Cell>::is_always_lock_free;
#endif
} // namespace Detail

/**
 * @brief Atomic `std::shared_ptr<T>` over a lock (the lock-based engine).
 * @details Keeps an ordinary `std::shared_ptr<T>` behind a two-bit lock. Works
 * with any standard library from C++11 on; not lock-free
 * (`is_always_lock_free` and `is_lock_free ()` are false); every operation
 * behaves as `seq_cst`. The previous value is released after the lock, so a
 * deleter that uses the same object does not deadlock. The members are those
 * of `std::atomic<std::shared_ptr<T>>` (see the file description).
 * @tparam T The element type of the shared pointer.
 */
template <typename T>
class atomic_shared_ptr_lock_based
    : public Detail::basic_atomic_shared_ptr<
          T, Detail::lock_based_cell<std::shared_ptr<T>>>
{
  using base_type = Detail::basic_atomic_shared_ptr<
      T, Detail::lock_based_cell<std::shared_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty shared pointer.
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_based () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds an empty shared pointer (LWG 3661).
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_based (std::nullptr_t) LUMEX_NOEXCEPT : base_type ()
  {
  }

  /// Creates an object that holds @p desired.
  atomic_shared_ptr_lock_based (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};

#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
/**
 * @brief Atomic `std::shared_ptr<T>` that wraps the standard library's
 * `std::atomic<std::shared_ptr<T>>` (explicit opt-in name).
 * @details Loads, stores, exchanges and comparisons are the library's;
 * `wait` and `notify_*` are the library's own epoch-counter implementation
 * (the library's `wait` does not meet the standard's postcondition, see
 * `LumexAtomicSmartPtrCell.hpp`). Declared only when the library has the
 * standard type (`LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED`). No common name
 * resolves to it: `is_lock_free ()` is the library's answer (false in
 * libstdc++ and the MSVC STL).
 * @tparam T The element type of the shared pointer.
 */
template <typename T>
class atomic_shared_ptr_std_backed
    : public Detail::basic_atomic_shared_ptr<
          T, Detail::std_backed_cell<std::shared_ptr<T>>>
{
  using base_type = Detail::basic_atomic_shared_ptr<
      T, Detail::std_backed_cell<std::shared_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty shared pointer.
  LUMEX_CONSTEXPR
  atomic_shared_ptr_std_backed () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds an empty shared pointer (LWG 3661).
  LUMEX_CONSTEXPR
  atomic_shared_ptr_std_backed (std::nullptr_t) LUMEX_NOEXCEPT : base_type ()
  {
  }

  /// Creates an object that holds @p desired.
  atomic_shared_ptr_std_backed (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED

#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
/**
 * @brief Lock-free atomic `std::shared_ptr<T>` (hazard-protected box engine).
 * @details Declared only when the engine exists
 * (`LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE`). The stored value is an immutable
 * heap box behind one `std::atomic` pointer, protected by
 * `core/hazard_pointer`: `load` never waits for another thread, a
 * compare-exchange retries only when another thread made progress, and
 * `is_always_lock_free` and `is_lock_free ()` are true in the sense of the
 * steady state: the first operation of a thread takes a hazard record and
 * every `store`, `exchange` and successful compare-exchange allocates a box,
 * which may block in the allocator. An allocation failure terminates the
 * program (every operation is `noexcept`). See
 * `LumexAtomicSmartPtrLockFreeCell.hpp` for the algorithm and the
 * destruction of replaced values.
 * @tparam T The element type of the shared pointer.
 * @tparam Reclaim When a replaced value is destroyed: `reclaim::immediate`
 * (the default) tries at once, inside the replacing call;
 * `reclaim::deferred` leaves it to the hazard domain.
 */
template <typename T, typename Reclaim = reclaim::immediate>
class atomic_shared_ptr_lock_free
    : public Detail::basic_atomic_shared_ptr<
          T, Detail::lock_free_cell<std::shared_ptr<T>, Reclaim>>
{
  using base_type = Detail::basic_atomic_shared_ptr<
      T, Detail::lock_free_cell<std::shared_ptr<T>, Reclaim>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty shared pointer.
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_free () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds an empty shared pointer (LWG 3661).
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_free (std::nullptr_t) LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds @p desired.
  atomic_shared_ptr_lock_free (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
/**
 * @brief Atomic `lumex::core::smart_ptr::shared_ptr<T>` over a lock (the
 * lock-based engine over the module's own pointer family).
 * @details The lock-based engine of `atomic_shared_ptr_lock_based` with
 * `lumex::core::smart_ptr::shared_ptr<T>` as the value type: not lock-free,
 * every operation behaves as `seq_cst`. Declared where the pointer family is
 * linked (`LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY`); it is what
 * `lumex::core::smart_ptr::atomic_shared_ptr` stands for where the
 * split-count engine does not exist.
 * @tparam T The element type of the shared pointer.
 */
template <typename T>
class atomic_shared_ptr_lock_based_lumex
    : public Detail::basic_atomic_shared_ptr<
          T, Detail::lock_based_cell<::lumex::core::smart_ptr::shared_ptr<T>>>
{
  using base_type = Detail::basic_atomic_shared_ptr<
      T, Detail::lock_based_cell<::lumex::core::smart_ptr::shared_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty shared pointer.
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_based_lumex () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds an empty shared pointer (LWG 3661).
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_based_lumex (std::nullptr_t) LUMEX_NOEXCEPT
      : base_type ()
  {
  }

  /// Creates an object that holds @p desired.
  atomic_shared_ptr_lock_based_lumex (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
/**
 * @brief Lock-free atomic `lumex::core::smart_ptr::shared_ptr<T>` (the
 * split-count engine).
 * @details Declared only where the engine exists
 * (`LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT`: x86-64 with the pointer family
 * linked). The value is one 16-byte word, a control block address, the
 * number of loads in flight and the offset of the stored pointer, updated by
 * a 128-bit compare-and-swap; see
 * `split_count/LumexSplitCountCell.hpp` for the protocol. The value type is
 * the module's own `shared_ptr`, never `std::shared_ptr`. `load` never waits
 * for another thread except past 2^24 - 1 loads of one object in flight; a
 * compare-exchange retries only when another thread made progress;
 * `is_always_lock_free` and `is_lock_free ()` are true in that sense. A value
 * replaced by `store`, `exchange` or a compare-exchange is destroyed in the
 * call unless a load of it is in flight (then the last thread to pay for it
 * destroys it, possibly inside `load`). `use_count ()` of a stored value
 * never reports fewer owners than exist and never waits; it is exact when no
 * operation on an atomic that holds the value is in flight, and can be
 * briefly higher while one is. Modifying operations do not wake `wait ()`.
 * Every operation terminates the program on a CPU without CMPXCHG16B (the
 * first use checks it once).
 * @tparam T The element type of the shared pointer.
 */
template <typename T>
class atomic_shared_ptr_lock_free_split_count
    : public Detail::basic_atomic_shared_ptr<
          T, Detail::split_count_cell<::lumex::core::smart_ptr::shared_ptr<T>>>
{
  using base_type = Detail::basic_atomic_shared_ptr<
      T, Detail::split_count_cell<::lumex::core::smart_ptr::shared_ptr<T>>>;

public:
  /// The type of the stored value.
  using value_type = typename base_type::value_type;

  /// Creates an object that holds an empty shared pointer.
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_free_split_count () LUMEX_NOEXCEPT : base_type () {}

  /// Creates an object that holds an empty shared pointer (LWG 3661).
  LUMEX_CONSTEXPR
  atomic_shared_ptr_lock_free_split_count (std::nullptr_t) LUMEX_NOEXCEPT
      : base_type ()
  {
  }

  /// Creates an object that holds @p desired.
  atomic_shared_ptr_lock_free_split_count (value_type desired) LUMEX_NOEXCEPT
      : base_type (std::move (desired))
  {
  }

  using base_type::operator=;
};
#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

/**
 * @brief The common name: `std::atomic<std::shared_ptr<T>>` from C++11 on.
 * @details An alias template of `atomic_shared_ptr_lock_free<T>` where the
 * lock-free engine exists (and `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is
 * not defined), of `atomic_shared_ptr_lock_based<T>` otherwise; never of the
 * `_std_backed` wrapper. Being an alias template it cannot be forward
 * declared, partially specialized or befriended: name the engine to do that.
 */
#if LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE
template <typename T> using atomic_shared_ptr = atomic_shared_ptr_lock_free<T>;
#else
template <typename T>
using atomic_shared_ptr = atomic_shared_ptr_lock_based<T>;
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
 * @brief The atomic `shared_ptr` of the module's own pointer family:
 * `std::atomic<std::shared_ptr<T>>` for `lumex::core::smart_ptr::shared_ptr`.
 * @details An alias template of
 * `lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free_split_count<T>`
 * where the split-count engine exists (and
 * `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is not defined), of
 * `atomic_shared_ptr_lock_based_lumex<T>` otherwise
 * (`LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT` tells which). It is
 * declared by the atomic module (`lumex/core/atomic/LumexAtomic`), not by the
 * pointer module, which does not depend on it.
 */
#if LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT
template <typename T>
using atomic_shared_ptr = ::lumex::core::atomic::smart_ptr::
    atomic_shared_ptr_lock_free_split_count<T>;
#else
template <typename T>
using atomic_shared_ptr
    = ::lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based_lumex<T>;
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

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SHARED_PTR_HPP
