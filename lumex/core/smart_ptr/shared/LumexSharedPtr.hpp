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
 * @file LumexSharedPtr.hpp
 * @brief `shared_ptr<T>`: shared ownership with the module's own control
 * block, at the level of the C++17 `std::shared_ptr`.
 * @details The class has the interface of [util.smartptr.shared] of C++17 on
 * every standard from C++11: all constructors (default, `nullptr`, `Y *`,
 * `Y *` with a deleter and an allocator, `nullptr` with a deleter, the
 * aliasing constructors with an lvalue and with an rvalue, copy and move,
 * converting, from `weak_ptr`, from `unique_ptr`), the assignments, `swap`,
 * the four `reset` forms, `get`, `*`, `->`, `[]` for arrays, `use_count`,
 * `operator bool`, `owner_before`, the comparison operators (with `<=>` from
 * C++20), `swap`, `get_deleter`, `operator<<`, `std::hash`, and the
 * deduction guides of C++17. `T[]` and `T[N]` follow the C++17 text;
 * `make_shared` for arrays (C++20) is not provided. `unique ()` is not
 * provided (removed in C++20).
 *
 * Differences from `std::shared_ptr` that are on purpose:
 * - The control block is the module's own `detail::ctl_base` (32 bytes, a
 *   packed strong counter and a weak ledger), designed for the split-count
 *   engine of `core/atomic`. The class is two pointers in size, as the
 *   standard one is.
 * - No implicit conversion to or from `std::shared_ptr`; `from_std` and
 *   `to_std` (`interop/LumexSmartPtrStd.hpp`) wrap explicitly.
 * - `shared_ptr<T> (Y *)` rejects, with a `static_assert`, a class that
 *   derives from `std::enable_shared_from_this` and not from the module's
 *   `enable_shared_from_this` (the standard base would stay unset); see
 *   `LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS`.
 * - The deleter and the pointer type of an allocator must not be fancy
 *   pointers.
 * - `get_deleter` finds the deleters of the module's own blocks only.
 *
 * Thread safety is that of [util.smartptr.shared.general]: the counters are
 * atomic, so distinct `shared_ptr` objects that share ownership may be
 * copied, assigned and destroyed concurrently; one object needs external
 * synchronization (or the atomic smart pointers of `core/atomic`).
 * `use_count ()` is exact when no other thread changes the owners of the
 * block at that moment.
 */
#ifndef LUMEX_CORE_SMART_PTR_SHARED_SHARED_PTR_HPP
#define LUMEX_CORE_SMART_PTR_SHARED_SHARED_PTR_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlBase.hpp"
#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlKinds.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrAccess.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrTraits.hpp"
#include "lumex/core/smart_ptr/shared/LumexBadWeakPtr.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
/**
 * @brief Shared ownership of an object ([util.smartptr.shared]).
 * @tparam T The managed type: an object type, `void`, `U[]` or `U[N]`.
 */
