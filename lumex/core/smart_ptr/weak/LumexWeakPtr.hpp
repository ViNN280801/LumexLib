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
 * @file LumexWeakPtr.hpp
 * @brief `weak_ptr<T>` and `owner_less`: non-owning observers of an object
 * that a `shared_ptr` owns ([util.smartptr.weak], [util.smartptr.ownerless]).
 * @details A `weak_ptr` shares the control block of the `shared_ptr` it was
 * made from and keeps only the block (the weak ledger), not the object, so
 * `lock ()` can promote it while an owner exists and `expired ()` tells
 * when none does. The promotion is an increment-if-nonzero on the packed
 * strong counter (`split_counter::try_add`): it never revives an object whose
 * last owner has dropped it. A weak pointer made from an aliasing `shared_ptr`
 * remembers the aliased pointer, and `lock ()` returns it.
 *
 * `owner_less<shared_ptr<T> >`, `owner_less<weak_ptr<T> >` and the transparent
 * `owner_less<void>` order both kinds of pointer by their control blocks, so
 * a `std::map` or `std::set` keyed by them is stable for the life of the
 * owners. The default argument `owner_less<>` is `owner_less<void>`.
 */
#ifndef LUMEX_CORE_SMART_PTR_WEAK_WEAK_PTR_HPP
#define LUMEX_CORE_SMART_PTR_WEAK_WEAK_PTR_HPP

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlBase.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrAccess.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrTraits.hpp"
#include "lumex/core/smart_ptr/shared/LumexSharedPtr.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
/**
 * @brief A weak reference to an object owned by `shared_ptr` objects.
 * @tparam T The observed type, as for `shared_ptr<T>`.
 */
template <class T> class weak_ptr
{
public:
  /// `T` without one array extent.
  typedef typename std::remove_extent<T>::type element_type;

  /// An empty weak pointer.
  LUMEX_CONSTEXPR_CTOR
  weak_ptr () LUMEX_NOEXCEPT : ptr_ (nullptr), cb_ (nullptr) {}

  /// Observes what @p r observes.
  weak_ptr (weak_ptr const &r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    if (cb_ != nullptr)
      cb_->add_weak ();
  }

  /// Observes what @p r observes; takes part when `Y *` is compatible with
  /// `T *`.
  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  weak_ptr (weak_ptr<Y> const &r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    if (cb_ != nullptr)
      cb_->add_weak ();
  }

  /// Observes the object @p r owns.
  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  weak_ptr (shared_ptr<Y> const &r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    if (cb_ != nullptr)
      cb_->add_weak ();
  }

  /// Takes what @p r observes; @p r is left empty.
  weak_ptr (weak_ptr &&r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    r.ptr_ = nullptr;
    r.cb_ = nullptr;
  }

  /// Takes what @p r observes; @p r is left empty.
  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  weak_ptr (weak_ptr<Y> &&r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    r.ptr_ = nullptr;
    r.cb_ = nullptr;
  }

  /// Releases the weak reference; deallocates the control block when it was
  /// the last reference of any kind.
  ~weak_ptr ()
  {
    if (cb_ != nullptr)
      cb_->release_weak ();
  }

  weak_ptr &
  operator= (weak_ptr const &r) LUMEX_NOEXCEPT
  {
    weak_ptr (r).swap (*this);
    return *this;
  }

  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  weak_ptr &
  operator= (weak_ptr<Y> const &r) LUMEX_NOEXCEPT
  {
    weak_ptr (r).swap (*this);
    return *this;
  }

  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  weak_ptr &
  operator= (shared_ptr<Y> const &r) LUMEX_NOEXCEPT
  {
    weak_ptr (r).swap (*this);
    return *this;
  }

  weak_ptr &
  operator= (weak_ptr &&r) LUMEX_NOEXCEPT
  {
    weak_ptr (std::move (r)).swap (*this);
    return *this;
  }

  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  weak_ptr &
  operator= (weak_ptr<Y> &&r) LUMEX_NOEXCEPT
  {
    weak_ptr (std::move (r)).swap (*this);
    return *this;
  }

  /// Exchanges what the two observe.
  void
  swap (weak_ptr &r) LUMEX_NOEXCEPT
  {
    element_type *const p = ptr_;
    ptr_ = r.ptr_;
    r.ptr_ = p;
    detail::ctl_base *const c = cb_;
    cb_ = r.cb_;
    r.cb_ = c;
  }

  /// Observes nothing.
  void
  reset () LUMEX_NOEXCEPT
  {
    weak_ptr ().swap (*this);
  }

  /// The number of `shared_ptr` objects that own the object (0 when it is
  /// gone or the weak pointer is empty).
  long
  use_count () const LUMEX_NOEXCEPT
  {
    return cb_ != nullptr ? cb_->use_count () : 0;
  }

  /// True when the object is gone: the strong word is zero, that is no count
  /// and no owner in transit (the same test as `lock ()`).
  bool
  expired () const LUMEX_NOEXCEPT
  {
    return cb_ == nullptr || !cb_->strong_counter ().alive ();
  }

  /**
   * @brief An owner of the observed object, or an empty `shared_ptr` when it
   * is gone.
   * @details Atomic: promotes only while the strong counter is not zero, so
   * it cannot race the last release into a dead object.
   */
  shared_ptr<T>
  lock () const LUMEX_NOEXCEPT
  {
    if (cb_ != nullptr && cb_->try_add_strong ())
      return detail::access::adopt<T> (cb_, ptr_);
    return shared_ptr<T> ();
  }

  /// Owner-based order against a shared pointer.
  template <class Y>
  bool
  owner_before (shared_ptr<Y> const &other) const LUMEX_NOEXCEPT
  {
    return std::less<detail::ctl_base *> () (cb_, other.cb_);
  }

  /// Owner-based order against a weak pointer.
  template <class Y>
  bool
  owner_before (weak_ptr<Y> const &other) const LUMEX_NOEXCEPT
  {
    return std::less<detail::ctl_base *> () (cb_, other.cb_);
  }

private:
  template <class> friend class shared_ptr;
  template <class> friend class weak_ptr;
  friend class detail::access;

  // Takes over a weak count the caller owns (detail::access::adopt_weak).
  weak_ptr (element_type *p, detail::ctl_base *cb,
            detail::access::adopt_tag_t) LUMEX_NOEXCEPT : ptr_ (p),
                                                          cb_ (cb)
  {
  }

  element_type *ptr_;
  detail::ctl_base *cb_;
};

