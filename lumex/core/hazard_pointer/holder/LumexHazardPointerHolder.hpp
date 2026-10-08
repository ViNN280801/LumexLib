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
 * @file LumexHazardPointerHolder.hpp
 * @brief `hazard_pointer`, the owner of one hazard slot, with
 * `make_hazard_pointer`, `swap` and the extension `clean_up`.
 * @details A `hazard_pointer` ([saferecl.hp.holder]) owns one hazard slot of
 * the domain. It protects at most one object at a time: `try_protect`
 * announces the object, issues a fence and checks that the source still holds
 * it, so a deleter of `hazard_pointer_obj_base` cannot run for the object
 * until the protection ends (`reset_protection`, the next `try_protect`, a
 * move assignment over the holder, or its destruction). A holder is movable,
 * not copyable; a default constructed or moved-from holder is empty and owns
 * no slot. `make_hazard_pointer` may throw `std::bad_alloc`; everything else
 * is `noexcept`.
 *
 * Every member that uses the slot expects a non-empty holder; that is checked
 * with `LUMEX_ASSERT` in builds without `NDEBUG` only, since this is the hot
 * path. A holder is used by one thread at a time; moving it to another thread
 * is fine.
 *
 * `clean_up ()` is an extension of this module: it runs a reclamation pass at
 * once, so that tests and leak checkers see every unprotected retired object
 * reclaimed.
 *
 * The classes are the library's own on every standard and toolchain, in
 * `lumex::core::hazard_pointer`; the API shape is the standard's.
 */