template <class T> class shared_ptr
{
public:
  /// `T` without one array extent ([util.smartptr.shared.general]).
  typedef typename std::remove_extent<T>::type element_type;
  /// The weak pointer type of this one (C++17).
  typedef weak_ptr<T> weak_type;

  // -- Construction ([util.smartptr.shared.const]) --

  /// An empty pointer: owns nothing, stores `nullptr`.
  LUMEX_CONSTEXPR_CTOR
  shared_ptr () LUMEX_NOEXCEPT : ptr_ (nullptr), cb_ (nullptr) {}

  /// An empty pointer.
  LUMEX_CONSTEXPR_CTOR
  shared_ptr (std::nullptr_t) LUMEX_NOEXCEPT : ptr_ (nullptr), cb_ (nullptr) {}

  /**
   * @brief Takes ownership of @p p; the object is deleted with `delete`
   * (`delete[]` for an array type).
   * @details Takes part in overload resolution when `Y *` (`Y (*)[]`,
   * `Y (*)[N]` for arrays) converts to the stored pointer type. `Y` must be
   * complete. Throws `std::bad_alloc` when the control block cannot be
   * allocated; @p p is deleted then. If `Y` has an accessible base
   * `enable_shared_from_this`, its weak reference is set.
   */
  template <class Y, class = typename std::enable_if<
                         detail::is_new_compatible<Y, T>::value>::type>
  explicit shared_ptr (Y *p) : ptr_ (p), cb_ (nullptr)
  {
    typedef typename detail::default_deleter_for<T, Y>::type deleter_type;
    detail::check_std_enable_shared_from_this<Y> ();
    deleter_type deleter;
    create_owner (p, deleter, detail::block_allocator<void> ());
    enable_from_this (p, std::is_array<T> ());
  }

  /// Takes ownership of @p p with the deleter @p d; `d (p)` deletes the
  /// object, and is called now if the control block cannot be allocated.
  template <class Y, class D,
            class = typename std::enable_if<
                detail::is_new_compatible<Y, T>::value
                && std::is_move_constructible<D>::value
                && detail::is_deleter_for<D, Y *>::value>::type>
  shared_ptr (Y *p, D d) : ptr_ (p), cb_ (nullptr)
  {
    detail::check_std_enable_shared_from_this<Y> ();
    create_owner (p, d, detail::block_allocator<void> ());
    enable_from_this (p, std::is_array<T> ());
  }

  /// As above, with an allocator for the control block.
  template <class Y, class D, class A,
            class = typename std::enable_if<
                detail::is_new_compatible<Y, T>::value
                && std::is_move_constructible<D>::value
                && detail::is_deleter_for<D, Y *>::value>::type>
  shared_ptr (Y *p, D d, A a) : ptr_ (p), cb_ (nullptr)
  {
    detail::check_std_enable_shared_from_this<Y> ();
    create_owner (p, d, a);
    enable_from_this (p, std::is_array<T> ());
  }

  /// Owns a null pointer with the deleter @p d (called with `nullptr`).
  template <class D,
            class = typename std::enable_if<
                std::is_move_constructible<D>::value
                && detail::is_deleter_for<D, std::nullptr_t>::value>::type>
  shared_ptr (std::nullptr_t p, D d) : ptr_ (nullptr), cb_ (nullptr)
  {
    create_owner (p, d, detail::block_allocator<void> ());
  }

  /// As above, with an allocator for the control block.
  template <class D, class A,
            class = typename std::enable_if<
                std::is_move_constructible<D>::value
                && detail::is_deleter_for<D, std::nullptr_t>::value>::type>
  shared_ptr (std::nullptr_t p, D d, A a) : ptr_ (nullptr), cb_ (nullptr)
  {
    create_owner (p, d, a);
  }

  /**
   * @brief The aliasing constructor: shares ownership with @p r and stores
   * @p p.
   * @details @p p is not owned and not checked; it must stay valid as long as
   * @p r owns the object. If @p r is empty the result is empty too, but
   * `get ()` returns @p p.
   */
  template <class Y>
  shared_ptr (shared_ptr<Y> const &r, element_type *p) LUMEX_NOEXCEPT
      : ptr_ (p),
        cb_ (r.cb_)
  {
    if (cb_ != nullptr)
      cb_->add_strong ();
  }

  /// The aliasing constructor for an rvalue (C++20): takes the ownership of
  /// @p r, which is left empty.
  template <class Y>
  shared_ptr (shared_ptr<Y> &&r, element_type *p) LUMEX_NOEXCEPT : ptr_ (p),
                                                                   cb_ (r.cb_)
  {
    r.ptr_ = nullptr;
    r.cb_ = nullptr;
  }

  /// Shares ownership with @p r.
  shared_ptr (shared_ptr const &r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    if (cb_ != nullptr)
      cb_->add_strong ();
  }

  /// Shares ownership with @p r; takes part when `Y *` is compatible with
  /// `T *`.
  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  shared_ptr (shared_ptr<Y> const &r) LUMEX_NOEXCEPT : ptr_ (r.ptr_),
                                                       cb_ (r.cb_)
  {
    if (cb_ != nullptr)
      cb_->add_strong ();
  }

  /// Takes the ownership of @p r, which is left empty.
  shared_ptr (shared_ptr &&r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    r.ptr_ = nullptr;
    r.cb_ = nullptr;
  }

  /// Takes the ownership of @p r, which is left empty.
  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  shared_ptr (shared_ptr<Y> &&r) LUMEX_NOEXCEPT : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    r.ptr_ = nullptr;
    r.cb_ = nullptr;
  }

  /// Shares ownership with the object @p r refers to; throws
  /// `bad_weak_ptr` when it is gone.
  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  explicit shared_ptr (weak_ptr<Y> const &r) : ptr_ (r.ptr_), cb_ (r.cb_)
  {
    if (cb_ == nullptr || !cb_->try_add_strong ())
      {
        ptr_ = nullptr;
        cb_ = nullptr;
        LUMEX_SMART_PTR_THROW (bad_weak_ptr ());
      }
  }

  /// Takes the object of @p r (a no-op for a null `unique_ptr`); a deleter
  /// that is a reference is referred to, not copied. If the control block
  /// cannot be allocated @p r keeps the object.
  template <class Y, class D,
            class = typename std::enable_if<
                detail::is_ptr_compatible<Y, T>::value
                && std::is_convertible<typename std::unique_ptr<Y, D>::pointer,
                                       element_type *>::value>::type>
  shared_ptr (std::unique_ptr<Y, D> &&r) : ptr_ (nullptr), cb_ (nullptr)
  {
    adopt_unique (r, std::is_reference<D> ());
  }

  /// Drops one owner; deletes the object when it was the last.
  ~shared_ptr ()
  {
    if (cb_ != nullptr)
      cb_->release_strong ();
  }

  // -- Assignment ([util.smartptr.shared.assign]) --

  shared_ptr &
  operator= (shared_ptr const &r) LUMEX_NOEXCEPT
  {
    shared_ptr (r).swap (*this);
    return *this;
  }

  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  shared_ptr &
  operator= (shared_ptr<Y> const &r) LUMEX_NOEXCEPT
  {
    shared_ptr (r).swap (*this);
    return *this;
  }

  shared_ptr &
  operator= (shared_ptr &&r) LUMEX_NOEXCEPT
  {
    shared_ptr (std::move (r)).swap (*this);
    return *this;
  }

  template <class Y, class = typename std::enable_if<
                         detail::is_ptr_compatible<Y, T>::value>::type>
  shared_ptr &
  operator= (shared_ptr<Y> &&r) LUMEX_NOEXCEPT
  {
    shared_ptr (std::move (r)).swap (*this);
    return *this;
  }

  template <class Y, class D,
            class = typename std::enable_if<
                detail::is_ptr_compatible<Y, T>::value
                && std::is_convertible<typename std::unique_ptr<Y, D>::pointer,
                                       element_type *>::value>::type>
  shared_ptr &
  operator= (std::unique_ptr<Y, D> &&r)
  {
    shared_ptr (std::move (r)).swap (*this);
    return *this;
  }

  // -- Modifiers ([util.smartptr.shared.mod]) --

  /// Exchanges the pointers and the ownership; never throws.
  void
  swap (shared_ptr &r) LUMEX_NOEXCEPT
  {
    element_type *const p = ptr_;
    ptr_ = r.ptr_;
    r.ptr_ = p;
    detail::ctl_base *const c = cb_;
    cb_ = r.cb_;
    r.cb_ = c;
  }

  /// Releases the ownership; the pointer becomes empty.
  void
  reset () LUMEX_NOEXCEPT
  {
    shared_ptr ().swap (*this);
  }

  /// Takes ownership of @p p, releasing the previous object.
  template <class Y, class = typename std::enable_if<
                         detail::is_new_compatible<Y, T>::value>::type>
  void
  reset (Y *p)
  {
    shared_ptr (p).swap (*this);
  }

  /// Takes ownership of @p p with the deleter @p d.
  template <class Y, class D,
            class = typename std::enable_if<
                detail::is_new_compatible<Y, T>::value
                && std::is_move_constructible<D>::value
                && detail::is_deleter_for<D, Y *>::value>::type>
  void
  reset (Y *p, D d)
  {
    shared_ptr (p, std::move (d)).swap (*this);
  }

  /// Takes ownership of @p p with the deleter @p d and the allocator @p a.
  template <class Y, class D, class A,
            class = typename std::enable_if<
                detail::is_new_compatible<Y, T>::value
                && std::is_move_constructible<D>::value
                && detail::is_deleter_for<D, Y *>::value>::type>
  void
  reset (Y *p, D d, A a)
  {
    shared_ptr (p, std::move (d), a).swap (*this);
  }

  // -- Observers ([util.smartptr.shared.obs]) --

  /// The stored pointer.
  element_type *
  get () const LUMEX_NOEXCEPT
  {
    return ptr_;
  }

  /// The object, for a non-array, non-void `T`.
  template <class U = T,
            class = typename std::enable_if<!std::is_array<U>::value
                                            && !std::is_void<U>::value>::type>
  typename std::add_lvalue_reference<element_type>::type
  operator* () const LUMEX_NOEXCEPT
  {
    return *ptr_;
  }

  /// The stored pointer, for a non-array `T`.
  template <class U = T,
            class = typename std::enable_if<!std::is_array<U>::value>::type>
  element_type *
  operator->() const LUMEX_NOEXCEPT
  {
    return ptr_;
  }

  /// Element @p i of an array; @p i must be inside the array.
  template <class U = T>
  typename std::enable_if<
      std::is_array<U>::value,
      typename std::add_lvalue_reference<
          typename std::remove_extent<U>::type>::type>::type
  operator[] (std::ptrdiff_t i) const
  {
    return ptr_[i];
  }

  /// The number of `shared_ptr` objects that own the object (0 for an
  /// empty pointer).
  long
  use_count () const LUMEX_NOEXCEPT
  {
    return cb_ != nullptr ? cb_->use_count () : 0;
  }

  /// The number of owners once no owner is in transit: `use_count ()` that
  /// waits (spins, then yields) while an atomic smart pointer has owners in
  /// transit on the block. Exact apart from the lag of a slot's decrement; it
  /// may block while a pinning thread is suspended, so it must not be called
  /// from a signal handler on a pinned thread. 0 for an empty pointer.
  long
  use_count_settled () const LUMEX_NOEXCEPT
  {
    return cb_ != nullptr ? cb_->use_count_settled () : 0;
  }

  /// True when the stored pointer is not null.
  explicit
  operator bool () const LUMEX_NOEXCEPT
  {
    return ptr_ != nullptr;
  }

  /// Owner-based order: true when the control block of this pointer
  /// precedes the one of @p other (a strict weak order that ignores the
  /// stored pointer).
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

  // Takes over a count the caller owns (detail::access::adopt).
  shared_ptr (element_type *p, detail::ctl_base *cb,
              detail::access::adopt_tag_t) LUMEX_NOEXCEPT : ptr_ (p),
                                                            cb_ (cb)
  {
  }

  // Creates the control block of a pointer and a deleter. The deleter is
  // called with the pointer when the block cannot be created
  // ([util.smartptr.shared.const]).
  template <class P, class D, class A>
  void
  create_owner (P p, D &d, A const &a)
  {
    typedef detail::ctl_ptr<P, D, A> block_type;
    LUMEX_SMART_PTR_TRY { cb_ = block_type::create (p, std::move (d), a); }
    LUMEX_SMART_PTR_CATCH_ALL
    {
      d (p);
      LUMEX_SMART_PTR_RETHROW;
    }
    cb_->set_anchor (reinterpret_cast<std::uintptr_t> (ptr_));
  }

  template <class Y>
  void
  enable_from_this (Y *p, std::false_type)
  {
    detail::esft_dispatch<Y>::apply (*this, p);
  }

  template <class Y>
  void
  enable_from_this (Y *, std::true_type) LUMEX_NOEXCEPT
  {
  }

  // unique_ptr with a deleter object: the deleter moves into the block.
  template <class Y, class D>
  void
  adopt_unique (std::unique_ptr<Y, D> &r, std::false_type)
  {
    typename std::unique_ptr<Y, D>::pointer const p = r.get ();
    if (p == nullptr)
      return;
    cb_ = detail::ctl_ptr<typename std::unique_ptr<Y, D>::pointer, D,
                          detail::block_allocator<void>>::
        create (p, std::move (r.get_deleter ()),
                detail::block_allocator<void> ());
    ptr_ = p;
    cb_->set_anchor (reinterpret_cast<std::uintptr_t> (ptr_));
    r.release ();
    enable_from_this (static_cast<element_type *> (ptr_), std::is_array<T> ());
  }

  // unique_ptr with a deleter reference: the block refers to the deleter.
  template <class Y, class D>
  void
  adopt_unique (std::unique_ptr<Y, D> &r, std::true_type)
  {
    typedef typename std::remove_reference<D>::type deleter_object;
    typedef std::reference_wrapper<deleter_object> deleter_ref;
    typename std::unique_ptr<Y, D>::pointer const p = r.get ();
    if (p == nullptr)
      return;
    cb_ = detail::ctl_ptr<typename std::unique_ptr<Y, D>::pointer, deleter_ref,
                          detail::block_allocator<void>>::
        create (p, deleter_ref (r.get_deleter ()),
                detail::block_allocator<void> ());
    ptr_ = p;
    cb_->set_anchor (reinterpret_cast<std::uintptr_t> (ptr_));
    r.release ();
    enable_from_this (static_cast<element_type *> (ptr_), std::is_array<T> ());
  }

  element_type *ptr_;
  detail::ctl_base *cb_;
};

