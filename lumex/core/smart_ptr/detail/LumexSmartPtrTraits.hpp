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
 * @file LumexSmartPtrTraits.hpp
 * @brief Building blocks of the smart pointers: the type identity without
 * RTTI, the empty-base storage of deleters and allocators, the allocator the
 * pointers use by default, and the compatibility traits of
 * [util.smartptr.shared.general].
 * @details Nothing here is part of the public interface; the names are in
 * `lumex::core::smart_ptr::detail`.
 *
 * - `type_id<T> ()` returns the address of a writable static object that is
 *   unique per type. It replaces `typeid` (so `-fno-rtti` builds keep
 *   working); it is writable on purpose, because identical-code folding of
 *   MSVC and gold may merge read-only objects of equal contents. Two shared
 *   libraries that each instantiate it get two different ids, so
 *   `get_deleter` does not see a deleter that was stored by the other one.
 * - `ebo_member<T, Tag>` stores a `T`; an empty, non-final `T` takes no
 *   space (empty base optimization). The tag keeps two members of the same
 *   type apart.
 * - `block_allocator<T>` is the allocator of the control blocks that are not
 *   given one (`shared_ptr (Y *)`, `make_shared`). It allocates with
 *   `operator new`, honours the alignment of `T` on every standard (the
 *   aligned `operator new` of C++17 where the toolchain has it, an
 *   over-allocation with the original pointer stored in front of the block
 *   below), and is stateless.
 * - `is_new_compatible<Y, T>` and `is_ptr_compatible<Y, T>` are the
 *   "compatible with" relations of the standard: the first for a pointer
 *   `Y *` that a `shared_ptr<T>` takes over, the second for a conversion from
 *   `shared_ptr<Y>`. Array forms follow the C++17 text: `Y (*)[N]` converts
 *   to `T (*)[]` here also below C++20, where the language does not allow
 *   that conversion.
 */
