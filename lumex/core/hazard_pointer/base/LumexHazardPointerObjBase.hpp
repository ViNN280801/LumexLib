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
 * @file LumexHazardPointerObjBase.hpp
 * @brief `hazard_pointer_obj_base<T, D>`, the base class of objects that
 * hazard pointers protect, and the trait `is_hazard_protectable`.
 * @details A class `T` is hazard-protectable ([saferecl.hp.general]) when it
 * has exactly one base class of the form `hazard_pointer_obj_base<T, D>` for
 * some `D`, that base is public and not virtual, and no base class is a
 * `hazard_pointer_obj_base` of another object type. `retire` hands the object
 * to the domain; its deleter runs, on some thread, once no hazard pointer
 * protects the object ([saferecl.hp.base]). `retire` may reclaim other
 * possibly-reclaimable objects on the calling thread.
 *
 * The base carries a two-word retire node and the deleter (stored through the
 * empty base optimization when `D` is an empty, non-final class). A copy or a
 * move of an object makes a fresh node: the copy is not retired, even when the
 * source is; assignment leaves the node of the target alone. An object is
 * retired at most once (retiring it twice aborts through `LUMEX_ASSERT`); the
 * deleter must not throw.
 *
 * The classes are the library's own on every standard and toolchain, in
 * `lumex::core::hazard_pointer`; the API shape is the standard's.
 */
#ifndef LUMEX_CORE_HAZARD_POINTER_BASE_HAZARD_POINTER_OBJ_BASE_HPP
#define LUMEX_CORE_HAZARD_POINTER_BASE_HAZARD_POINTER_OBJ_BASE_HPP

#include <memory>
#include <type_traits>
#include <utility>

