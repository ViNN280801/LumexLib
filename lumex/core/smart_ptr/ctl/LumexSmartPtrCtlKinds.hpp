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
 * @file LumexSmartPtrCtlKinds.hpp
 * @brief The control blocks `shared_ptr` creates: `ctl_ptr` (a separate
 * object with a deleter), `ctl_inplace` (the object lives in the block) and
 * `ctl_holder` (an alias that the split-count engine cannot pack).
 * @details
 * - `ctl_ptr<P, D, A>` manages a pointer `P` (`Y *`, or `std::nullptr_t`)
 *   with a deleter `D` and an allocator `A`, both stored with the empty-base
 *   optimization. It answers `query` for the type of `D`, which is
 *   `get_deleter`. The block is allocated with `A` rebound to the block type;
 *   `shared_ptr (Y *)` uses `block_allocator`.
 * - `ctl_inplace<T, A>` is the single allocation of `make_shared` and
 *   `allocate_shared`: the object is constructed in the block through
 *   `allocator_traits<A>::construct` (an allocator rebound to the value type)
 *   and destroyed through `allocator_traits<A>::destroy`. `get_deleter`
 *   finds nothing in it, as in the standard libraries.
 * - `ctl_holder` is a block that owns one strong reference to another block
 *   and the stored pointer of an alias. The engine installs it in an atomic
 *   smart pointer when the pointer is too far from the anchor of its block
 *   to be packed; a reader ticks the holder, reads `owner ()` and
 *   `pointer ()`, and takes its count on the owner. Disposing the holder
 *   drops the owner reference.
 * - `ctl_weak_holder` is the twin for weak pointers: it owns one weak
 * reference to another block (`add_weak` in `create`, `release_weak` in
 * `dispose`).
 *
 * Allocators must have raw pointers as `allocator_traits<A>::pointer`; fancy
 * pointers are not supported (a `static_assert` says so). The allocator is
 * copied into the block and a rebound copy is used to deallocate, as the
 * standard requires.
 */
#ifndef LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_CTL_KINDS_HPP
#define LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_CTL_KINDS_HPP

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlBase.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrTraits.hpp"
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
 * @brief Allocates one object of @p Block with the allocator @p Alloc
 * rebound to it, constructs it with @p make and frees the memory if the
 * construction throws.
 * @details @p make is a callable that takes the memory and placement-news
 * the block into it.
 */
template <class Block, class Alloc, class Make>
Block *
create_block (Alloc const &alloc, Make make)
{
  typedef typename std::allocator_traits<Alloc>::template rebind_alloc<Block>
      block_alloc;
  typedef std::allocator_traits<block_alloc> block_traits;
  static_assert (std::is_pointer<typename block_traits::pointer>::value,
                 "smart_ptr: allocators with fancy pointers are not "
                 "supported");
  block_alloc block_allocator_copy (alloc);
  Block *memory = block_traits::allocate (block_allocator_copy, 1);
  LUMEX_SMART_PTR_TRY { make (memory); }
  LUMEX_SMART_PTR_CATCH_ALL
  {
    block_traits::deallocate (block_allocator_copy, memory, 1);
    LUMEX_SMART_PTR_RETHROW;
  }
  return memory;
}

/**
 * @brief Destroys @p block and deallocates it with the allocator @p alloc
 * rebound to the block type.
 * @details The allocator is copied before the block is destroyed, because it
 * lives inside the block.
 */
template <class Block, class Alloc>
void
destroy_block (Block *block, Alloc const &alloc) LUMEX_NOEXCEPT
{
  typedef typename std::allocator_traits<Alloc>::template rebind_alloc<Block>
      block_alloc;
  block_alloc block_allocator_copy (alloc);
  block->~Block ();
  std::allocator_traits<block_alloc>::deallocate (block_allocator_copy, block,
                                                  1);
}

/**
 * @brief A control block over a pointer, a deleter and an allocator.
 * @tparam P The managed pointer type: `Y *` or `std::nullptr_t`.
 * @tparam D The deleter type; `d (p)` is called when the strong group ends.
 * @tparam A The allocator type the block was allocated with.
 */