#if defined(__cpp_deduction_guides) && __cplusplus >= 201703L
template <class T> shared_ptr (weak_ptr<T>) -> shared_ptr<T>;
template <class T, class D>
shared_ptr (std::unique_ptr<T, D>) -> shared_ptr<T>;
#endif

// -- Comparison ([util.smartptr.shared.cmp]) --

/// True when the stored pointers are equal.
template <class T, class U>
bool
operator== (shared_ptr<T> const &a, shared_ptr<U> const &b) LUMEX_NOEXCEPT
{
  return a.get () == b.get ();
}

/// True when @p a stores a null pointer.
template <class T>
bool
operator== (shared_ptr<T> const &a, std::nullptr_t) LUMEX_NOEXCEPT
{
  return !a;
}

#if LUMEX_HAS_THREE_WAY_COMPARISON
/// Orders by the stored pointers (`std::compare_three_way`).
template <class T, class U>
std::strong_ordering
operator<=> (shared_ptr<T> const &a, shared_ptr<U> const &b) LUMEX_NOEXCEPT
{
  return std::compare_three_way () (a.get (), b.get ());
}

template <class T>
std::strong_ordering
operator<=> (shared_ptr<T> const &a, std::nullptr_t) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type *pointer_type;
  return std::compare_three_way () (a.get (),
                                    static_cast<pointer_type> (nullptr));
}
#else
template <class T, class U>
bool
operator!= (shared_ptr<T> const &a, shared_ptr<U> const &b) LUMEX_NOEXCEPT
{
  return a.get () != b.get ();
}