#include "lumex/core/hazard_pointer/engine/LumexHazardPointerEngine.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace hazard_pointer
{
template <class T, class D = std::default_delete<T>>
class hazard_pointer_obj_base;

class hazard_pointer;

namespace detail
{
// The record a retired object carries, as a member (not a base class) so the
// names of node_t do not leak into the protected class. Copying or moving an
// object makes a fresh, not retired node; assigning one object to another
// changes nothing. The class is standard-layout with the node as its only
// member, so the two pointers convert by reinterpret_cast.
class node_base
{
protected:
  node_base () LUMEX_NOEXCEPT { engine::init_node (node_); }
  node_base (node_base const &) LUMEX_NOEXCEPT { engine::init_node (node_); }
  node_base (node_base &&) LUMEX_NOEXCEPT { engine::init_node (node_); }
  node_base &
  operator= (node_base const &) LUMEX_NOEXCEPT
  {
    return *this;
  }
  node_base &
  operator= (node_base &&) LUMEX_NOEXCEPT
  {
    return *this;
  }
  ~node_base () = default;

  engine::node_t *
  node () LUMEX_NOEXCEPT
  {
    return &node_;
  }

  engine::node_t const *
  node () const LUMEX_NOEXCEPT
  {
    return &node_;
  }

  static node_base *
  from_node (engine::node_t *node) LUMEX_NOEXCEPT
  {
    return reinterpret_cast<node_base *> (node);
  }

private:
  engine::node_t node_;
};

template <class D> struct is_final_class
{
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
  static bool const value = __is_final (D);
#else
  static bool const value = false;
#endif
};

// Holds the deleter; an empty deleter takes no space.
template <class D, bool = std::is_empty<D>::value && !is_final_class<D>::value>
class deleter_storage
{
protected:
  deleter_storage () = default;
  deleter_storage (deleter_storage const &) = default;
  deleter_storage (deleter_storage &&) = default;
  deleter_storage &operator= (deleter_storage const &) = default;
  deleter_storage &operator= (deleter_storage &&) = default;
  ~deleter_storage () = default;

  D &
  get_deleter () LUMEX_NOEXCEPT
  {
    return deleter_;
  }

private:
  D deleter_;
};

template <class D> class deleter_storage<D, true> : private D
{
protected:
  deleter_storage () = default;
  deleter_storage (deleter_storage const &) = default;
  deleter_storage (deleter_storage &&) = default;
  deleter_storage &operator= (deleter_storage const &) = default;
  deleter_storage &operator= (deleter_storage &&) = default;
  ~deleter_storage () = default;

  D &
  get_deleter () LUMEX_NOEXCEPT
  {
    return *this;
  }
};

template <class...> struct make_void
{
  typedef void type;
};

template <class U, class D> struct base_pair
{
  typedef U object_type;
  typedef D deleter_type;
};

// Declared, never defined. The first overload deduces the object and deleter
// type of the only base of the form hazard_pointer_obj_base<U, D> (a second
// one or none make the deduction fail) and takes part only when T is that U,
// the base is accessible (is_convertible honors access, which a call in
// decltype does not on every compiler: a private or protected base must make
// the overload vanish, not the program ill-formed) and the conversion back
// from the base exists (a virtual base has none).
template <class T, class U, class D,
          class = decltype (static_cast<U *> (
              std::declval<hazard_pointer_obj_base<U, D> *> ()))>
typename std::enable_if<
    std::is_same<U, typename std::remove_cv<T>::type>::value
        && std::is_convertible<
            T *, hazard_pointer_obj_base<U, D> const volatile *>::value,
    std::true_type>::type
protectable_probe (hazard_pointer_obj_base<U, D> const volatile *);

template <class T> std::false_type protectable_probe (...);

// The object and deleter type of the base of a protectable T.
template <class U, class D>
base_pair<U, D> base_of_probe (hazard_pointer_obj_base<U, D> const volatile *);

template <class T,
          bool = decltype (protectable_probe<T> (std::declval<T *> ()))::value>
struct protectable_base
{
  typedef void type;
};

template <class T> struct protectable_base<T, true>
{
  typedef decltype (base_of_probe (std::declval<T *> ())) pair_type;
  typedef hazard_pointer_obj_base<typename pair_type::object_type,
                                  typename pair_type::deleter_type>
      type;
};

template <class T>
struct protectable_impl
    : std::integral_constant<bool, decltype (protectable_probe<T> (
                                       std::declval<T *> ()))::value>
{
  typedef typename protectable_base<T>::type base_type;
};
} // namespace detail

/**
 * @brief True when `T` is hazard-protectable ([saferecl.hp.general]).
 * @details `T` may be cv-qualified. `T` must be complete.
 */
template <class T>
struct is_hazard_protectable
    : std::integral_constant<bool, detail::protectable_impl<T>::value>
{
};

/**
 * @brief Base class of objects that hazard pointers protect
 * ([saferecl.hp.base]).
 * @tparam T The most derived class, the one that derives from this base.
 * @tparam D The function object that reclaims an object: default
 * constructible and move assignable.
 */
template <class T, class D>
class hazard_pointer_obj_base : private detail::node_base,
                                private detail::deleter_storage<D>
{
public:
  /**
   * @brief Retires the object: it is reclaimed with `d` once no hazard
   * pointer protects it.
   * @param[in] d The deleter, move-assigned to the stored one.
   * @pre `T` is hazard-protectable. The object was not retired before, and
   * moving `d` does not throw.
   * @note May reclaim other possibly-reclaimable objects on this thread.
   */
  void
  retire (D d = D ()) LUMEX_NOEXCEPT
  {
    static_assert (is_hazard_protectable<T>::value,
                   "T must have exactly one public, non-virtual base "
                   "hazard_pointer_obj_base<T, D>");
    this->get_deleter () = std::move (d);
    this->node ()->reclaim = &hazard_pointer_obj_base::reclaim_node;
    engine::retire_node (this->node ());
  }

protected:
  hazard_pointer_obj_base () = default;
  hazard_pointer_obj_base (hazard_pointer_obj_base const &) = default;
  hazard_pointer_obj_base (hazard_pointer_obj_base &&) = default;
  hazard_pointer_obj_base &operator= (hazard_pointer_obj_base const &)
      = default;
  hazard_pointer_obj_base &operator= (hazard_pointer_obj_base &&) = default;
  ~hazard_pointer_obj_base () = default;

private:
  friend class hazard_pointer;

  // The address a hazard slot holds for this object.
  static void const *
  node_address (hazard_pointer_obj_base const *self) LUMEX_NOEXCEPT
  {
    return self->node ();
  }

  static void
  reclaim_node (engine::node_t *node) LUMEX_NOEXCEPT
  {
    hazard_pointer_obj_base *self = static_cast<hazard_pointer_obj_base *> (
        detail::node_base::from_node (node));
    T *object = static_cast<T *> (self);
    self->get_deleter () (object);
  }
};
} // namespace hazard_pointer
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_HAZARD_POINTER_BASE_HAZARD_POINTER_OBJ_BASE_HPP
