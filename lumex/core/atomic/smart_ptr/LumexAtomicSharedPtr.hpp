/**
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
 * @brief `atomic_shared_ptr<T>`: the interface of the C++20
 * `std::atomic<std::shared_ptr<T>>` for C++11 and later.
 * @details The class wraps `std::atomic<std::shared_ptr<T>>` where the
 * standard library provides it and otherwise keeps an ordinary
 * `std::shared_ptr<T>` behind a lock (see
 * `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp`). The
 * lock-based implementation is a port of the lock-based method of the
 * author's own libc++ implementation of P0718R2 (llvm-project pull request
 * 194215). The member
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
 * The lock-based implementation is not lock-free (`is_always_lock_free` and
 * `is_lock_free ()` are false) and treats every memory order as `seq_cst`.
 *
 * The template is declared only in `lumex::core::atomic::smart_ptr`;
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
/**
 * @brief Atomic `std::shared_ptr<T>`, as `std::atomic<std::shared_ptr<T>>`.
 * @details Every member function may be called concurrently with any other
 * on the same object, except the constructors and the destructor. Two shared
 * pointers are equivalent when they store the same pointer and either share
 * ownership or are both empty; `compare_exchange_*` and `wait` compare that
 * way.
 * @tparam T The element type of the shared pointer (may be incomplete, `void`
 * or `const`; an array type from C++17 on).
 */
template <typename T> class atomic_shared_ptr
{
  using cell_type = Detail::Cell<std::shared_ptr<T>>;

public:
  /// The type of the stored value.
  using value_type = std::shared_ptr<T>;

  /// Whether every object of this type is lock-free.
  static LUMEX_CONSTEXPR bool is_always_lock_free
      = cell_type::is_always_lock_free;

  /**
   * @brief Creates an object that holds an empty shared pointer.
   */
  LUMEX_CONSTEXPR
  atomic_shared_ptr () LUMEX_NOEXCEPT : cell_ () {}

  /**
   * @brief Creates an object that holds an empty shared pointer; usable for
   * constant initialization (LWG 3661).
   */
  LUMEX_CONSTEXPR
  atomic_shared_ptr (std::nullptr_t) LUMEX_NOEXCEPT : cell_ () {}

  /**
   * @brief Creates an object that holds @p desired.
   * @param desired The initial value; its ownership moves into the object.
   */
  atomic_shared_ptr (value_type desired) LUMEX_NOEXCEPT
      : cell_ (std::move (desired))
  {
  }

  atomic_shared_ptr (atomic_shared_ptr const &) = delete;
  void operator= (atomic_shared_ptr const &) = delete;

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
template <typename T>
LUMEX_CONSTEXPR bool atomic_shared_ptr<T>::is_always_lock_free;
#endif

} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SHARED_PTR_HPP