template <class T>
bool
operator!= (shared_ptr<T> const &a, std::nullptr_t) LUMEX_NOEXCEPT
{
  return static_cast<bool> (a);
}

template <class T>
bool
operator== (std::nullptr_t, shared_ptr<T> const &b) LUMEX_NOEXCEPT
{
  return !b;
}

template <class T>
bool
operator!= (std::nullptr_t, shared_ptr<T> const &b) LUMEX_NOEXCEPT
{
  return static_cast<bool> (b);
}

/// Orders by the stored pointers (`std::less` of their common type).
template <class T, class U>
bool
operator< (shared_ptr<T> const &a, shared_ptr<U> const &b) LUMEX_NOEXCEPT
{
  typedef
      typename std::common_type<typename shared_ptr<T>::element_type *,
                                typename shared_ptr<U>::element_type *>::type
          common_pointer;
  return std::less<common_pointer> () (a.get (), b.get ());
}

template <class T, class U>
bool
operator> (shared_ptr<T> const &a, shared_ptr<U> const &b) LUMEX_NOEXCEPT
{
  return b < a;
}

template <class T, class U>
bool
operator<= (shared_ptr<T> const &a, shared_ptr<U> const &b) LUMEX_NOEXCEPT
{
  return !(b < a);
}

template <class T, class U>
bool
operator>= (shared_ptr<T> const &a, shared_ptr<U> const &b) LUMEX_NOEXCEPT
{
  return !(a < b);
}