template <class P, class D, class A>
class ctl_ptr final : public ctl_base,
                      private ebo_member<D, 0>,
                      private ebo_member<A, 1>
{
public:
  /**
   * @brief Allocates and constructs a block.
   * @details Throws what the allocator or the move constructor of the
   * deleter throws; the deleter is not called then (the caller does, as the
   * standard specifies).
   */
  static ctl_ptr *
  create (P pointer, D &&deleter, A const &alloc)
  {
    return create_block<ctl_ptr> (
        alloc, [&] (void *memory)
          { ::new (memory) ctl_ptr (pointer, std::move (deleter), alloc); });
  }

  void *
  query (void const *key) LUMEX_NOEXCEPT override
  {
    return !is_implicit_deleter<D>::value && key == type_id<D> ()
               ? static_cast<void *> (std::addressof (deleter_ref ()))
               : nullptr;
  }

private:
  ctl_ptr (P pointer, D &&deleter, A const &alloc)
      : ctl_base (), ebo_member<D, 0> (std::move (deleter)),
        ebo_member<A, 1> (alloc), pointer_ (pointer)
  {
  }

  D &
  deleter_ref () LUMEX_NOEXCEPT
  {
    return ebo_member<D, 0>::get ();
  }

  A &
  allocator_ref () LUMEX_NOEXCEPT
  {
    return ebo_member<A, 1>::get ();
  }

  void
  dispose () LUMEX_NOEXCEPT override
  {
    deleter_ref () (pointer_);
  }

  void
  destroy () LUMEX_NOEXCEPT override
  {
    A alloc_copy (allocator_ref ());
    destroy_block (this, alloc_copy);
  }

  P pointer_;
};

/**
 * @brief The control block of `make_shared` / `allocate_shared`: block and
 * object in one allocation.
 * @tparam T The object type (cv-qualifiers allowed, not an array).
 * @tparam A The allocator type.
 */
template <class T, class A>
class ctl_inplace final : public ctl_base, private ebo_member<A, 1>
{
public:
  typedef typename std::remove_cv<T>::type value_type;

  /**
   * @brief Allocates a block and constructs the object in it from @p args.
   * @details The object is constructed through `allocator_traits::construct`
   * of the allocator rebound to `value_type`. If its constructor throws the
   * memory is returned to the allocator and the exception propagates.
   */
  template <class... Args>
  static ctl_inplace *
  create (A const &alloc, Args &&...args)
  {
    ctl_inplace *created = nullptr;
    create_block<ctl_inplace> (alloc,
                               [&] (void *memory)
                                 {
                                   created = ::new (memory) ctl_inplace (
                                       alloc, std::forward<Args> (args)...);
                                 });
    return created;
  }

  /// The object.
  value_type *
  object () LUMEX_NOEXCEPT
  {
    return std::addressof (storage_.value);
  }

private:
  typedef typename std::allocator_traits<A>::template rebind_alloc<value_type>
      value_alloc;
  typedef std::allocator_traits<value_alloc> value_traits;

  // A union keeps the storage aligned for value_type and the object
  // unconstructed until the block constructor builds it.
  union storage_t
  {
    storage_t () {}
    ~storage_t () {}
    value_type value;
  };

  template <class... Args>
  explicit ctl_inplace (A const &alloc, Args &&...args)
      : ctl_base (), ebo_member<A, 1> (alloc)
  {
    value_alloc object_alloc (alloc);
    value_traits::construct (object_alloc, object (),
                             std::forward<Args> (args)...);
  }

  A &
  allocator_ref () LUMEX_NOEXCEPT
  {
    return ebo_member<A, 1>::get ();
  }

  void
  dispose () LUMEX_NOEXCEPT override
  {
    value_alloc object_alloc (allocator_ref ());
    value_traits::destroy (object_alloc, object ());
  }

  void
  destroy () LUMEX_NOEXCEPT override
  {
    A alloc_copy (allocator_ref ());
    destroy_block (this, alloc_copy);
  }

  storage_t storage_;
};

/**
 * @brief A block that owns one strong reference to another block and the
 * stored pointer of an alias (see the file text).
 */
