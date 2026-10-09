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
 * @file LumexSmartPtrAccess.hpp
 * @brief Forward declarations of the pointer classes and `detail::access`,
 * the friend that gives the split-count engine and the helpers of the module
 * the raw parts of a pointer.
 * @details `access` is the documented `detail` interface of the family. It is
 * a friend of `shared_ptr`, `weak_ptr` and `enable_shared_from_this`; nothing
 * else in the module reads `ptr_` and `cb_` directly. Its functions:
 *
 * - `control (p)` returns the control block of a `shared_ptr` or `weak_ptr`
 *   (null for an empty pointer, also for an alias of an empty owner).
 * - `adopt<T> (cb, ptr)` builds a `shared_ptr<T>` from a block and a stored
 *   pointer and takes over ONE strong count that the caller already owns;
 *   `adopt_weak<T> (cb, ptr)` does the same for a weak count of a
 *   `weak_ptr<T>`. Neither touches the counters.
 * - `detach (p, ptr)` leaves @p p empty WITHOUT dropping its count, writes the
 *   stored pointer to @p ptr and returns the block: the count moves to the
 *   caller (the hand-over of the engine's `exchange`). `detach_weak` is the
 *   weak twin.
 * - `weak_count (p)` is the number of weak pointers of the block.
 *
 * The enable_shared_from_this support lives here too: `esft_dispatch<Y>`
 * finds out whether `Y` has an accessible, unambiguous base
 * `enable_shared_from_this<U>` and, if so, assigns the weak reference of that
 * base once ([util.smartptr.shared.const]); `is_std_esft_only<Y>` tells a
 * class that has `std::enable_shared_from_this` and no base of the module's
 * own, which `shared_ptr (Y *)` rejects (see
 * `LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS`).
 */
#ifndef LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_ACCESS_HPP
#define LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_ACCESS_HPP

#include <memory>
#include <type_traits>
#include <utility>

#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlBase.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
template <class T> class shared_ptr;
template <class T> class weak_ptr;
template <class T> class enable_shared_from_this;

namespace detail
{
/// Result of the probe when a class has no `enable_shared_from_this` base.
struct esft_none_t
{
};

/// Result of the probe: the class has a base `enable_shared_from_this<U>`.
template <class U> struct esft_found_t
{
  typedef U type;
};

// Declared, never defined. The first overload deduces U from the (single)
// base enable_shared_from_this<U> of Y and takes part only when that base is
// accessible and unambiguous: the third parameter fails the substitution
// otherwise, which removes the overload instead of making the program
// ill-formed (an access violation in a call is not a SFINAE error, but a
// failed substitution is). The ellipsis overload answers "none".
template <class U, class Y>
esft_found_t<U>
esft_probe (enable_shared_from_this<U> const volatile *, Y *,
            typename std::enable_if<
                std::is_convertible<
                    Y *, enable_shared_from_this<U> const volatile *>::value,
                int>::type
            = 0);
esft_none_t esft_probe (...);

template <class U, class Y>
std::true_type std_esft_probe (
    std::enable_shared_from_this<U> const volatile *, Y *,
    typename std::enable_if<
        std::is_convertible<
            Y *, std::enable_shared_from_this<U> const volatile *>::value,
        int>::type
    = 0);
std::false_type std_esft_probe (...);

/// The probe result for `Y`: `esft_found_t<U>` or `esft_none_t`.
template <class Y> struct esft_probe_result
{
  typedef decltype (esft_probe (static_cast<Y *> (nullptr),
                                static_cast<Y *> (nullptr))) type;
};

/// True when `Y` derives from `std::enable_shared_from_this` and has no base
/// `enable_shared_from_this` of this module.
template <class Y> struct is_std_esft_only
{
  static bool const value
      = decltype (std_esft_probe (static_cast<Y *> (nullptr),
                                  static_cast<Y *> (nullptr)))::value
        && std::is_same<typename esft_probe_result<Y>::type,
                        esft_none_t>::value;
};

/**
 * @brief Rejects, with a `static_assert`, a class that derives from
 * `std::enable_shared_from_this` and not from the module's own base.
 * @details Called by every operation that creates the owner of a new object
 * (`shared_ptr (Y *)` and its variants, `make_shared`, `allocate_shared`):
 * the standard base could not be set, so `shared_from_this ()` of it would
 * throw later. Turned off by
 * `LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS`.
 */
template <class Y>
void
check_std_enable_shared_from_this () LUMEX_NOEXCEPT
{
#if !defined(LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS)
  static_assert (!is_std_esft_only<Y>::value,
                 "smart_ptr: this class derives from "
                 "std::enable_shared_from_this, which this shared_ptr cannot "
                 "set; derive from "
                 "lumex::core::smart_ptr::enable_shared_from_this instead (or "
                 "define LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS)");
#endif
}

class access;

/**
 * @brief Assigns the weak reference of the `enable_shared_from_this` base of
 * `Y`, if it has one.
 * @details The primary template does nothing (no accessible, unambiguous
 * base); the specialization for a found base sets the weak reference.
 */
template <class Y, class Probe = typename esft_probe_result<Y>::type>
struct esft_dispatch
{
  template <class X>
  static void
  apply (shared_ptr<X> const &, Y *) LUMEX_NOEXCEPT
  {
  }
};

template <class Y, class U> struct esft_dispatch<Y, esft_found_t<U>>
{
  template <class X> static void apply (shared_ptr<X> const &owner, Y *p);
};

/**
 * @brief The friend of the pointer classes (see the file text).
 */
class access
{
public:
  access () = delete;

  /// The control block of a shared pointer (null when it has none).
  template <class T>
  static ctl_base *
  control (shared_ptr<T> const &p) LUMEX_NOEXCEPT
  {
    return p.cb_;
  }

  /// The control block of a weak pointer (null when it has none).
  template <class T>
  static ctl_base *
  control (weak_ptr<T> const &p) LUMEX_NOEXCEPT
  {
    return p.cb_;
  }

  /// Builds a shared pointer from a block and a stored pointer, taking over
  /// one strong count the caller owns.
  template <class T>
  static shared_ptr<T>
  adopt (ctl_base *cb,
         typename shared_ptr<T>::element_type *ptr) LUMEX_NOEXCEPT
  {
    return shared_ptr<T> (ptr, cb, adopt_tag_t ());
  }

  /// Builds a weak pointer from a block and a stored pointer, taking over
  /// one weak count the caller owns.
  template <class T>
  static weak_ptr<T>
  adopt_weak (ctl_base *cb,
              typename weak_ptr<T>::element_type *ptr) LUMEX_NOEXCEPT
  {
    return weak_ptr<T> (ptr, cb, adopt_tag_t ());
  }

  /// Empties @p p without dropping its count; the count and the stored
  /// pointer (written to @p ptr) go to the caller.
  template <class T>
  static ctl_base *
  detach (shared_ptr<T> &p,
          typename shared_ptr<T>::element_type *&ptr) LUMEX_NOEXCEPT
  {
    ptr = p.ptr_;
    ctl_base *const cb = p.cb_;
    p.ptr_ = nullptr;
    p.cb_ = nullptr;
    return cb;
  }

  /// Empties @p p without dropping its weak count (see `detach`).
  template <class T>
  static ctl_base *
  detach_weak (weak_ptr<T> &p,
               typename weak_ptr<T>::element_type *&ptr) LUMEX_NOEXCEPT
  {
    ptr = p.ptr_;
    ctl_base *const cb = p.cb_;
    p.ptr_ = nullptr;
    p.cb_ = nullptr;
    return cb;
  }

  /// The number of weak pointers sharing the block of @p p (0 for none).
  template <class T>
  static long
  weak_count (shared_ptr<T> const &p) LUMEX_NOEXCEPT
  {
    return p.cb_ != nullptr ? p.cb_->weak_count () : 0;
  }

  /// The number of weak pointers sharing the block of @p p (0 for none).
  template <class T>
  static long
  weak_count (weak_ptr<T> const &p) LUMEX_NOEXCEPT
  {
    return p.cb_ != nullptr ? p.cb_->weak_count () : 0;
  }

  /// Sets the weak reference of an `enable_shared_from_this` base once.
  template <class U, class X, class Y>
  static void
  esft_set (enable_shared_from_this<U> const &base, shared_ptr<X> const &owner,
            Y *p)
  {
    if (base.weak_this_.expired ())
      {
        typedef typename std::remove_cv<Y>::type plain_type;
        base.weak_this_
            = shared_ptr<plain_type> (owner, const_cast<plain_type *> (p));
      }
  }

  /// The tag of the private constructors that take over a count.
  struct adopt_tag_t
  {
  };
};

template <class Y, class U>
template <class X>
void
esft_dispatch<Y, esft_found_t<U>>::apply (shared_ptr<X> const &owner, Y *p)
{
  if (p != nullptr)
    access::esft_set (static_cast<enable_shared_from_this<U> const &> (*p),
                      owner, p);
}
} // namespace detail
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_ACCESS_HPP