#if defined(__cpp_deduction_guides) && __cplusplus >= 201703L
template <class T> weak_ptr (shared_ptr<T>) -> weak_ptr<T>;
#endif

/// Exchanges two weak pointers.
template <class T>
void
swap (weak_ptr<T> &a, weak_ptr<T> &b) LUMEX_NOEXCEPT
{
  a.swap (b);
}

/// The owner-based order as a function object; see the specializations.
template <class T = void> class owner_less;

/// Orders shared and weak pointers of one type by their owners.
template <class T> class owner_less<shared_ptr<T>>
{
public:
  typedef bool result_type;
  typedef shared_ptr<T> first_argument_type;
  typedef shared_ptr<T> second_argument_type;

  bool
  operator() (shared_ptr<T> const &a,
              shared_ptr<T> const &b) const LUMEX_NOEXCEPT
  {
    return a.owner_before (b);
  }

  bool
  operator() (shared_ptr<T> const &a,
              weak_ptr<T> const &b) const LUMEX_NOEXCEPT
  {
    return a.owner_before (b);
  }

  bool
  operator() (weak_ptr<T> const &a,
              shared_ptr<T> const &b) const LUMEX_NOEXCEPT
  {
    return a.owner_before (b);
  }
};

/// Orders weak and shared pointers of one type by their owners.
template <class T> class owner_less<weak_ptr<T>>
{
public:
  typedef bool result_type;
  typedef weak_ptr<T> first_argument_type;
  typedef weak_ptr<T> second_argument_type;

  bool
  operator() (weak_ptr<T> const &a, weak_ptr<T> const &b) const LUMEX_NOEXCEPT
  {
    return a.owner_before (b);
  }

  bool
  operator() (shared_ptr<T> const &a,
              weak_ptr<T> const &b) const LUMEX_NOEXCEPT
  {
    return a.owner_before (b);
  }

  bool
  operator() (weak_ptr<T> const &a,
              shared_ptr<T> const &b) const LUMEX_NOEXCEPT
  {
    return a.owner_before (b);
  }
};

/// The transparent form: any pair of shared and weak pointers.
template <> class owner_less<void>
{
public:
  typedef void is_transparent;

  template <class T, class U>
  bool
  operator() (T const &a, U const &b) const LUMEX_NOEXCEPT
  {
    return a.owner_before (b);
  }
};
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_WEAK_WEAK_PTR_HPP