template <class T>
bool
operator< (shared_ptr<T> const &a, std::nullptr_t) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type *pointer_type;
  return std::less<pointer_type> () (a.get (), nullptr);
}

template <class T>
bool
operator< (std::nullptr_t, shared_ptr<T> const &b) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type *pointer_type;
  return std::less<pointer_type> () (nullptr, b.get ());
}

template <class T>
bool
operator> (shared_ptr<T> const &a, std::nullptr_t) LUMEX_NOEXCEPT
{
  return nullptr < a;
}

template <class T>
bool
operator> (std::nullptr_t, shared_ptr<T> const &b) LUMEX_NOEXCEPT
{
  return b < nullptr;
}

template <class T>
bool
operator<= (shared_ptr<T> const &a, std::nullptr_t) LUMEX_NOEXCEPT
{
  return !(nullptr < a);
}

template <class T>
bool
operator<= (std::nullptr_t, shared_ptr<T> const &b) LUMEX_NOEXCEPT
{
  return !(b < nullptr);
}

template <class T>
bool
operator>= (shared_ptr<T> const &a, std::nullptr_t) LUMEX_NOEXCEPT
{
  return !(a < nullptr);
}

template <class T>
bool
operator>= (std::nullptr_t, shared_ptr<T> const &b) LUMEX_NOEXCEPT
{
  return !(nullptr < b);
}
#endif

// -- Specialized algorithms and observers --