#ifndef LUMEX_CORE_HAZARD_POINTER_HOLDER_HAZARD_POINTER_HOLDER_HPP
#define LUMEX_CORE_HAZARD_POINTER_HOLDER_HAZARD_POINTER_HOLDER_HPP

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "lumex/core/hazard_pointer/base/LumexHazardPointerObjBase.hpp"
#include "lumex/core/hazard_pointer/engine/LumexHazardPointerEngine.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace hazard_pointer
{
class hazard_pointer;

namespace detail
{
// The one door to the slot of a holder for the free functions of the module.
struct holder_access;
} // namespace detail

/**
 * @brief Owner of one hazard slot ([saferecl.hp.holder]).
 */
class hazard_pointer
{
public:
  /// Constructs an empty holder (no slot).
  hazard_pointer () LUMEX_NOEXCEPT : slot_ (nullptr) {}

  /// Takes the slot of `other`, which becomes empty.
  hazard_pointer (hazard_pointer &&other) LUMEX_NOEXCEPT : slot_ (other.slot_)
  {
    other.slot_ = nullptr;
  }

  /// Ends the protection of this holder and takes the slot of `other`;
  /// self-assignment has no effect.
  hazard_pointer &
  operator= (hazard_pointer &&other) LUMEX_NOEXCEPT
  {
    if (this != &other)
      {
        release ();
        slot_ = other.slot_;
        other.slot_ = nullptr;
      }
    return *this;
  }

  hazard_pointer (hazard_pointer const &) = delete;
  hazard_pointer &operator= (hazard_pointer const &) = delete;

  /// Ends the protection and gives the slot back (when not empty).
  ~hazard_pointer () { release (); }

  /// True when the holder owns no slot.
  LUMEX_ATTRIBUTE_NODISCARD ("an empty holder protects nothing")
  bool
  empty () const LUMEX_NOEXCEPT
  {
    return slot_ == nullptr;
  }

  /**
   * @brief Loads `src` and protects the loaded object; returns it.
   * @pre The holder is not empty. `T` is hazard-protectable.
   */
  template <class T>
  LUMEX_ATTRIBUTE_NODISCARD ("the pointer is the point of calling protect")
  T *protect (std::atomic<T *> const &src) LUMEX_NOEXCEPT
  {
    T *ptr = src.load (std::memory_order_relaxed);
    while (!try_protect (ptr, src))
      {
      }
    return ptr;
  }

  /**
   * @brief Protects `ptr` and checks that `src` still holds it.
   * @param[in,out] ptr The pointer to protect; on failure it receives the
   * value that `src` holds now.
   * @param[in] src The source the pointer was read from.
   * @return True when `src` still held `ptr` after the announcement; the
   * object is then protected. False: nothing is protected, `ptr` is the new
   * value.
   * @pre The holder is not empty. `T` is hazard-protectable.
   */
  template <class T>
  LUMEX_ATTRIBUTE_NODISCARD ("false means the object is not protected")
  bool try_protect (T *&ptr, std::atomic<T *> const &src) LUMEX_NOEXCEPT
  {
    return announce_and_validate (ptr, atomic_load<T> (src));
  }

  /**
   * @brief Extension: `try_protect` over a source that is not a
   * `std::atomic<T *>`.
   * @tparam T The protected object type.
   * @tparam Source A callable: `src ()` loads the shared word (with at least
   * acquire ordering; pass `seq_cst` if the structure needs it) and returns
   * it, for example a tagged pointer or an integer.
   * @tparam Filter A callable: `filter (word)` returns the `T *` the word
   * stands for (the word with its tag bits removed).
   * @param[in,out] ptr The pointer to protect; on failure it receives
   * `filter (src ())`.
   * @param[in] src Loads the shared word.
   * @param[in] filter Maps a loaded word to the pointer.
   * @return True when `filter (src ())` equals `ptr` after the announcement.
   * @pre The holder is not empty. `T` is hazard-protectable. Neither
   * callable throws.
   * @note An extension: the standard's `hazard_pointer` has no such
   * overload.
   */
  template <class T, class Source, class Filter>
  LUMEX_ATTRIBUTE_NODISCARD ("false means the object is not protected")
  bool try_protect (T *&ptr, Source const &src, Filter filter) LUMEX_NOEXCEPT
  {
    return announce_and_validate (ptr,
                                  filtered_load<Source, Filter> (src, filter));
  }

  /**
   * @brief Extension: `protect` over a callable source and a filter (see
   * `try_protect` with three arguments).
   * @return The pointer `filter (src ())` returned when the protection held.
   */
  template <class Source, class Filter>
  LUMEX_ATTRIBUTE_NODISCARD ("the pointer is the point of calling protect")
  auto protect (Source const &src,
                Filter filter) LUMEX_NOEXCEPT->decltype (filter (src ()))
  {
    typedef decltype (filter (src ())) pointer_type;
    static_assert (std::is_pointer<pointer_type>::value,
                   "the filter returns a pointer");
    pointer_type ptr = filter (src ());
    while (!try_protect (ptr, src, filter))
      {
      }
    return ptr;
  }

  /**
   * @brief Protects `ptr` without checking a source (ends the previous
   * protection).
   * @details For an object that is already protected some other way, for
   * example by another holder (hand-over-hand traversal). A null `ptr` ends
   * the protection.
   * @pre The holder is not empty. `T` is hazard-protectable.
   */
  template <class T>
  void
  reset_protection (T const *ptr) LUMEX_NOEXCEPT
  {
    static_assert (is_hazard_protectable<T>::value,
                   "T must have exactly one public, non-virtual base "
                   "hazard_pointer_obj_base<T, D>");
    LUMEX_HAZARD_POINTER_DEBUG_ASSERT (slot_ != nullptr);
    announce (ptr == nullptr ? static_cast<void const *> (nullptr)
                             : node_address (ptr));
  }

  /**
   * @brief Ends the protection.
   * @pre The holder is not empty.
   */
  void
  reset_protection (std::nullptr_t = nullptr) LUMEX_NOEXCEPT
  {
    LUMEX_HAZARD_POINTER_DEBUG_ASSERT (slot_ != nullptr);
    announce (nullptr);
  }

  /// Exchanges the slots; no protection starts or ends.
  void
  swap (hazard_pointer &other) LUMEX_NOEXCEPT
  {
    engine::slot_t *slot = slot_;
    slot_ = other.slot_;
    other.slot_ = slot;
  }

private:
  friend struct detail::holder_access;

  explicit hazard_pointer (engine::slot_t *slot) LUMEX_NOEXCEPT : slot_ (slot)
  {
  }

  // The load of try_protect over a std::atomic.
  template <class T> struct atomic_load
  {
    explicit atomic_load (std::atomic<T *> const &source) LUMEX_NOEXCEPT
        : source_ (source)
    {
    }

    T *
    operator() () const LUMEX_NOEXCEPT
    {
      return source_.load (std::memory_order_acquire);
    }

    std::atomic<T *> const &source_;
  };

  // The load of the extension over a callable source and a filter.
  template <class Source, class Filter> struct filtered_load
  {
    filtered_load (Source const &source, Filter &filter) LUMEX_NOEXCEPT
        : source_ (source),
          filter_ (filter)
    {
    }

    auto
    operator() () const LUMEX_NOEXCEPT->decltype (std::declval<Filter &> () (
        std::declval<Source const &> () ()))
    {
      return filter_ (source_ ());
    }

    Source const &source_;
    Filter &filter_;
  };

  // The protocol: announce, fence, reload through `load`, compare.
  template <class T, class Load>
  bool
  announce_and_validate (T *&ptr, Load const &load) LUMEX_NOEXCEPT
  {
    static_assert (is_hazard_protectable<T>::value,
                   "T must have exactly one public, non-virtual base "
                   "hazard_pointer_obj_base<T, D>");
    LUMEX_HAZARD_POINTER_DEBUG_ASSERT (slot_ != nullptr);
    T *old = ptr;
    announce (old == nullptr ? static_cast<void const *> (nullptr)
                             : node_address (old));
    engine::reader_fence ();
    LUMEX_HAZARD_POINTER_TEST_POINT (reader_announced);
    ptr = load ();
    if (old != ptr)
      {
        announce (nullptr);
        return false;
      }
    return true;
  }

  void
  announce (void const *node) LUMEX_NOEXCEPT
  {
    slot_->value.store (node, std::memory_order_release);
  }

  void
  release () LUMEX_NOEXCEPT
  {
    if (slot_ != nullptr)
      {
        engine::release_slot (slot_);
        slot_ = nullptr;
      }
  }

  template <class T>
  static void const *
  node_address (T const *ptr) LUMEX_NOEXCEPT
  {
    typedef typename detail::protectable_impl<T>::base_type base_type;
    return base_type::node_address (static_cast<base_type const *> (ptr));
  }

  engine::slot_t *slot_;
};

namespace detail
{
struct holder_access
{
  static hazard_pointer
  make (engine::slot_t *slot) LUMEX_NOEXCEPT
  {
    return hazard_pointer (slot);
  }

  static engine::slot_t *&
  slot (hazard_pointer &holder) LUMEX_NOEXCEPT
  {
    return holder.slot_;
  }
};
} // namespace detail

/**
 * @brief Makes a holder that owns a slot ([saferecl.hp.holder.nonmem]).
 * @throws std::bad_alloc when a new block of slots cannot be allocated.
 */
inline hazard_pointer
make_hazard_pointer ()
{
  return detail::holder_access::make (engine::acquire_slot ());
}

/// Exchanges two holders.
inline void
swap (hazard_pointer &lhs, hazard_pointer &rhs) LUMEX_NOEXCEPT
{
  lhs.swap (rhs);
}

/**
 * @brief Runs a reclamation pass now (extension of this module).
 * @details Every retired object that no hazard pointer protects is reclaimed
 * before the call returns. Called from a deleter, it returns at once.
 */
inline void
clean_up () LUMEX_NOEXCEPT
{
  engine::clean_up ();
}
} // namespace hazard_pointer
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_HAZARD_POINTER_HOLDER_HAZARD_POINTER_HOLDER_HPP
