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
 * @file LumexSmartPtrCtlBase.hpp
 * @brief `ctl_base`: the control block of `shared_ptr` and `weak_ptr`, 32
 * bytes on a 64-bit target.
 * @details Layout: the virtual table pointer (8), the strong counter (8), the
 * weak ledger (8) and the anchor (8). The two counters are packed
 * `split_counter` words (see `LumexSmartPtrCounter.hpp`). Counts start at 1
 * strong owner and 1 weak count: the weak count carries one implicit unit on
 * behalf of the whole group of strong owners, which is dropped when the
 * object is disposed, as in the standard libraries. The block is deallocated
 * by the one that brings the weak ledger to zero.
 *
 * The anchor is the integer value of the first stored pointer the block was
 * created for (the object address for `make_shared`, the converted pointer
 * for `shared_ptr<Base> (new Derived)`); the split-count engine packs the
 * difference of a stored pointer and the anchor (an aliasing pointer, a base
 * conversion) into its atomic word. The pointers themselves never read it.
 *
 * Two virtual functions finish a block: `dispose ()` destroys the managed
 * object (and releases what the block holds for it) when the strong group
 * ends, `destroy ()` deallocates the block when the weak ledger ends. Both are
 * private and are called only through `release_strong`, `settle_strong`,
 * `release_weak` and `settle_weak`. `query (key)` is the type-erased lookup
 * behind `get_deleter`: it returns a pointer to the stored object whose
 * `type_id` is @p key, or null. Block kinds that recognise each other (the
 * interop with `std::shared_ptr`) use their own keys.
 *
 * The interface below is the one the split-count engine of `core/atomic`
 * builds on (the documented `detail` surface of the module):
 * `add_strong`, `release_strong`, `transfer_strong_ext`, `settle_strong`,
 * `try_add_strong` and the same four for the weak ledger. `release_strong`
 * and `settle_strong` dispose the object only when `count` and `ext` are both
 * zero, so a block that a pinned reader or a pending transfer still refers to
 * is never disposed.
 */
#ifndef LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_CTL_BASE_HPP
#define LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_CTL_BASE_HPP

#include <cstdint>

#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCounter.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
namespace detail
{
/**
 * @brief The polymorphic control block (see the file text).
 */
class ctl_base
{
public:
  ctl_base (ctl_base const &) = delete;
  ctl_base &operator= (ctl_base const &) = delete;

  // -- Strong group --

  /// Adds one strong owner. The caller owns a count or the block is pinned.
  void
  add_strong () LUMEX_NOEXCEPT
  {
    strong_.add ();
  }

  /// Promotion of a weak pointer: adds one strong owner unless the object is
  /// gone. True when an owner was added.
  bool
  try_add_strong () LUMEX_NOEXCEPT
  {
    return strong_.try_add ();
  }

  /// Drops one strong owner; disposes the object and drops the implicit weak
  /// unit when `count` and `ext` both reach zero.
  void
  release_strong () LUMEX_NOEXCEPT
  {
    if (strong_.release ())
      finish_strong ();
  }

  /// Adds @p n to the strong `ext` (the engine's transfer of swapped-out
  /// ticks; call it before the writer drops its own count).
  void
  transfer_strong_ext (std::uint32_t n) LUMEX_NOEXCEPT
  {
    strong_.transfer_ext (n);
  }

  /// Pays one unit of the strong `ext` back (the engine's settle); disposes
  /// the object when this makes the word zero.
  void
  settle_strong () LUMEX_NOEXCEPT
  {
    if (strong_.settle ())
      finish_strong ();
  }

  /// The number of strong owners (see `split_counter::use_count`).
  long
  use_count () const LUMEX_NOEXCEPT
  {
    return strong_.use_count ();
  }

  // -- Weak ledger --

  /// Adds one weak pointer.
  void
  add_weak () LUMEX_NOEXCEPT
  {
    weak_.add ();
  }

  /// Drops one weak pointer; deallocates the block when the ledger reaches
  /// zero.
  void
  release_weak () LUMEX_NOEXCEPT
  {
    if (weak_.release ())
      destroy ();
  }

  /// Adds @p n to the weak `ext` (the engine of the weak pointers).
  void
  transfer_weak_ext (std::uint32_t n) LUMEX_NOEXCEPT
  {
    weak_.transfer_ext (n);
  }

  /// Pays one unit of the weak `ext` back; deallocates the block when this
  /// makes the ledger zero.
  void
  settle_weak () LUMEX_NOEXCEPT
  {
    if (weak_.settle ())
      destroy ();
  }

  /**
   * @brief The number of weak pointers, without the implicit unit of the
   * strong group.
   * @details Exact at quiescence; used by the tests and by diagnostics.
   */
  long
  weak_count () const LUMEX_NOEXCEPT
  {
    long const count = split_counter::count_of (weak_.load ());
    return strong_.load () != 0 ? count - 1 : count;
  }

  // -- Access to the words (the engine and the tests) --

  /// The strong counter word.
  split_counter &
  strong_counter () LUMEX_NOEXCEPT
  {
    return strong_;
  }

  /// The strong counter word.
  split_counter const &
  strong_counter () const LUMEX_NOEXCEPT
  {
    return strong_;
  }

  /// The weak ledger word.
  split_counter &
  weak_counter () LUMEX_NOEXCEPT
  {
    return weak_;
  }

  /// The weak ledger word.
  split_counter const &
  weak_counter () const LUMEX_NOEXCEPT
  {
    return weak_;
  }

  // -- Anchor --

  /// The anchor (see the file text); 0 until a pointer sets it.
  std::uintptr_t
  anchor () const LUMEX_NOEXCEPT
  {
    return anchor_;
  }

  /// Sets the anchor. Called once, by the constructor that creates the
  /// block, before the block is shared.
  void
  set_anchor (std::uintptr_t value) LUMEX_NOEXCEPT
  {
    anchor_ = value;
  }

  // -- Type-erased lookup --

  /// Pointer to the stored object whose `type_id` is @p key (a deleter), or
  /// null. The default knows none.
  virtual void *
  query (void const *key) LUMEX_NOEXCEPT
  {
    static_cast<void> (key);
    return nullptr;
  }

protected:
  ctl_base () LUMEX_NOEXCEPT : strong_ (1), weak_ (1), anchor_ (0) {}

  // Blocks are finished by dispose () and destroy (); nobody deletes
  // through the base.
  ~ctl_base () = default;

private:
  /// Destroys the managed object. Runs once, from the strong group's end.
  virtual void dispose () LUMEX_NOEXCEPT = 0;

  /// Deallocates the block. Runs once, from the weak ledger's end.
  virtual void destroy () LUMEX_NOEXCEPT = 0;

  void
  finish_strong () LUMEX_NOEXCEPT
  {
    dispose ();
    release_weak ();
  }

  split_counter strong_;
  split_counter weak_;
  std::uintptr_t anchor_;
};
} // namespace detail
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_CTL_BASE_HPP