#ifndef LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_TRAITS_HPP
#define LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_TRAITS_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace smart_ptr
{
namespace detail
{
/// Maps any list of types to `void` (the C++17 `void_t` for C++11).
template <class...> struct make_void
{
  typedef void type;
};

/// Static storage whose address identifies the type `T` (see the file text).
template <class T> class type_id_storage
{
public:
  static char value;
};

template <class T> char type_id_storage<T>::value = 0;

/// The identity of `T`: the same pointer for the same type, in one program.
template <class T>
void const *
type_id () LUMEX_NOEXCEPT
{
  return &type_id_storage<T>::value;
}

/// True for a class declared `final` (the compiler intrinsic; false where
/// there is none).
template <class T> struct is_final_class
{
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
  static bool const value = __is_final (T);
#else
  static bool const value = false;
#endif
};

/**
 * @brief Stores a `T` that is usually empty (a deleter, an allocator).
 * @details The primary template holds a member. The specialization for an
 * empty, non-final `T` derives from it privately, so the member takes no
 * space.
 * @tparam T The stored type.
 * @tparam Tag Distinguishes two members of the same type in one class.
 */
template <class T, int Tag,
          bool = std::is_empty<T>::value && !is_final_class<T>::value>
class ebo_member
{
public:
  explicit ebo_member (T const &value) : value_ (value) {}
  explicit ebo_member (T &&value) : value_ (std::move (value)) {}

  T &
  get () LUMEX_NOEXCEPT
  {
    return value_;
  }

private:
  T value_;
};

template <class T, int Tag> class ebo_member<T, Tag, true> : private T
{
public:
  explicit ebo_member (T const &value) : T (value) {}
  explicit ebo_member (T &&value) : T (std::move (value)) {}

  T &
  get () LUMEX_NOEXCEPT
  {
    return *this;
  }
};

/// Alignment `operator new` honours without the aligned overloads.
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
#define LUMEX_SMART_PTR_DEFAULT_NEW_ALIGNMENT __STDCPP_DEFAULT_NEW_ALIGNMENT__
#else
#define LUMEX_SMART_PTR_DEFAULT_NEW_ALIGNMENT alignof (std::max_align_t)
#endif

/// Allocates @p bytes aligned to @p alignment (a power of two); throws
/// `std::bad_alloc`.
inline void *
allocate_aligned (std::size_t bytes, std::size_t alignment)
{
  if (alignment <= LUMEX_SMART_PTR_DEFAULT_NEW_ALIGNMENT)
    return ::operator new (bytes);
#if defined(__cpp_aligned_new)
  return ::operator new (bytes, std::align_val_t (alignment));
#else
  // Over-allocate, align inside the block and keep the pointer that
  // operator new returned in the word in front of the aligned address.
  std::size_t const extra = alignment + sizeof (void *);
  if (bytes > (std::numeric_limits<std::size_t>::max) () - extra)
    LUMEX_SMART_PTR_THROW (std::bad_alloc ());
  void *const raw = ::operator new (bytes + extra);
  std::uintptr_t address
      = reinterpret_cast<std::uintptr_t> (raw) + sizeof (void *);
  address = (address + alignment - 1)
            & ~(static_cast<std::uintptr_t> (alignment) - 1);
  reinterpret_cast<void **> (address)[-1] = raw;
  return reinterpret_cast<void *> (address);
#endif
}

/// Releases memory from `allocate_aligned` with the same @p alignment.
inline void
deallocate_aligned (void *block, std::size_t alignment) LUMEX_NOEXCEPT
{
  if (alignment <= LUMEX_SMART_PTR_DEFAULT_NEW_ALIGNMENT)
    {
      ::operator delete (block);
      return;
    }
#if defined(__cpp_aligned_new)
  ::operator delete (block, std::align_val_t (alignment));
#else
  ::operator delete (reinterpret_cast<void **> (block)[-1]);
#endif
}

/**
 * @brief The allocator of control blocks that no allocator was given for.
 * @details A minimal C++11 allocator: stateless, always equal, no
 * construct/destroy of its own (the `allocator_traits` defaults place the
 * object with `::new`, as `make_shared` is specified to do). The block and
 * the object of `make_shared` are one allocation of `block_allocator`.
 */
template <class T> class block_allocator
{
public:
  typedef T value_type;

  block_allocator () LUMEX_NOEXCEPT {}

  template <class U>
  block_allocator (block_allocator<U> const &) LUMEX_NOEXCEPT
  {
  }

  T *
  allocate (std::size_t count)
  {
    if (count > (std::numeric_limits<std::size_t>::max) () / sizeof (T))
      LUMEX_SMART_PTR_THROW (std::bad_alloc ());
    return static_cast<T *> (
        allocate_aligned (count * sizeof (T), alignof (T)));
  }

  void
  deallocate (T *block, std::size_t) LUMEX_NOEXCEPT
  {
    deallocate_aligned (block, alignof (T));
  }
};

template <class T, class U>
bool
operator== (block_allocator<T> const &,
            block_allocator<U> const &) LUMEX_NOEXCEPT
{
  return true;
}

template <class T, class U>
bool
operator!= (block_allocator<T> const &,
            block_allocator<U> const &) LUMEX_NOEXCEPT
{
  return false;
}

/// True when `Y *` (a pointer that is handed to `shared_ptr<T>`) is
/// convertible to the stored pointer type of `shared_ptr<T>`.
template <class Y, class T>
struct is_new_compatible : std::is_convertible<Y *, T *>
{
};

template <class Y, class U>
struct is_new_compatible<Y, U[]> : std::is_convertible<Y (*)[], U (*)[]>
{
};

template <class Y, class U, std::size_t N>
struct is_new_compatible<Y, U[N]> : std::is_convertible<Y (*)[N], U (*)[N]>
{
};

/// True when `shared_ptr<Y>` converts to `shared_ptr<T>` (Y and T may be
/// array types).
template <class Y, class T>
struct is_ptr_compatible : std::is_convertible<Y *, T *>
{
};

template <class Y, class U, std::size_t N>
struct is_ptr_compatible<Y[N], U[]> : std::is_convertible<Y (*)[], U (*)[]>
{
};

/// True when `d (p)` is well-formed for a `D` lvalue and a `P` lvalue.
template <class D, class P, class = void>
struct is_deleter_for : std::false_type
{
};

template <class D, class P>
struct is_deleter_for<D, P,
                      typename make_void<decltype (std::declval<D &> () (
                          std::declval<P &> ()))>::type> : std::true_type
{
};

/// `delete p` for a single object.
template <class Y> class default_deleter
{
public:
  void
  operator() (Y *p) const LUMEX_NOEXCEPT
  {
    static_assert (sizeof (Y) > 0, "smart_ptr: the deleted type must be "
                                   "complete");
    delete p;
  }
};

/// `delete[] p` for an array.
template <class Y> class default_array_deleter
{
public:
  void
  operator() (Y *p) const LUMEX_NOEXCEPT
  {
    static_assert (sizeof (Y) > 0, "smart_ptr: the deleted type must be "
                                   "complete");
    delete[] p;
  }
};

/// True for the deleters of `shared_ptr (Y *)` itself: they are not reported
/// by `get_deleter`, as the implicit `delete` of the standard is not.
template <class D> struct is_implicit_deleter : std::false_type
{
};

template <class Y>
struct is_implicit_deleter<default_deleter<Y>> : std::true_type
{
};

template <class Y>
struct is_implicit_deleter<default_array_deleter<Y>> : std::true_type
{
};

/// The deleter `shared_ptr<T> (Y *)` uses: `delete[]` for array `T`.
template <class T, class Y> struct default_deleter_for
{
  typedef default_deleter<Y> type;
};

template <class U, class Y> struct default_deleter_for<U[], Y>
{
  typedef default_array_deleter<Y> type;
};

template <class U, std::size_t N, class Y> struct default_deleter_for<U[N], Y>
{
  typedef default_array_deleter<Y> type;
};
} // namespace detail
} // namespace smart_ptr
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_TRAITS_HPP