/// Exchanges two pointers ([util.smartptr.shared.spec]).
template <class T>
void
swap (shared_ptr<T> &a, shared_ptr<T> &b) LUMEX_NOEXCEPT
{
  a.swap (b);
}

/// Writes the stored pointer to @p os.
template <class E, class Tr, class T>
std::basic_ostream<E, Tr> &
operator<< (std::basic_ostream<E, Tr> &os, shared_ptr<T> const &p)
{
  return os << p.get ();
}

/**
 * @brief A pointer to the deleter of type `D` that @p p owns, or null.
 * @details Only a deleter that was given to the constructor or to `reset`
 * is found (not the implicit `delete`, not a `make_shared` block), as in the
 * standard. A deleter that came from a shared library other than this one
 * is not found: the type identity is per library.
 */
template <class D, class T>
D *
get_deleter (shared_ptr<T> const &p) LUMEX_NOEXCEPT
{
  detail::ctl_base *const cb = detail::access::control (p);
  if (cb == nullptr)
    return nullptr;
  return static_cast<D *> (
      cb->query (detail::type_id<typename std::remove_cv<D>::type> ()));
}

// -- Casts ([util.smartptr.shared.cast]) --

/// `static_cast` of the stored pointer; shares ownership with @p r.
template <class T, class U>
shared_ptr<T>
static_pointer_cast (shared_ptr<U> const &r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  return shared_ptr<T> (r, static_cast<element_type *> (r.get ()));
}

/// `static_cast` of the stored pointer; takes the ownership of @p r.
template <class T, class U>
shared_ptr<T>
static_pointer_cast (shared_ptr<U> &&r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  return shared_ptr<T> (std::move (r), static_cast<element_type *> (r.get ()));
}

/// `dynamic_cast` of the stored pointer: empty when the cast fails.
template <class T, class U>
shared_ptr<T>
dynamic_pointer_cast (shared_ptr<U> const &r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  if (element_type *const p = dynamic_cast<element_type *> (r.get ()))
    return shared_ptr<T> (r, p);
  return shared_ptr<T> ();
}

/// `dynamic_cast` of the stored pointer: empty when the cast fails (@p r is
/// then unchanged), otherwise takes the ownership of @p r.
template <class T, class U>
shared_ptr<T>
dynamic_pointer_cast (shared_ptr<U> &&r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  if (element_type *const p = dynamic_cast<element_type *> (r.get ()))
    return shared_ptr<T> (std::move (r), p);
  return shared_ptr<T> ();
}

/// `const_cast` of the stored pointer; shares ownership with @p r.
template <class T, class U>
shared_ptr<T>
const_pointer_cast (shared_ptr<U> const &r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  return shared_ptr<T> (r, const_cast<element_type *> (r.get ()));
}

/// `const_cast` of the stored pointer; takes the ownership of @p r.
template <class T, class U>
shared_ptr<T>
const_pointer_cast (shared_ptr<U> &&r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  return shared_ptr<T> (std::move (r), const_cast<element_type *> (r.get ()));
}

/// `reinterpret_cast` of the stored pointer; shares ownership with @p r.
template <class T, class U>
shared_ptr<T>
reinterpret_pointer_cast (shared_ptr<U> const &r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  return shared_ptr<T> (r, reinterpret_cast<element_type *> (r.get ()));
}

/// `reinterpret_cast` of the stored pointer; takes the ownership of @p r.
template <class T, class U>
shared_ptr<T>
reinterpret_pointer_cast (shared_ptr<U> &&r) LUMEX_NOEXCEPT
{
  typedef typename shared_ptr<T>::element_type element_type;
  return shared_ptr<T> (std::move (r),
                        reinterpret_cast<element_type *> (r.get ()));
}
} // namespace smart_ptr
} // namespace core
} // namespace lumex

namespace std
{
/// Hash of the stored pointer ([util.smartptr.hash]).
template <class T> struct hash<lumex::core::smart_ptr::shared_ptr<T>>
{
  typedef lumex::core::smart_ptr::shared_ptr<T> argument_type;
  typedef std::size_t result_type;

  std::size_t
  operator() (lumex::core::smart_ptr::shared_ptr<T> const &p) const
      LUMEX_NOEXCEPT
  {
    return std::hash<
        typename lumex::core::smart_ptr::shared_ptr<T>::element_type *> () (
        p.get ());
  }
};
} // namespace std

#endif // !LUMEX_CORE_SMART_PTR_SHARED_SHARED_PTR_HPP