class ctl_holder final : public ctl_base
{
public:
  /**
   * @brief Creates a holder for @p owner and @p pointer.
   * @details Takes one strong reference on @p owner (`add_strong`), which
   * the holder drops when it is disposed. Throws `std::bad_alloc`.
   */
  static ctl_holder *
  create (ctl_base *owner, void *pointer)
  {
    void *memory
        = allocate_aligned (sizeof (ctl_holder), alignof (ctl_holder));
    LUMEX_SMART_PTR_TRY
    {
      ctl_holder *holder = ::new (memory) ctl_holder (owner, pointer);
      owner->add_strong ();
      return holder;
    }
    LUMEX_SMART_PTR_CATCH_ALL
    {
      deallocate_aligned (memory, alignof (ctl_holder));
      LUMEX_SMART_PTR_RETHROW;
    }
    return nullptr;
  }

  /// The block whose strong reference the holder owns.
  ctl_base *
  owner () const LUMEX_NOEXCEPT
  {
    return owner_;
  }

  /// The stored pointer of the alias.
  void *
  pointer () const LUMEX_NOEXCEPT
  {
    return pointer_;
  }

private:
  ctl_holder (ctl_base *owner, void *pointer) LUMEX_NOEXCEPT
      : ctl_base (),
        owner_ (owner),
        pointer_ (pointer)
  {
  }

  void
  dispose () LUMEX_NOEXCEPT override
  {
    owner_->release_strong ();
  }

  void
  destroy () LUMEX_NOEXCEPT override
  {
    this->~ctl_holder ();
    deallocate_aligned (this, alignof (ctl_holder));
  }

  ctl_base *owner_;
  void *pointer_;
};

/**
 * @brief The holder of an alias that the engine of `atomic_weak_ptr` cannot
 * pack: it owns one WEAK reference to another block and the stored pointer.
 * @details The twin of `ctl_holder` for weak pointers: `create` takes a weak
 * reference on @p owner (`add_weak`), disposing the holder drops it
 * (`release_weak`), so the holder keeps the owner's block, not its object. The
 * holder's own counters are used like those of `ctl_holder`: the atomic
 * object owns one strong reference to the holder, readers tick it and settle
 * on its strong group.
 */
class ctl_weak_holder final : public ctl_base
{
public:
  /**
   * @brief Creates a holder for @p owner and @p pointer.
   * @details Takes one weak reference on @p owner (`add_weak`), which the
   * holder drops when it is disposed. Throws `std::bad_alloc`.
   */
  static ctl_weak_holder *
  create (ctl_base *owner, void *pointer)
  {
    void *memory = allocate_aligned (sizeof (ctl_weak_holder),
                                     alignof (ctl_weak_holder));
    LUMEX_SMART_PTR_TRY
    {
      ctl_weak_holder *holder
          = ::new (memory) ctl_weak_holder (owner, pointer);
      owner->add_weak ();
      return holder;
    }
    LUMEX_SMART_PTR_CATCH_ALL
    {
      deallocate_aligned (memory, alignof (ctl_weak_holder));
      LUMEX_SMART_PTR_RETHROW;
    }
    return nullptr;
  }

  /// The block whose weak reference the holder owns.
  ctl_base *
  owner () const LUMEX_NOEXCEPT
  {
    return owner_;
  }

  /// The stored pointer of the alias.
  void *
  pointer () const LUMEX_NOEXCEPT
  {
    return pointer_;
  }

private:
  ctl_weak_holder (ctl_base *owner, void *pointer) LUMEX_NOEXCEPT
      : ctl_base (),
        owner_ (owner),
        pointer_ (pointer)
  {
  }

  void
  dispose () LUMEX_NOEXCEPT override
  {
    owner_->release_weak ();
  }

  void
  destroy () LUMEX_NOEXCEPT override
  {
    this->~ctl_weak_holder ();
    deallocate_aligned (this, alignof (ctl_weak_holder));
  }

  ctl_base *owner_;
  void *pointer_;
};
} // namespace detail
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_CTL_SMART_PTR_CTL_KINDS_HPP
