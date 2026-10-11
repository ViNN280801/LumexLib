/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file CircularBuffer.hpp
 * @brief `circular_buffer`, a ring buffer with a run-time capacity that
 * overwrites its oldest element when full, modeled on Boost's
 * `circular_buffer`.
 * @details Header-only (`lumex::circular_buffer` is an interface target) and
 * written for C++11: the class template, its random-access iterators and the
 * non-member comparison operators, with `operator<=>` added when the compiler
 * supports three-way comparison and the standard concepts library (C++20). The
 * class documentation lists the iterator invalidation, complexity and
 * exception guarantees of each operation.
 */
#ifndef LUMEX_CORE_CIRCULAR_BUFFER_BUFFER_HPP
#define LUMEX_CORE_CIRCULAR_BUFFER_BUFFER_HPP

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-type-reinterpret-cast)

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<compare>)
#include <compare> // For std::three_way_comparable and the ordering types
#endif
#endif
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<concepts>)
#include <concepts> // For the standard concepts library
#endif
#endif

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace circular_buffer
{
/**
 * @brief Circular buffer with a run-time capacity that overwrites when full
 * (similar to Boost's `circular_buffer`).
 *
 * @tparam T Element type stored in the buffer.
 * @tparam Allocator Allocator type; default `std::allocator<T>`.
 *
 * This container mimics the main behavior of Boost `circular_buffer`
 * while staying C++11 and header-only. The capacity is set at construction
 * (or by `set_capacity`, `rset_capacity`, `resize` and `assign`) and may be 0:
 * a buffer with capacity 0 is valid, holds nothing and ignores `push_*`.
 *
 * @section characteristics Characteristics
 *  - Insert into a full buffer overwrites the oldest element.
 *  - `T` need not be default-constructible (elements are created on demand).
 *  - Strong exception safety for insert on a non-full buffer and, when `T` is
 *    move-constructible, for the construction part of an overwrite (see
 *    below).
 *  - Random access at indexes (`0..size()-1`), where `0` is the oldest
 *    element.
 *  - `emplace_back` / `emplace_front` need room for the new element and throw
 *    `std::length_error` on a buffer with capacity 0 (they return a
 * reference); `push_back` / `push_front` ignore their argument then.
 *  - Not thread-safe.
 * @section guarantees Guarantees, invalidation, and complexity
 *
 * @subsection invalidation Pointer/iterator/reference invalidation
 * Iterators store a logical index, so any operation
 * that moves the head remaps "logical index -> object".
 * Those operations invalidate every iterator. The objects themselves do not
 * move.
 *
 * Per operation:
 *  - `emplace_back` / `push_back` on a non-full buffer:
 *      - iterators: remain valid (except `end()`, which changes);
 *      - references/pointers to existing elements: remain valid.
 *  - `emplace_front` / `push_front` on a non-full buffer:
 *      - iterators: all invalidated (head changes, logical indexes shift);
 *      - references/pointers: remain valid (objects do not move).
 *  - `emplace_back` / `push_back` on a full buffer (overwrite the oldest):
 *      - iterators: all invalidated (head changes);
 *      - references/pointers: the destroyed front element is invalid; the rest
 * stay valid.
 *  - `emplace_front` / `push_front` on a full buffer (overwrite the newest):
 *      - iterators: all invalidated (head changes);
 *      - references/pointers: the destroyed back element is invalid; the rest
 * stay valid.
 *  - `pop_front`:
 *      - iterators: all invalidated (head changes);
 *      - references/pointers: the removed element is invalid; the rest stay
 * valid.
 *  - `pop_back`:
 *      - iterators: iterators to the last element and `end()` are invalidated;
 * the rest stay valid;
 *      - references/pointers: the removed element is invalid; the rest stay
 * valid.
 *  - `insert`, `erase`, `erase_begin`, `erase_end`, `resize`: every iterator
 * is invalidated; elements are moved by assignment, so references and pointers
 * to the elements that stay are valid but refer to other values.
 *  - `set_capacity`, `rset_capacity`, `linearize` (when it has to move
 * elements), `assign`: every iterator, reference and pointer is invalidated.
 *  - `clear`:
 *      - iterators/references/pointers: all invalidated.
 *  - `swap`:
 *      - iterators: all invalidated (they are bound to this container object);
 *      - references/pointers: still point at the same objects, now owned by
 * the other buffer.
 *
 * @subsection complexity Complexity
 * Core operations are O(1): indexed access, `front`/`back`,
 * `push_*`/`emplace_*`/`pop_*`, including overwrite. Iteration is O(n). `swap`
 * is O(1). Copy, `assign`, `set_capacity`, `rset_capacity`, `resize`,
 * `insert` and `erase` are linear.
 *
 * @subsection exceptions Exception guarantees
 *  - Construct/insert on a non-full buffer: strong guarantee
 * (commit-or-rollback).
 *  - Overwrite (full buffer): the new element is first built in a temporary,
 * so when its construction throws the buffer is unchanged. Then the replaced
 * element is destroyed and the new one is moved into its place; if that move
 * throws the buffer stays consistent and holds one element fewer (basic). For
 * a `T` that is not move-constructible the replaced element is destroyed
 * first, then the new one is constructed in place (basic, and the arguments
 * must not refer to the replaced element).
 *  - Copy construction and copy assignment: strong (nothing leaks, `*this` is
 * unchanged when a copy throws).
 *  - `set_capacity`, `rset_capacity`, `linearize`, `assign`: strong.
 *  - `insert`, `erase`, `resize`: basic (the buffer stays valid); `resize` and
 * `insert` roll back the elements they constructed when a construction throws.
 *  - `pop_*`, `clear`, `swap` do not throw (as declared `noexcept`/macros).
 *
 * @subsection threads Thread safety
 * The container is not thread-safe. Concurrent access from several threads
 * needs external synchronization.
 *
 * @subsection iterators Iterators
 * Iterators are random-access (they model `std::random_access_iterator` from
 * C++20). Because they store an index, any change of `head` makes them
 * inconsistent with the original elements (see invalidation). `iterator`
 * converts to `const_iterator` and the two compare with each other.
 *
 * @subsection contiguity Memory contiguity
 *             The container does not promise contiguous `T` like
 * `std::vector`; `linearize` makes the content contiguous in the storage and
 * returns a pointer to it, `is_linearized` tells whether it already is. There
 * is no `data()`. Addresses of live elements stay stable across operations
 * that do not destroy that element; overwrite/erase invalidates the address.
 *
 * @subsection ordering Comparison
 *  - `==`, `!=`, `<`, `<=`, `>`, `>=` are lexicographical on the logical
 * sequence; they require the matching operations on `T`.
 *  - Operator `<=>` is available in C++20+ for any `T` that is
 * `std::three_way_comparable`; it returns `std::compare_three_way_result_t<T>`
 * (strong for `int`, partial for `double`).
 *
 * @subsection allocator Allocator, copy, move and swap
 * Copy construction uses
 * <tt>std::allocator_traits\<Allocator\>\::select_on_container_copy_construction</tt>.
 * Copy assignment follows
 * <tt>std::allocator_traits\<Allocator\>\::propagate_on_container_copy_assignment</tt>:
 * when it is `true` the allocator of the source is taken (the old storage is
 * released with the old allocator); otherwise the own allocator is kept and
 * the new storage is made with it. The move operations always move the
 * allocator along with the storage. On `swap`, allocators are exchanged only
 * if
 * <tt>std::allocator_traits\<Allocator\>\::propagate_on_container_swap\::value</tt>
 * is `true`. Otherwise they are assumed equivalent (as for the standard
 * containers; a debug build asserts it); elements do not move.
 */
template <class T, class Allocator = std::allocator<T>> class circular_buffer
{
  using alloc_traits = std::allocator_traits<Allocator>;

  /// Tag of the private constructor that only allocates the storage.
  struct alloc_tag
  {
  };

  /// Tag of the private constructors that fill the buffer from a range.
  struct range_tag
  {
  };

public:
  // ============= public member types =============

  using value_type = T; ///< Element type stored in the buffer.
  using allocator_type
      = Allocator; ///< Allocator type used to manage element memory.
  using size_type = std::size_t; ///< Type used for size or element count.
  using difference_type = std::ptrdiff_t; ///< Type used for the difference
                                          ///< between two iterators.
  using reference = value_type &;         ///< Reference to a buffer element.
  using const_reference
      = value_type const &; ///< Const reference to a buffer element.
  using pointer =
      typename alloc_traits::pointer; ///< Pointer to a buffer element.
  using const_pointer =
      typename alloc_traits::const_pointer; ///< Const pointer to a buffer
                                            ///< element.

private:
  // Uninitialized storage for T elements
  struct Slot
  {
    alignas (T) unsigned char data[sizeof (T)];
  };

  LUMEX_STATIC_ASSERT_MSG (alignof (Slot) >= alignof (T),
                           "Slot alignment must be >= T alignment");
  LUMEX_STATIC_ASSERT_MSG (sizeof (Slot) >= sizeof (T),
                           "Slot size must be >= sizeof(T)");

  using slot_allocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Slot>;
  using slot_traits = std::allocator_traits<slot_allocator>;

  LUMEX_CONST_NUM bool is_swap_noexcept
      = (std::allocator_traits<Allocator>::propagate_on_container_swap::value
             ? (LUMEX_NOEXCEPT_IF (std::swap (std::declval<Allocator &> (),
                                              std::declval<Allocator &> ()))
                && LUMEX_NOEXCEPT_IF (
                    std::swap (std::declval<slot_allocator &> (),
                               std::declval<slot_allocator &> ())))
             : std::allocator_traits<Allocator>::is_always_equal::value);

  /// Enabled for the types that are at least input iterators (not integers).
  template <class It>
  using enable_if_input_iterator = typename std::enable_if<
      std::is_convertible<typename std::iterator_traits<It>::iterator_category,
                          std::input_iterator_tag>::value>::type;

public:
  /**
   * @brief Random-access iterator over the logical element sequence.
   *
   * `iterator` and `const_iterator` are the two instances of this template.
   * The iterator walks the buffer elements with random access, like a pointer
   * or an iterator of `std::vector`; `const_iterator` cannot modify elements.
   * An `iterator` converts to a `const_iterator`, and the two compare with
   * each other in both directions.
   *
   * @tparam IsConst `true` for the iterator that gives const access.
   */
  template <bool IsConst> class basic_iterator
  {
    using buffer_pointer =
        typename std::conditional<IsConst, circular_buffer const *,
                                  circular_buffer *>::type;

  public:
    using iterator_category
        = std::random_access_iterator_tag; ///< Iterator category.
    using value_type = T; ///< Element type the iterator points to.
    using difference_type = std::ptrdiff_t; ///< Type used for the difference
                                            ///< between two iterators.
    using pointer = typename std::conditional<IsConst, T const *,
                                              T *>::type; ///< Pointer type.
    using reference =
        typename std::conditional<IsConst, T const &,
                                  T &>::type; ///< Reference type.

    /// @brief Default constructor. Creates a singular iterator.
    basic_iterator () : m_buf (nullptr), m_idx (0) {}

    /// @brief Conversion from the non-const iterator to the const iterator.
    /// @param other Source iterator.
    template <bool OtherConst,
              class = typename std::enable_if<IsConst && !OtherConst>::type>
    basic_iterator (basic_iterator<OtherConst> const &other)
        : m_buf (other.m_buf), m_idx (other.m_idx)
    {
    }

    /**
     * @brief Dereference operator.
     * @return Reference to the element the iterator points to.
     */
    reference
    operator* () const
    {
      return (*m_buf)[m_idx];
    }

    /**
     * @brief Member-access operator.
     * @return Pointer to the element the iterator points to.
     */
    pointer
    operator->() const
    {
      return std::addressof ((*m_buf)[m_idx]);
    }

    /**
     * @brief Subscript operator.
     * @param n Offset from the iterator.
     * @return Reference to the element `n` positions after the iterator.
     */
    reference
    operator[] (difference_type n) const
    {
      return (*m_buf)[m_idx + static_cast<size_type> (n)];
    }

    /**
     * @brief Prefix increment. Advances the iterator to the next element.
     * @return Reference to this iterator after increment.
     */
    basic_iterator &
    operator++ ()
    {
      ++m_idx;
      return *this;
    }

    /**
     * @brief Postfix increment. Advances the iterator to the next element.
     * @return Copy of the iterator before increment.
     */
    basic_iterator
    operator++ (int)
    {
      basic_iterator tmp (*this);
      ++*this;
      return tmp;
    }

    /**
     * @brief Prefix decrement. Moves the iterator to the previous element.
     * @return Reference to this iterator after decrement.
     */
    basic_iterator &
    operator-- ()
    {
      --m_idx;
      return *this;
    }

    /**
     * @brief Postfix decrement. Moves the iterator to the previous element.
     * @return Copy of the iterator before decrement.
     */
    basic_iterator
    operator-- (int)
    {
      basic_iterator tmp (*this);
      --*this;
      return tmp;
    }

    /**
     * @brief Addition assignment. Advances the iterator by `n` positions.
     * @param n Number of positions to move (may be negative).
     * @return Reference to this iterator after the move.
     */
    basic_iterator &
    operator+= (difference_type n)
    {
      m_idx += static_cast<size_type> (n);
      return *this;
    }

    /**
     * @brief Subtraction assignment. Moves the iterator back by `n` positions.
     * @param n Number of positions to move (may be negative).
     * @return Reference to this iterator after the move.
     */
    basic_iterator &
    operator-= (difference_type n)
    {
      m_idx -= static_cast<size_type> (n);
      return *this;
    }

    /**
     * @brief Addition. Creates a new iterator advanced by `n` positions.
     * @param n Number of positions to shift.
     * @return New iterator shifted by `n` positions.
     */
    basic_iterator
    operator+ (difference_type n) const
    {
      basic_iterator tmp (*this);
      return tmp += n;
    }

    /**
     * @brief Friend addition. Creates a new iterator advanced by `n`
     * positions.
     * @param n Number of positions to shift.
     * @param iter Source iterator.
     * @return New iterator shifted by `n` positions.
     */
    friend basic_iterator
    operator+ (difference_type n, basic_iterator iter)
    {
      return iter += n;
    }

    /**
     * @brief Subtraction. Creates a new iterator moved back by `n` positions.
     * @param n Number of positions to shift.
     * @return New iterator shifted by `n` positions.
     */
    basic_iterator
    operator- (difference_type n) const
    {
      basic_iterator tmp (*this);
      return tmp -= n;
    }

    /**
     * @brief Difference. Distance between two iterators of one buffer.
     * @param lhs The iterator at the end of the distance.
     * @param rhs The iterator at the start of the distance.
     * @return Position difference `lhs - rhs`.
     * @note Found for every mix of `iterator` and `const_iterator`.
     */
    friend difference_type
    operator- (basic_iterator const &lhs, basic_iterator const &rhs)
    {
      return static_cast<difference_type> (lhs.m_idx)
             - static_cast<difference_type> (rhs.m_idx);
    }

    /**
     * @brief Equality comparison.
     * @param lhs The first iterator.
     * @param rhs The second iterator.
     * @return `true` if both iterators point at the same element in the same
     * buffer, otherwise `false`.
     * @note Found for every mix of `iterator` and `const_iterator`.
     */
    friend bool
    operator== (basic_iterator const &lhs, basic_iterator const &rhs)
    {
      return lhs.m_buf == rhs.m_buf && lhs.m_idx == rhs.m_idx;
    }

    /**
     * @brief Inequality comparison.
     * @param lhs The first iterator.
     * @param rhs The second iterator.
     * @return `true` if the iterators point at different elements or different
     * buffers, otherwise `false`.
     */
    friend bool
    operator!= (basic_iterator const &lhs, basic_iterator const &rhs)
    {
      return !(lhs == rhs);
    }

    /**
     * @brief "less" operator. Compares the positions of two iterators.
     * @param lhs The first iterator.
     * @param rhs The second iterator.
     * @return `true` if `lhs` precedes `rhs`, otherwise `false`.
     */
    friend bool
    operator< (basic_iterator const &lhs, basic_iterator const &rhs)
    {
      return lhs.m_idx < rhs.m_idx;
    }

    /**
     * @brief "greater" operator. Compares the positions of two iterators.
     * @param lhs The first iterator.
     * @param rhs The second iterator.
     * @return `true` if `lhs` follows `rhs`, otherwise `false`.
     */
    friend bool
    operator> (basic_iterator const &lhs, basic_iterator const &rhs)
    {
      return rhs.m_idx < lhs.m_idx;
    }

    /**
     * @brief "less or equal" operator. Compares the positions of two
     * iterators.
     * @param lhs The first iterator.
     * @param rhs The second iterator.
     * @return `true` if `lhs` precedes or equals `rhs`, otherwise `false`.
     */
    friend bool
    operator<= (basic_iterator const &lhs, basic_iterator const &rhs)
    {
      return !(rhs.m_idx < lhs.m_idx);
    }

    /**
     * @brief "greater or equal" operator. Compares the positions of two
     * iterators.
     * @param lhs The first iterator.
     * @param rhs The second iterator.
     * @return `true` if `lhs` follows or equals `rhs`, otherwise `false`.
     */
    friend bool
    operator>= (basic_iterator const &lhs, basic_iterator const &rhs)
    {
      return !(lhs.m_idx < rhs.m_idx);
    }

  private:
    friend class circular_buffer;
    template <bool> friend class basic_iterator;

    /// @brief Private constructor.
    /// @param buf Pointer to the circular_buffer that owns the iterator.
    /// @param idx Logical element index.
    basic_iterator (buffer_pointer buf, size_type idx)
        : m_buf (buf), m_idx (idx)
    {
    }

    buffer_pointer m_buf; ///< Pointer to the buffer owned by this iterator.
    size_type m_idx;      ///< Logical element index in the buffer.
  };

  using iterator = basic_iterator<false>; ///< Random-access iterator.
  using const_iterator
      = basic_iterator<true>; ///< Random-access iterator to const elements.
  using reverse_iterator
      = std::reverse_iterator<iterator>; ///< Reverse-iterator type.
  using const_reverse_iterator
      = std::reverse_iterator<const_iterator>; ///< Const reverse-iterator
                                               ///< type.

  // ============= ctors / dtors =============

  /**
   * @brief Creates an empty buffer with capacity 0.
   *
   * A buffer with capacity 0 is valid: it holds nothing, `push_*` ignore their
   * argument, and `set_capacity`, `assign` or an assignment give it room.
   */
  circular_buffer () LUMEX_NOEXCEPT_IF (
      std::is_nothrow_default_constructible<allocator_type>::value)
      : circular_buffer (alloc_tag (), allocator_type (), 0)
  {
  }

  /**
   * @brief Creates an empty buffer with capacity 0 and the given allocator.
   * @param alloc Allocator used to allocate memory.
   */
  explicit circular_buffer (allocator_type const &alloc) LUMEX_NOEXCEPT
      : circular_buffer (alloc_tag (), alloc, 0)
  {
  }

  /**
   * @brief Creates an empty buffer with the given capacity.
   *
   * @param capacity Maximum number of elements the buffer can hold; may be 0.
   * @param alloc Allocator used to allocate memory.
   * @throws std::length_error if `capacity > max_size()`; whatever the
   * allocator throws.
   * @note The buffer starts empty; elements are added later.
   */
  explicit circular_buffer (size_type capacity,
                            allocator_type const &alloc = allocator_type ())
      : circular_buffer (alloc_tag (), alloc, capacity)
  {
  }

  /**
   * @brief Creates a full buffer of `n` copies of `item` (capacity `n`).
   * @param n Number of elements, also the capacity.
   * @param item Value to copy.
   * @param alloc Allocator used to allocate memory.
   * @throws Whatever the copy constructor of `T` or the allocator throws; no
   * memory leaks.
   */
  circular_buffer (size_type n, value_type const &item,
                   allocator_type const &alloc = allocator_type ())
      : circular_buffer (alloc_tag (), alloc, n)
  {
    fill_back (n, item);
  }

  /**
   * @brief Creates a buffer of the given capacity filled with `n` copies of
   * `item`.
   * @param capacity Capacity of the buffer.
   * @param n Number of elements, at most `capacity` (more are cut to the
   * capacity).
   * @param item Value to copy.
   * @param alloc Allocator used to allocate memory.
   * @throws Whatever the copy constructor of `T` or the allocator throws; no
   * memory leaks.
   */
  circular_buffer (size_type capacity, size_type n, value_type const &item,
                   allocator_type const &alloc = allocator_type ())
      : circular_buffer (alloc_tag (), alloc, capacity)
  {
    fill_back (n < capacity ? n : capacity, item);
  }

  /**
   * @brief Creates a full buffer with a copy of a range (capacity = length of
   * the range).
   * @tparam InputIt Input iterator type.
   * @param first Start of the range.
   * @param last End of the range.
   * @param alloc Allocator used to allocate memory.
   * @throws Whatever the construction of `T` from `*first` or the allocator
   * throws; no memory leaks.
   * @note An input iterator that is not a forward iterator is read once into a
   * temporary `std::vector` first.
   */
  template <class InputIt, class = enable_if_input_iterator<InputIt>>
  circular_buffer (InputIt first, InputIt last,
                   allocator_type const &alloc = allocator_type ())
      : circular_buffer (
            range_tag (), first, last, alloc,
            typename std::iterator_traits<InputIt>::iterator_category ())
  {
  }

  /**
   * @brief Creates a buffer of the given capacity with a copy of a range.
   *
   * If the range is longer than `capacity`, only its last `capacity` elements
   * are copied.
   *
   * @tparam InputIt Input iterator type.
   * @param capacity Capacity of the buffer.
   * @param first Start of the range.
   * @param last End of the range.
   * @param alloc Allocator used to allocate memory.
   * @throws Whatever the construction of `T` from `*first` or the allocator
   * throws; no memory leaks.
   */
  template <class InputIt, class = enable_if_input_iterator<InputIt>>
  circular_buffer (size_type capacity, InputIt first, InputIt last,
                   allocator_type const &alloc = allocator_type ())
      : circular_buffer (
            range_tag (), capacity, first, last, alloc,
            typename std::iterator_traits<InputIt>::iterator_category ())
  {
  }

  /**
   * @brief Creates a full buffer with the elements of a list (capacity = size
   * of the list).
   * @param init Elements, oldest first.
   * @param alloc Allocator used to allocate memory.
   */
  circular_buffer (std::initializer_list<value_type> init,
                   allocator_type const &alloc = allocator_type ())
      : circular_buffer (init.begin (), init.end (), alloc)
  {
  }

  /**
   * @brief Creates a buffer of the given capacity with the elements of a list.
   *
   * If the list is longer than `capacity`, only its last `capacity` elements
   * are kept.
   *
   * @param capacity Capacity of the buffer.
   * @param init Elements, oldest first.
   * @param alloc Allocator used to allocate memory.
   */
  circular_buffer (size_type capacity, std::initializer_list<value_type> init,
                   allocator_type const &alloc = allocator_type ())
      : circular_buffer (capacity, init.begin (), init.end (), alloc)
  {
  }

  /**
   * @brief Copy constructor.
   *
   * Creates a new circular buffer that is a copy of `other` (same capacity,
   * same logical content). The content is copied in at most two contiguous
   * pieces. The allocator is
   * `std::allocator_traits<Allocator>::select_on_container_copy_construction`
   * of the allocator of `other`.
   *
   * @param other Buffer copied from.
   * @throws Exceptions thrown by `T` copy constructors or the allocator;
   * nothing leaks when a copy throws.
   */
  circular_buffer (circular_buffer const &other)
      : circular_buffer (alloc_tag (),
                         alloc_traits::select_on_container_copy_construction (
                             other.m_allocator),
                         other.m_capacity)
  {
    append_copy_of (other);
  }

  /**
   * @brief Copy assignment operator.
   *
   * Assigns the contents of `other` to this buffer (the capacity of `other`
   * is taken). The copy is made first and swapped in, so `*this` is unchanged
   * when a copy throws. With
   * `propagate_on_container_copy_assignment` the allocator of `other` is taken
   * and the old storage is released with the old allocator; otherwise the own
   * allocator is kept and used for the new storage.
   *
   * @param other Buffer assigned from.
   * @return Reference to this buffer.
   * @throws Exceptions thrown by the copy constructor of `T` or the allocator.
   */
  circular_buffer &
  operator= (circular_buffer const &other)
  {
    if (this == &other)
      return *this;
    copy_assign (
        other,
        typename alloc_traits::propagate_on_container_copy_assignment ());
    return *this;
  }

  /**
   * @brief Move constructor.
   *
   * Creates a new circular buffer by moving resources from `other`.
   * After the call, `other` is valid, empty and has capacity 0.
   *
   * @param other Buffer whose resources are moved.
   * @note This constructor is `noexcept` so move is well-behaved.
   */
  circular_buffer (circular_buffer &&other) LUMEX_NOEXCEPT
      : m_allocator (std::move (other.m_allocator)),
        m_slots_alloc (std::move (other.m_slots_alloc)),
        m_slots (other.m_slots),
        m_capacity (other.m_capacity),
        m_size (other.m_size),
        m_head (other.m_head)
  {
    other.m_slots = nullptr;
    other.m_capacity = 0;
    other.m_size = 0;
    other.m_head = 0;
  }

  /**
   * @brief Move assignment operator.
   *
   * Assigns `other` to this buffer by moving resources.
   * Every existing element in this buffer is destroyed and memory is released.
   * After the call, `other` is valid, empty and has capacity 0 (it can be
   * assigned to or given room by `set_capacity`).
   *
   * @param other Buffer whose resources are moved.
   * @return Reference to this buffer.
   * @note This operator is `noexcept` so move is well-behaved.
   */
  circular_buffer &
  operator= (circular_buffer &&other) LUMEX_NOEXCEPT
  {
    if (this == &other)
      return *this;
    clear ();
    deallocate_storage ();
    m_allocator = std::move (other.m_allocator);
    m_slots_alloc = std::move (other.m_slots_alloc);
    m_slots = other.m_slots;
    m_capacity = other.m_capacity;
    m_size = other.m_size;
    m_head = other.m_head;
    other.m_slots = nullptr;
    other.m_capacity = 0;
    other.m_size = 0;
    other.m_head = 0;
    return *this;
  }

  /**
   * @brief Destructor.
   *
   * Destroys every element and releases allocated memory.
   */
  ~circular_buffer ()
  {
    clear ();
    deallocate_storage ();
  }

  // ============= assignment of content =============

  /**
   * @brief Replaces the content with `n` copies of `item`; the capacity
   * becomes `n`.
   * @param n Number of elements, also the new capacity.
   * @param item Value to copy (may be an element of this buffer).
   * @throws Whatever the copy constructor of `T` or the allocator throws; the
   * buffer is unchanged then (strong).
   */
  void
  assign (size_type n, value_type const &item)
  {
    circular_buffer tmp (alloc_tag (), m_allocator, n);
    tmp.fill_back (n, item);
    swap_data (tmp);
  }

  /**
   * @brief Replaces the content with `n` copies of `item` in a buffer of the
   * given capacity.
   * @param capacity New capacity.
   * @param n Number of elements, at most `capacity` (more are cut to it).
   * @param item Value to copy (may be an element of this buffer).
   * @throws As `assign (size_type, value_type const &)`.
   */
  void
  assign (size_type capacity, size_type n, value_type const &item)
  {
    circular_buffer tmp (alloc_tag (), m_allocator, capacity);
    tmp.fill_back (n < capacity ? n : capacity, item);
    swap_data (tmp);
  }

  /**
   * @brief Replaces the content with a copy of a range; the capacity becomes
   * the length of the range.
   * @tparam InputIt Input iterator type.
   * @param first Start of the range (not an iterator of this buffer unless the
   * range is the whole buffer).
   * @param last End of the range.
   * @throws As the range constructor; the buffer is unchanged then (strong).
   */
  template <class InputIt, class = enable_if_input_iterator<InputIt>>
  void
  assign (InputIt first, InputIt last)
  {
    circular_buffer tmp (first, last, m_allocator);
    swap_data (tmp);
  }

  /**
   * @brief Replaces the content with a copy of a range in a buffer of the
   * given capacity (the last `capacity` elements of a longer range).
   * @tparam InputIt Input iterator type.
   * @param capacity New capacity.
   * @param first Start of the range.
   * @param last End of the range.
   * @throws As the range constructor; the buffer is unchanged then (strong).
   */
  template <class InputIt, class = enable_if_input_iterator<InputIt>>
  void
  assign (size_type capacity, InputIt first, InputIt last)
  {
    circular_buffer tmp (capacity, first, last, m_allocator);
    swap_data (tmp);
  }

  /**
   * @brief Replaces the content with the elements of a list; the capacity
   * becomes the size of the list.
   * @param init Elements, oldest first.
   */
  void
  assign (std::initializer_list<value_type> init)
  {
    assign (init.begin (), init.end ());
  }

  /**
   * @brief Replaces the content with the elements of a list in a buffer of the
   * given capacity (the last `capacity` elements of a longer list).
   * @param capacity New capacity.
   * @param init Elements, oldest first.
   */
  void
  assign (size_type capacity, std::initializer_list<value_type> init)
  {
    assign (capacity, init.begin (), init.end ());
  }

  // ============= capacity / state =============

  /**
   * @brief Returns the maximum number of elements the buffer can hold.
   * @return Buffer capacity.
   * @note This function is `noexcept`.
   */
  size_type
  capacity () const LUMEX_NOEXCEPT
  {
    return m_capacity;
  }

  /**
   * @brief Returns the current number of elements in the buffer.
   * @return Number of elements in the buffer.
   * @note This function is `noexcept`.
   */
  size_type
  size () const LUMEX_NOEXCEPT
  {
    return m_size;
  }

  /**
   * @brief Returns the largest capacity the allocator and the difference type
   * allow.
   * @return Maximum capacity.
   * @note This function is `noexcept`.
   */
  size_type
  max_size () const LUMEX_NOEXCEPT
  {
    return (std::min)(static_cast<size_type> (
                          alloc_traits::max_size (m_allocator)),
                      static_cast<size_type> (
                          (std::numeric_limits<difference_type>::max) ()));
  }

  /**
   * @brief Checks whether the buffer is empty.
   * @return `true` if the buffer holds no elements, otherwise `false`.
   * @note This function is `noexcept`.
   */
  bool
  empty () const LUMEX_NOEXCEPT
  {
    return m_size == 0;
  }

  /**
   * @brief Checks whether the buffer is full.
   * @return `true` if the buffer holds capacity elements, otherwise `false`.
   * A buffer with capacity 0 is both empty and full.
   * @note This function is `noexcept`.
   */
  bool
  full () const LUMEX_NOEXCEPT
  {
    return m_size == m_capacity;
  }

  /**
   * @brief Checks whether the content is one contiguous run in the storage.
   * @return `true` if `&(*this)[0] < &(*this)[1] < ... < &(*this)[size () -
   * 1]` holds as consecutive slots (always for an empty buffer).
   * @note This function is `noexcept`; it does not invalidate anything.
   */
  bool
  is_linearized () const LUMEX_NOEXCEPT
  {
    return m_head + m_size <= m_capacity;
  }

  /**
   * @brief Makes the content contiguous in the storage.
   *
   * If the buffer is already linearized nothing moves. Otherwise the elements
   * are moved (copied when `T` has no `noexcept` move) into a new block of the
   * same capacity, so the first element ends up at the start of the storage.
   *
   * @return Pointer to the oldest element, or `nullptr` for an empty buffer.
   * The elements `p[0] .. p[size () - 1]` are the logical sequence.
   * @throws Whatever the allocator or the move/copy of `T` throws; the buffer
   * is unchanged then (strong).
   * @note Invalidates every iterator, pointer and reference when it moves the
   * elements.
   */
  pointer
  linearize ()
  {
    if (m_size == 0)
      return nullptr;
    if (!is_linearized ())
      reallocate (m_capacity, 0, m_size);
    return elem_ptr (m_head);
  }

  /**
   * @brief Changes the capacity; a shrink removes the newest elements.
   *
   * If the new capacity is smaller than the size, the `size () -
   * new_capacity` last (newest) elements are removed. Nothing happens when the
   * capacity does not change.
   *
   * @param new_capacity New capacity; may be 0.
   * @throws std::length_error if `new_capacity > max_size()`; whatever the
   * allocator or the move/copy of `T` throws. The buffer is unchanged then
   * (strong).
   * @note Invalidates every iterator, pointer and reference if the capacity
   * changes. Elements are moved if their move is `noexcept` (or `T` cannot be
   * copied), otherwise copied.
   */
  void
  set_capacity (size_type new_capacity)
  {
    if (new_capacity == m_capacity)
      return;
    reallocate (new_capacity, 0, (std::min)(m_size, new_capacity));
  }

  /**
   * @brief Changes the capacity; a shrink removes the oldest elements.
   *
   * If the new capacity is smaller than the size, the `size () -
   * new_capacity` first (oldest) elements are removed.
   *
   * @param new_capacity New capacity; may be 0.
   * @throws As `set_capacity`.
   * @note Invalidates every iterator, pointer and reference if the capacity
   * changes.
   */
  void
  rset_capacity (size_type new_capacity)
  {
    if (new_capacity == m_capacity)
      return;
    size_type const keep = (std::min)(m_size, new_capacity);
    reallocate (new_capacity, m_size - keep, keep);
  }

  /**
   * @brief Changes the size; a growth appends copies of `item`, a shrink
   * removes the newest elements.
   *
   * If `new_size` exceeds the capacity, the capacity is set to `new_size`
   * first.
   *
   * @param new_size New size.
   * @param item Value to append (may be an element of this buffer).
   * @throws Whatever the copy of `T` or the allocator throws. The elements
   * appended before the exception are removed again.
   */
  void
  resize (size_type new_size, value_type const &item)
  {
    if (new_size <= m_size)
      {
        erase_end_n (m_size - new_size);
        return;
      }
    if (aliases (item))
      {
        value_type copy (item);
        resize (new_size, copy);
        return;
      }
    if (new_size > m_capacity)
      set_capacity (new_size);
    fill_back (new_size - m_size, item);
  }

  /**
   * @brief Changes the size; a growth appends value-initialized elements.
   * @param new_size New size.
   * @throws As `resize (size_type, value_type const &)`.
   * @note Requires a default-constructible `T`.
   */
  void
  resize (size_type new_size)
  {
    resize (new_size, value_type ());
  }

  /**
   * @brief Removes every element from the buffer.
   *
   * Destroys every element without changing capacity.
   * After `clear()`, the buffer is empty (`size()` is `0`).
   *
   * @note Invalidates every iterator, reference, and pointer.
   * @note Complexity: O(size()) destructor calls of `T` (or O(1) if `T` is
   * trivially destructible).
   *
   * @note This function is `noexcept`.
   */
  void
  clear () LUMEX_NOEXCEPT
  {
    destroy_elements ();
    m_size = 0;
    m_head = 0;
  }

  // ============= element access =============

  /**
   * @brief Bounds-checked access by logical index.
   *
   * Returns a reference to the element at the logical index.
   * Logical index `0` is always the oldest element.
   *
   * @param idx Logical element index (`0 <= idx < size()`).
   * @return Reference to the element at that index.
   * @throws std::out_of_range if `idx` is outside the valid range (`idx >=
   * size()`).
   */
  reference
  at (size_type idx)
  {
    if (idx >= m_size)
      throw std::out_of_range ("circular_buffer::at: index out of range");
    return (*this)[idx];
  }

  /**
   * @brief Const bounds-checked access by logical index.
   *
   * Returns a const reference to the element at the logical index.
   * Logical index `0` is always the oldest element.
   *
   * @param idx Logical element index (`0 <= idx < size()`).
   * @return Const reference to the element at that index.
   * @throws std::out_of_range if `idx` is outside the valid range (`idx >=
   * size()`).
   */
  const_reference
  at (size_type idx) const
  {
    if (idx >= m_size)
      throw std::out_of_range ("circular_buffer::at: index out of range");
    return (*this)[idx];
  }

  /**
   * @brief Unchecked access by logical index.
   *
   * Returns a reference to the element at the logical index.
   * Logical index `0` is always the oldest element.
   *
   * @warning Using this operator with an invalid index (`idx >= size()`)
   *          is undefined behavior (UB).
   * @param idx Logical element index (`0 <= idx < size()`).
   * @return Reference to the element at that index.
   */
  reference
  operator[] (size_type idx)
  {
    return *elem_ptr (physical_index (idx));
  }

  /**
   * @brief Const unchecked access by logical index.
   *
   * Returns a const reference to the element at the logical index.
   * Logical index `0` is always the oldest element.
   *
   * @warning Using this operator with an invalid index (`idx >= size()`)
   *          is undefined behavior (UB).
   * @param idx Logical element index (`0 <= idx < size()`).
   * @return Const reference to the element at that index.
   */
  const_reference
  operator[] (size_type idx) const
  {
    return *elem_ptr (physical_index (idx));
  }

  /**
   * @brief Access the oldest element (front).
   *
   * Returns a reference to the first (oldest) element.
   *
   * @see front_safe() - bounds-checked version that throws std::out_of_range.
   * @pre The buffer must not be empty (`!empty()`) - UB.
   * @return Reference to the first element.
   */
  reference
  front ()
  {
#ifndef NDEBUG
    LUMEX_ASSERT (!empty () && "circular_buffer::front() on empty buffer");
#endif
    return *elem_ptr (m_head);
  }

  /**
   * @brief Bounds-checked access to the oldest element (front).
   *
   * Returns a reference to the first (oldest) element.
   *
   * @see front() - unchecked version; UB if empty.
   * @return Reference to the first element.
   * @throws std::out_of_range if the buffer is empty.
   */
  reference
  front_safe ()
  {
    if (empty ())
      throw std::out_of_range (
          "circular_buffer::front_safe() on empty buffer");
    return (*this)[0];
  }

  /**
   * @brief Const access to the oldest element (front).
   *
   * Returns a const reference to the first (oldest) element.
   *
   * @see front_safe() - bounds-checked version that throws std::out_of_range.
   * @pre The buffer must not be empty (`!empty()`) - UB.
   * @return Const reference to the first element.
   */
  const_reference
  front () const
  {
#ifndef NDEBUG
    LUMEX_ASSERT (!empty () && "circular_buffer::front() on empty buffer");
#endif
    return *elem_ptr (m_head);
  }

  /**
   * @brief Const bounds-checked access to the oldest element (front).
   *
   * Returns a const reference to the first (oldest) element.
   *
   * @see front() - unchecked version; UB if empty.
   * @return Const reference to the first element.
   * @throws std::out_of_range if the buffer is empty.
   */
  const_reference
  front_safe () const
  {
    if (empty ())
      throw std::out_of_range (
          "circular_buffer::front_safe() on empty buffer");
    return (*this)[0];
  }

  /**
   * @brief Access the newest element (back).
   *
   * Returns a reference to the last (newest) element.
   *
   * @pre The buffer must not be empty (`!empty()`) - UB.
   * @return Reference to the last element.
   */
  reference
  back ()
  {
#ifndef NDEBUG
    LUMEX_ASSERT (!empty () && "circular_buffer::back() on empty buffer");
#endif
    return (*this)[m_size - 1];
  }

  /**
   * @brief Bounds-checked access to the newest element (back).
   *
   * Returns a reference to the last (newest) element.
   *
   * @see back() - unchecked version; UB if empty.
   * @return Reference to the last element.
   * @throws std::out_of_range if the buffer is empty.
   */
  reference
  back_safe ()
  {
    if (empty ())
      throw std::out_of_range ("circular_buffer::back_safe() on empty buffer");
    return (*this)[m_size - 1];
  }

  /**
   * @brief Const access to the newest element (back).
   *
   * Returns a const reference to the last (newest) element.
   *
   * @pre The buffer must not be empty (`!empty()`) - UB.
   * @see back_safe() - version that throws std::out_of_range.
   * @return Const reference to the last element.
   */
  const_reference
  back () const
  {
#ifndef NDEBUG
    LUMEX_ASSERT (!empty () && "circular_buffer::back() on empty buffer");
#endif
    return (*this)[m_size - 1];
  }

  /**
   * @brief Const bounds-checked access to the newest element (back).
   *
   * Returns a const reference to the last (newest) element.
   *
   * @see back() - unchecked version; UB if empty.
   * @return Const reference to the last element.
   * @throws std::out_of_range if the buffer is empty.
   */
  const_reference
  back_safe () const
  {
    if (empty ())
      throw std::out_of_range ("circular_buffer::back_safe() on empty buffer");
    return (*this)[m_size - 1];
  }

  // ============= modifiers =============

  /**
   * @brief Appends a copy of an element.
   *
   * If the buffer is full, the oldest element is overwritten. If the capacity
   * is 0, nothing happens. `value` may be an element of this buffer.
   *
   * @param value Value to append.
   * @throws Exceptions thrown by the `T` copy constructor or the allocator.
   */
  void
  push_back (T const &value)
  {
    emplace_back_ptr (value);
  }

  /**
   * @brief Appends a moved element.
   *
   * If the buffer is full, the oldest element is overwritten. If the capacity
   * is 0, nothing happens.
   *
   * @param value Value to append (moved).
   * @throws Exceptions thrown by the `T` move constructor or the allocator.
   */
  void
  push_back (T &&value)
  {
    emplace_back_ptr (std::move (value));
  }

  /**
   * @brief Prepends a copy of an element.
   *
   * If the buffer is full, the newest element is overwritten (lost).
   * Unlike `push_back`, which overwrites the oldest element. If the capacity
   * is 0, nothing happens. `value` may be an element of this buffer.
   *
   * @param value Value to prepend.
   * @throws Exceptions thrown by the `T` copy constructor or the allocator.
   * @note The element becomes the new front.
   */
  void
  push_front (T const &value)
  {
    emplace_front_ptr (value);
  }

  /**
   * @brief Prepends a moved element.
   *
   * If the buffer is full, the newest element is overwritten (lost).
   * Unlike `push_back`, which overwrites the oldest element. If the capacity
   * is 0, nothing happens.
   *
   * @param value Value to prepend (moved).
   * @throws Exceptions thrown by the `T` move constructor or the allocator.
   * @note The element becomes the new front.
   */
  void
  push_front (T &&value)
  {
    emplace_front_ptr (std::move (value));
  }

  /**
   * @brief Constructs an element in place at the back.
   *
   * Constructs a new element in the buffer from the forwarded arguments.
   * If the buffer is full, the oldest element is replaced. When `T` is
   * move-constructible the new element is built in a temporary first (so the
   * arguments may refer to the oldest element and a throwing constructor
   * leaves the buffer unchanged) and then moved into the slot of the oldest
   * element.
   *
   * @tparam Args Argument types forwarded to the `T` constructor.
   * @param args Arguments forwarded to the `T` constructor.
   * @return Reference to the newly inserted element.
   * @throws std::length_error if the capacity is 0 (there is nowhere to put
   * the element); exceptions thrown by the `T` constructor or the allocator.
   * @note Strong exception safety on a non-full buffer; on a full buffer see
   * the class documentation.
   *
   * @note
   *  - Non-full buffer: iterators stay valid except `end()`.
   *  - Full buffer: every iterator is invalidated; references/pointers to the
   * oldest element are invalid.
   * @note Complexity: O(1).
   */
  template <class... Args>
  reference
  emplace_back (Args &&...args)
  {
    T *const elem = emplace_back_ptr (std::forward<Args> (args)...);
    if (elem == nullptr)
      throw std::length_error (
          "circular_buffer::emplace_back: the capacity is 0");
    return *elem;
  }

  /**
   * @brief Constructs an element in place at the front.
   *
   * Constructs a new element in the buffer from the forwarded arguments.
   * If the buffer is full, the newest element is replaced (see
   * `emplace_back` for the order of construction).
   *
   * @tparam Args Argument types forwarded to the `T` constructor.
   * @param args Arguments forwarded to the `T` constructor.
   * @return Reference to the newly inserted element.
   * @throws std::length_error if the capacity is 0; exceptions thrown by the
   * `T` constructor or the allocator.
   */
  template <class... Args>
  reference
  emplace_front (Args &&...args)
  {
    T *const elem = emplace_front_ptr (std::forward<Args> (args)...);
    if (elem == nullptr)
      throw std::length_error (
          "circular_buffer::emplace_front: the capacity is 0");
    return *elem;
  }

  /**
   * @brief Removes the oldest element.
   *
   * If the buffer is not empty, destroys the first (oldest) element and
   * shrinks size. If the buffer is empty, this is a no-op.
   *
   * @note Every iterator is invalidated; references/pointers to the
   * removed element are invalid.
   * @note Complexity: O(1).
   *
   * @note This function is `noexcept`.
   */
  void
  pop_front () LUMEX_NOEXCEPT
  {
    if (empty ())
      return;
    alloc_traits::destroy (m_allocator, elem_ptr (m_head));
    m_head = next_index (m_head);
    --m_size;
  }

  /**
   * @brief Removes the newest element.
   *
   * If the buffer is not empty, destroys the last (newest) element and shrinks
   * size. If the buffer is empty, this is a no-op.
   *
   * @note Iterators to the last element and `end()` are invalidated;
   * other iterators and references/pointers (except the removed element) stay
   * valid.
   * @note Complexity: O(1).
   *
   * @note This function is `noexcept`.
   */
  void
  pop_back () LUMEX_NOEXCEPT
  {
    if (empty ())
      return;
    alloc_traits::destroy (m_allocator,
                           elem_ptr (physical_index (m_size - 1)));
    --m_size;
  }

  /**
   * @brief Inserts an element before `pos`.
   *
   * The result is the last `capacity ()` elements of the sequence "elements
   * before `pos`, `item`, elements from `pos`". So on a full buffer the oldest
   * element is removed, and when `pos == begin ()` on a full buffer `item` is
   * not inserted at all (as in Boost).
   *
   * @param pos Iterator to this buffer, `begin ()` to `end ()`.
   * @param item Value to insert (may be an element of this buffer).
   * @return Iterator to the inserted element, or `begin ()` if nothing was
   * inserted.
   * @throws Exceptions thrown by `T` (copy or move construction and
   * assignment) or the allocator. Basic exception safety.
   * @note Requires `T` to be move-assignable (or copy-assignable). Invalidates
   * every iterator. Complexity: linear in `size ()`.
   */
  iterator
  insert (const_iterator pos, value_type const &item)
  {
    return insert_one (pos, item);
  }

  /// @copybrief insert(const_iterator, value_type const&)
  /// @param pos Iterator to this buffer, `begin ()` to `end ()`.
  /// @param item Value to insert (moved).
  /// @return Iterator to the inserted element, or `begin ()` if nothing was
  /// inserted.
  iterator
  insert (const_iterator pos, value_type &&item)
  {
    return insert_one (pos, std::move (item));
  }

  /**
   * @brief Inserts `n` copies of `item` before `pos`.
   *
   * The result is the last `capacity ()` elements of the sequence "elements
   * before `pos`, `n` copies, elements from `pos`".
   *
   * @param pos Iterator to this buffer, `begin ()` to `end ()`.
   * @param n Number of copies.
   * @param item Value to copy (may be an element of this buffer).
   * @throws As `insert (const_iterator, value_type const &)`.
   */
  void
  insert (const_iterator pos, size_type n, value_type const &item)
  {
    if (aliases (item))
      {
        value_type copy (item);
        insert (pos, n, copy);
        return;
      }
    fill_gen gen (m_allocator, item);
    insert_generated (index_of (pos), n, gen);
  }

  /**
   * @brief Inserts a range before `pos`.
   *
   * The result is the last `capacity ()` elements of the sequence "elements
   * before `pos`, the range, elements from `pos`".
   *
   * @tparam InputIt Input iterator type (not an iterator of this buffer).
   * @param pos Iterator to this buffer, `begin ()` to `end ()`.
   * @param first Start of the range.
   * @param last End of the range.
   * @throws As `insert (const_iterator, value_type const &)`.
   */
  template <class InputIt, class = enable_if_input_iterator<InputIt>>
  void
  insert (const_iterator pos, InputIt first, InputIt last)
  {
    insert_range (
        index_of (pos), first, last,
        typename std::iterator_traits<InputIt>::iterator_category ());
  }

  /**
   * @brief Inserts the elements of a list before `pos`.
   * @param pos Iterator to this buffer, `begin ()` to `end ()`.
   * @param init Elements, oldest first.
   * @throws As `insert (const_iterator, value_type const &)`.
   */
  void
  insert (const_iterator pos, std::initializer_list<value_type> init)
  {
    insert (pos, init.begin (), init.end ());
  }

  /**
   * @brief Removes the element at `pos`.
   * @param pos Iterator to an element of this buffer.
   * @return Iterator to the element that followed the removed one (`end ()` if
   * it was the last).
   * @note Requires `T` to be move-assignable (or copy-assignable). Invalidates
   * every iterator. Complexity: linear in the number of elements after `pos`.
   */
  iterator
  erase (const_iterator pos)
  {
    size_type const idx = index_of (pos);
#ifndef NDEBUG
    LUMEX_ASSERT (idx < m_size && "circular_buffer::erase() at end()");
#endif
    std::move (begin () + static_cast<difference_type> (idx + 1), end (),
               begin () + static_cast<difference_type> (idx));
    pop_back ();
    return begin () + static_cast<difference_type> (idx);
  }

  /**
   * @brief Removes the elements of the range `[first, last)`.
   * @param first Start of the range.
   * @param last End of the range.
   * @return Iterator to the element that followed the removed ones.
   * @note Requires `T` to be move-assignable (or copy-assignable). Invalidates
   * every iterator. Complexity: linear.
   */
  iterator
  erase (const_iterator first, const_iterator last)
  {
    size_type const from = index_of (first);
    size_type const to = index_of (last);
#ifndef NDEBUG
    LUMEX_ASSERT (from <= to && to <= m_size
                  && "circular_buffer::erase() invalid range");
#endif
    if (from == to)
      return begin () + static_cast<difference_type> (from);
    std::move (begin () + static_cast<difference_type> (to), end (),
               begin () + static_cast<difference_type> (from));
    erase_end_n (to - from);
    return begin () + static_cast<difference_type> (from);
  }

  /**
   * @brief Removes the `n` oldest elements.
   * @param n Number of elements; limited to `size ()`.
   * @note Invalidates every iterator. Complexity: O(n).
   */
  void
  erase_begin (size_type n) LUMEX_NOEXCEPT
  {
    erase_begin_n ((std::min)(n, m_size));
  }

  /**
   * @brief Removes the `n` newest elements.
   * @param n Number of elements; limited to `size ()`.
   * @note Iterators to the removed elements and `end ()` are invalidated.
   */
  void
  erase_end (size_type n) LUMEX_NOEXCEPT
  {
    erase_end_n ((std::min)(n, m_size));
  }

  /**
   * @brief Returns a copy of the allocator used by the buffer.
   * @return Copy of the allocator object.
   * @note This function is `noexcept`.
   */
  allocator_type
  get_allocator () const LUMEX_NOEXCEPT
  {
    return m_allocator;
  }

  /**
   * @brief Exchanges the contents of two buffers.
   *
   * @details Swaps every element between this buffer and `other`.
   *          If
   * <tt>std::allocator_traits\<Allocator\>\::propagate_on_container_swap\::value</tt>
   * is `true`, allocators are swapped too. Otherwise allocators are not
   * swapped, and they are assumed equivalent (undefined behavior otherwise,
   * as for the standard containers; a debug build asserts it).
   *
   * @note Invalidates every iterator (they are bound to this
   * container). References/pointers stay valid, but ownership moves to the
   * other buffer.
   * @note Complexity: O(1).
   * @throws Does not throw when the declared noexcept conditions hold.
   *
   * @param other Buffer to swap with.
   * @note This function is `noexcept` under allocator-dependent conditions.
   */
  void
  swap (circular_buffer &other) LUMEX_NOEXCEPT_IF (is_swap_noexcept)
  {
    if (std::allocator_traits<Allocator>::propagate_on_container_swap::value)
      {
        // Swapping allocators (and rebind) is allowed and correct:
        std::swap (m_allocator, other.m_allocator);
        std::swap (m_slots_alloc, other.m_slots_alloc);
      }
    else
      {
        // Allocators do not propagate. Buffer swap requires equivalent
        // allocators. Release trusts the documented precondition; debug checks
        // it.
#ifndef NDEBUG
        // Allocator requirements (C++11): equality comparison is allowed.
        LUMEX_ASSERT (m_allocator == other.m_allocator);
#endif
        // Allocators stay in place.
      }

    swap_data (other);
  }

  /**
   * @brief Non-member `swap` that exchanges two buffers.
   *
   * Free `swap` overload for `circular_buffer` that
   * exchanges two buffers efficiently.
   *
   * @param other_1 First buffer.
   * @param other_2 Second buffer.
   * @note This function is `noexcept` under allocator-dependent conditions.
   */
  friend void
  swap (circular_buffer &other_1, circular_buffer &other_2)
      LUMEX_NOEXCEPT_IF (is_swap_noexcept)
  {
    other_1.swap (other_2);
  }

  // ============= iteration =============

  /**
   * @brief Returns an iterator to the start of the logical sequence.
   * @return Iterator to the oldest element.
   */
  iterator
  begin ()
  {
    return iterator (this, 0);
  }

  /**
   * @brief Returns an iterator to the end of the logical sequence.
   * @return Iterator to the past-the-end position.
   */
  iterator
  end ()
  {
    return iterator (this, m_size);
  }

  /**
   * @brief Returns a const iterator to the start of the logical sequence.
   * @return Const iterator to the oldest element.
   * @note This overload lets `begin()` be used in `const` contexts.
   */
  const_iterator
  begin () const
  {
    return cbegin ();
  }

  /**
   * @brief Returns a const iterator to the end of the logical sequence.
   * @return Const iterator to the past-the-end position.
   * @note This overload lets `end()` be used in `const` contexts.
   */
  const_iterator
  end () const
  {
    return cend ();
  }

  /**
   * @brief Returns a const iterator to the start of the logical sequence.
   * @return Const iterator to the oldest element.
   */
  const_iterator
  cbegin () const
  {
    return const_iterator (this, 0);
  }

  /**
   * @brief Returns a const iterator to the end of the logical sequence.
   * @return Const iterator to the past-the-end position.
   */
  const_iterator
  cend () const
  {
    return const_iterator (this, m_size);
  }

  /**
   * @brief Returns a reverse iterator to the start of the reverse sequence.
   * @return Reverse iterator to the newest element.
   */
  reverse_iterator
  rbegin ()
  {
    return reverse_iterator (end ());
  }

  /**
   * @brief Returns a reverse iterator to the end of the reverse sequence.
   * @return Reverse iterator to the position before the oldest element.
   */
  reverse_iterator
  rend ()
  {
    return reverse_iterator (begin ());
  }

  /**
   * @brief Returns a const reverse iterator to the start of the reverse
   * sequence.
   * @return Const reverse iterator to the newest element.
   */
  const_reverse_iterator
  rbegin () const
  {
    return const_reverse_iterator (end ());
  }

  /**
   * @brief Returns a const reverse iterator to the end of the reverse
   * sequence.
   * @return Const reverse iterator to the position before the oldest element.
   */
  const_reverse_iterator
  rend () const
  {
    return const_reverse_iterator (begin ());
  }

  /**
   * @brief Returns a const reverse iterator to the start of the reverse
   * sequence.
   * @return Const reverse iterator to the newest element.
   */
  const_reverse_iterator
  crbegin () const
  {
    return const_reverse_iterator (cend ());
  }

  /**
   * @brief Returns a const reverse iterator to the end of the reverse
   * sequence.
   * @return Const reverse iterator to the position before the oldest element.
   */
  const_reverse_iterator
  crend () const
  {
    return const_reverse_iterator (cbegin ());
  }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif
private:
  allocator_type m_allocator;   ///< Allocator for `T` elements.
  slot_allocator m_slots_alloc; ///< Allocator for internal slots (`Slot`) used
                                ///< to allocate raw memory.
  Slot *m_slots{};      ///< Pointer to the slot array (raw memory) that holds
                        ///< elements; `nullptr` for capacity 0.
  size_type m_capacity; ///< Maximum number of elements the buffer can hold.
  size_type m_size{};   ///< Current number of constructed (live) elements.
  size_type m_head{};   ///< Physical index in `m_slots` of the logical start
                        ///< (the oldest element); below `m_capacity`.
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

  // ============= private constructors =============

  /// Allocates the storage for `capacity` elements; the buffer is empty. Every
  /// other constructor delegates to it, so a throwing body runs the
  /// destructor and nothing leaks.
  circular_buffer (alloc_tag, allocator_type const &alloc, size_type capacity)
      : m_allocator (alloc), m_slots_alloc (m_allocator), m_capacity (capacity)
  {
    allocate_storage ();
  }

  /// Forward range, capacity = length of the range.
  template <class It>
  circular_buffer (range_tag, It first, It last, allocator_type const &alloc,
                   std::forward_iterator_tag)
      : circular_buffer (alloc_tag (), alloc,
                         static_cast<size_type> (std::distance (first, last)))
  {
    append_run (first, m_capacity);
  }

  /// Input range read once, capacity = length of the range.
  template <class It>
  circular_buffer (range_tag, It first, It last, allocator_type const &alloc,
                   std::input_iterator_tag)
      : circular_buffer (alloc_tag (), alloc, 0)
  {
    std::vector<value_type, allocator_type> tmp (first, last, alloc);
    set_capacity (tmp.size ());
    for (typename std::vector<value_type, allocator_type>::iterator it
         = tmp.begin ();
         it != tmp.end (); ++it)
      emplace_back_ptr (std::move (*it));
  }

  /// Forward range with a capacity: the last `capacity` elements are kept.
  template <class It>
  circular_buffer (range_tag, size_type capacity, It first, It last,
                   allocator_type const &alloc, std::forward_iterator_tag)
      : circular_buffer (alloc_tag (), alloc, capacity)
  {
    size_type n = static_cast<size_type> (std::distance (first, last));
    if (n > capacity)
      {
        std::advance (
            first,
            static_cast<typename std::iterator_traits<It>::difference_type> (
                n - capacity));
        n = capacity;
      }
    append_run (first, n);
  }

  /// Input range with a capacity: pushing overwrites, so the last `capacity`
  /// elements are kept.
  template <class It>
  circular_buffer (range_tag, size_type capacity, It first, It last,
                   allocator_type const &alloc, std::input_iterator_tag)
      : circular_buffer (alloc_tag (), alloc, capacity)
  {
    for (; first != last; ++first)
      emplace_back_ptr (*first);
  }

  // ============= private helpers =============

  /**
   * @brief Maps a logical index to a physical index in the backing array.
   * @param logical_idx Logical element index (`0` to `m_capacity - 1`).
   * @return Physical index in `m_slots`.
   * @note The head is below the capacity and the index at most the capacity,
   * so one subtraction wraps; no division. This function is `noexcept`.
   */
  size_type
  physical_index (size_type logical_idx) const LUMEX_NOEXCEPT
  {
    size_type const phys = m_head + logical_idx;
    return phys >= m_capacity ? phys - m_capacity : phys;
  }

  /**
   * @brief Next physical index in circular order.
   * @param idx Current physical index.
   * @return Next physical index.
   * @note This function is `noexcept`.
   */
  size_type
  next_index (size_type idx) const LUMEX_NOEXCEPT
  {
    ++idx;
    return idx == m_capacity ? 0 : idx;
  }

  /**
   * @brief Previous physical index in circular order.
   * @param idx Current physical index.
   * @return Previous physical index.
   * @note This function is `noexcept`.
   */
  size_type
  prev_index (size_type idx) const LUMEX_NOEXCEPT
  {
    return idx == 0 ? m_capacity - 1 : idx - 1;
  }

  /// Address of the element slot `phys_idx` of the block `block`.
  static T *
  elem_in (Slot *block, size_type phys_idx) LUMEX_NOEXCEPT
  {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif
    // address of the byte pocket for T, not the Slot object
    return reinterpret_cast<T *> (
        block[phys_idx]
            .data); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  }

  /// Address of the element slot `phys_idx` of the storage.
  T *
  elem_ptr (size_type phys_idx) LUMEX_NOEXCEPT
  {
    return elem_in (m_slots, phys_idx);
  }

  /// Const address of the element slot `phys_idx` of the storage.
  T const *
  elem_ptr (size_type phys_idx) const LUMEX_NOEXCEPT
  {
    return elem_in (m_slots, phys_idx);
  }

  /// Logical index of a const iterator of this buffer.
  size_type
  index_of (const_iterator pos) const
  {
#ifndef NDEBUG
    LUMEX_ASSERT (pos.m_buf == this
                  && "circular_buffer: iterator of another buffer");
#endif
    return pos.m_idx;
  }

  /// Whether `value` is an element (or a part of one) of this buffer.
  bool
  aliases (value_type const &value) const LUMEX_NOEXCEPT
  {
    std::less<void const *> const before;
    void const *const addr
        = static_cast<void const *> (std::addressof (value));
    void const *const low = static_cast<void const *> (m_slots);
    void const *const high = static_cast<void const *> (m_slots + m_capacity);
    return !before (addr, low) && before (addr, high);
  }

  /// Allocates the block of `m_capacity` slots (none for capacity 0).
  void
  allocate_storage ()
  {
    if (m_capacity == 0)
      return;
    if (m_capacity > max_size ())
      throw std::length_error ("circular_buffer: capacity exceeds max_size");
    m_slots = slot_traits::allocate (m_slots_alloc, m_capacity);
  }

  /**
   * @brief Releases allocated slot memory.
   *
   * If `m_slots` is not `nullptr`, memory is released with `m_slots_alloc`.
   * After release, `m_slots` is set to `nullptr`.
   *
   * @note This function is `noexcept`.
   */
  void
  deallocate_storage () LUMEX_NOEXCEPT
  {
    if (m_slots)
      slot_traits::deallocate (m_slots_alloc, m_slots, m_capacity);
    m_slots = nullptr;
  }

  /// Destroys the live elements (no size change).
  void
  destroy_elements () LUMEX_NOEXCEPT
  {
    if (std::is_trivially_destructible<T>::value)
      return;
    size_type idx = m_head;
    for (size_type i = 0; i < m_size; ++i)
      {
        alloc_traits::destroy (m_allocator, elem_ptr (idx));
        idx = next_index (idx);
      }
  }

  /// Exchanges the storage, capacity, size and head (not the allocators).
  void
  swap_data (circular_buffer &other) LUMEX_NOEXCEPT
  {
    std::swap (m_slots, other.m_slots);
    std::swap (m_capacity, other.m_capacity);
    std::swap (m_size, other.m_size);
    std::swap (m_head, other.m_head);
  }

  /// Constructs `n` elements from `src` after the last element, in contiguous
  /// runs (one index computation per run). The room must be there.
  template <class It>
  void
  append_run (It src, size_type n)
  {
    while (n > 0)
      {
        size_type const phys = physical_index (m_size);
        size_type const run = (std::min)(n, m_capacity - phys);
        T *const dest = elem_ptr (phys);
        size_type done = 0;
        try
          {
            for (; done < run; ++done, ++src)
              alloc_traits::construct (m_allocator, dest + done, *src);
          }
        catch (...)
          {
            for (size_type i = 0; i < done; ++i)
              alloc_traits::destroy (m_allocator, dest + i);
            throw;
          }
        m_size += run;
        n -= run;
      }
  }

  /// Appends a copy of the content of `other` in at most two contiguous
  /// pieces. The room must be there.
  void
  append_copy_of (circular_buffer const &other)
  {
    if (other.m_size == 0)
      return;
    size_type const first_piece
        = (std::min)(other.m_size, other.m_capacity - other.m_head);
    append_run (other.elem_ptr (other.m_head), first_piece);
    append_run (other.elem_ptr (0), other.m_size - first_piece);
  }

  /// Copy assignment when the allocator propagates: take the allocator of the
  /// source, release the old storage with the old allocator.
  void
  copy_assign (circular_buffer const &other, std::true_type)
  {
    circular_buffer tmp (alloc_tag (), other.m_allocator, other.m_capacity);
    tmp.append_copy_of (other);
    std::swap (m_allocator, tmp.m_allocator);
    std::swap (m_slots_alloc, tmp.m_slots_alloc);
    swap_data (tmp);
  }

  /// Copy assignment when the allocator does not propagate: keep the own
  /// allocator and make the new storage with it.
  void
  copy_assign (circular_buffer const &other, std::false_type)
  {
    circular_buffer tmp (alloc_tag (), m_allocator, other.m_capacity);
    tmp.append_copy_of (other);
    swap_data (tmp);
  }

  /// Builds the element for an overwrite in a temporary on the stack.
  struct scratch
  {
    template <class... Args>
    explicit scratch (allocator_type &alloc, Args &&...args) : m_alloc (alloc)
    {
      alloc_traits::construct (m_alloc, ptr (), std::forward<Args> (args)...);
    }

    scratch (scratch const &) = delete;
    scratch &operator= (scratch const &) = delete;

    ~scratch () { alloc_traits::destroy (m_alloc, ptr ()); }

    T *
    ptr () LUMEX_NOEXCEPT
    {
      return reinterpret_cast<T *> (m_bytes);
    }

    allocator_type &m_alloc;
    alignas (T) unsigned char m_bytes[sizeof (T)];
  };

  /// Constructs at the back; returns the new element or `nullptr` for
  /// capacity 0.
  template <class... Args>
  T *
  emplace_back_ptr (Args &&...args)
  {
    if (m_capacity == 0)
      return nullptr;
    if (m_size < m_capacity)
      {
        T *const slot = elem_ptr (physical_index (m_size));
        alloc_traits::construct (m_allocator, slot,
                                 std::forward<Args> (args)...);
        ++m_size;
        return slot;
      }
    return overwrite_oldest (std::is_move_constructible<T> (),
                             std::forward<Args> (args)...);
  }

  /// Full buffer, `T` movable: build the new element first, then replace.
  template <class... Args>
  T *
  overwrite_oldest (std::true_type, Args &&...args)
  {
    scratch tmp (m_allocator, std::forward<Args> (args)...);
    return replace_oldest (std::move (*tmp.ptr ()));
  }

  /// Full buffer, `T` not movable: destroy first, then construct in place.
  template <class... Args>
  T *
  overwrite_oldest (std::false_type, Args &&...args)
  {
    return replace_oldest (std::forward<Args> (args)...);
  }

  /// Destroys the oldest element and constructs the new one in its slot.
  template <class... Args>
  T *
  replace_oldest (Args &&...args)
  {
    T *const slot = elem_ptr (m_head);
    alloc_traits::destroy (m_allocator, slot);
    m_head = next_index (m_head);
    --m_size;
    // The slot of the destroyed element is the tail of the shortened buffer.
    alloc_traits::construct (m_allocator, slot, std::forward<Args> (args)...);
    ++m_size;
    return slot;
  }

  /// Constructs at the front; returns the new element or `nullptr` for
  /// capacity 0.
  template <class... Args>
  T *
  emplace_front_ptr (Args &&...args)
  {
    if (m_capacity == 0)
      return nullptr;
    if (m_size < m_capacity)
      {
        size_type const pos = (m_size == 0) ? m_head : prev_index (m_head);
        T *const slot = elem_ptr (pos);
        alloc_traits::construct (m_allocator, slot,
                                 std::forward<Args> (args)...);
        m_head = pos;
        ++m_size;
        return slot;
      }
    return overwrite_newest (std::is_move_constructible<T> (),
                             std::forward<Args> (args)...);
  }

  /// Full buffer, `T` movable: build the new element first, then replace.
  template <class... Args>
  T *
  overwrite_newest (std::true_type, Args &&...args)
  {
    scratch tmp (m_allocator, std::forward<Args> (args)...);
    return replace_newest (std::move (*tmp.ptr ()));
  }

  /// Full buffer, `T` not movable: destroy first, then construct in place.
  template <class... Args>
  T *
  overwrite_newest (std::false_type, Args &&...args)
  {
    return replace_newest (std::forward<Args> (args)...);
  }

  /// Destroys the newest element and constructs the new one in its slot,
  /// which becomes the front.
  template <class... Args>
  T *
  replace_newest (Args &&...args)
  {
    size_type const pos = prev_index (m_head); // slot of the newest element
    T *const slot = elem_ptr (pos);
    alloc_traits::destroy (m_allocator, slot);
    --m_size; // head is not moved: the live elements are unchanged
    alloc_traits::construct (m_allocator, slot, std::forward<Args> (args)...);
    m_head = pos;
    ++m_size;
    return slot;
  }

  /// Removes `n` (at most `size ()`) oldest elements.
  void
  erase_begin_n (size_type n) LUMEX_NOEXCEPT
  {
    for (size_type i = 0; i < n; ++i)
      pop_front ();
  }

  /// Removes `n` (at most `size ()`) newest elements.
  void
  erase_end_n (size_type n) LUMEX_NOEXCEPT
  {
    for (size_type i = 0; i < n; ++i)
      pop_back ();
  }

  /// Source of `n` copies of one value for `append_generated`.
  struct fill_gen
  {
    fill_gen (allocator_type &alloc, value_type const &item)
        : m_alloc (alloc), m_item (item)
    {
    }

    void
    operator() (T *where)
    {
      alloc_traits::construct (m_alloc, where, m_item);
    }

    void
    skip (size_type)
    {
    }

    allocator_type &m_alloc;
    value_type const &m_item;
  };

  /// Source of one value (copy or move) for `append_generated`.
  template <class U> struct value_gen
  {
    value_gen (allocator_type &alloc, U &&item)
        : m_alloc (alloc), m_item (std::forward<U> (item))
    {
    }

    void
    operator() (T *where)
    {
      alloc_traits::construct (m_alloc, where, std::forward<U> (m_item));
    }

    void
    skip (size_type)
    {
    }

    allocator_type &m_alloc;
    U &&m_item;
  };

  /// Source of the elements of a forward range for `append_generated`.
  template <class It> struct range_gen
  {
    range_gen (allocator_type &alloc, It first) : m_alloc (alloc), m_it (first)
    {
    }

    void
    operator() (T *where)
    {
      alloc_traits::construct (m_alloc, where, *m_it);
      ++m_it;
    }

    void
    skip (size_type count)
    {
      std::advance (
          m_it,
          static_cast<typename std::iterator_traits<It>::difference_type> (
              count));
    }

    allocator_type &m_alloc;
    It m_it;
  };

  /// Constructs `n` elements at the back one by one from `gen`; the room must
  /// be there. A throwing construction removes the elements made so far.
  template <class Gen>
  void
  append_generated (size_type n, Gen &gen)
  {
    size_type const before = m_size;
    try
      {
        for (size_type i = 0; i < n; ++i)
          {
            gen (elem_ptr (physical_index (m_size)));
            ++m_size;
          }
      }
    catch (...)
      {
        erase_end_n (m_size - before);
        throw;
      }
  }

  /// Appends `n` copies of `item` (room required).
  void
  fill_back (size_type n, value_type const &item)
  {
    fill_gen gen (m_allocator, item);
    append_generated (n, gen);
  }

  /// The core of every insert: puts up to `n` elements of `gen` before the
  /// logical index `pos` so that the buffer holds the last `capacity ()`
  /// elements of "before pos, the items, from pos". Returns the logical index
  /// of the first inserted element (`pos` when nothing was inserted).
  template <class Gen>
  size_type
  insert_generated (size_type pos, size_type n, Gen &gen)
  {
    size_type const after = m_size - pos;
    if (n == 0 || after >= m_capacity)
      return pos;
    size_type const fit = m_capacity - after;
    size_type const count = n < fit ? n : fit;
    gen.skip (n - count); // of a longer range only the last items fit
    size_type const drop
        = m_size + count > m_capacity ? m_size + count - m_capacity : 0;
    erase_begin_n (drop);
    size_type const target = pos - drop;
    size_type const old_size = m_size;
    append_generated (count, gen);
    if (target != old_size)
      std::rotate (begin () + static_cast<difference_type> (target),
                   begin () + static_cast<difference_type> (old_size), end ());
    return target;
  }

  /// Single insert; a value of this buffer is copied first.
  template <class U>
  iterator
  insert_one (const_iterator pos, U &&item)
  {
    if (aliases (item))
      {
        value_type copy (std::forward<U> (item));
        return insert_one (pos, std::move (copy));
      }
    value_gen<U> gen (m_allocator, std::forward<U> (item));
    size_type const idx = insert_generated (index_of (pos), 1, gen);
    return begin () + static_cast<difference_type> (idx);
  }

  /// Range insert for forward iterators.
  template <class It>
  void
  insert_range (size_type pos, It first, It last, std::forward_iterator_tag)
  {
    range_gen<It> gen (m_allocator, first);
    insert_generated (
        pos, static_cast<size_type> (std::distance (first, last)), gen);
  }

  /// Range insert for single-pass input iterators: one insert per element.
  template <class It>
  void
  insert_range (size_type pos, It first, It last, std::input_iterator_tag)
  {
    if (full () && pos == 0)
      return;
    for (; first != last; ++first)
      {
        value_type item (*first);
        value_gen<value_type> gen (m_allocator, std::move (item));
        pos = insert_generated (pos, 1, gen) + 1;
      }
  }

  /// Makes a new block of `new_capacity` slots from `count` elements starting
  /// at logical index `first`, moved if their move cannot throw (or `T` cannot
  /// be copied), copied otherwise; the old block is released. Strong.
  void
  reallocate (size_type new_capacity, size_type first, size_type count)
  {
    if (new_capacity > max_size ())
      throw std::length_error ("circular_buffer: capacity exceeds max_size");
    Slot *const block
        = new_capacity != 0
              ? slot_traits::allocate (m_slots_alloc, new_capacity)
              : nullptr;
    size_type built = 0;
    try
      {
        for (; built < count; ++built)
          alloc_traits::construct (
              m_allocator, elem_in (block, built),
              std::move_if_noexcept ((*this)[first + built]));
      }
    catch (...)
      {
        for (size_type i = 0; i < built; ++i)
          alloc_traits::destroy (m_allocator, elem_in (block, i));
        if (block != nullptr)
          slot_traits::deallocate (m_slots_alloc, block, new_capacity);
        throw;
      }
    destroy_elements ();
    deallocate_storage ();
    m_slots = block;
    m_capacity = new_capacity;
    m_size = count;
    m_head = 0;
  }
};

// ============= non-member comparison operators =============

/**
 * @brief Equality comparison of two `circular_buffer` objects.
 *
 * Compares two `circular_buffer` objects element-wise. They are equal
 * if they have the same size and every logical element compares equal.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `circular_buffer`.
 * @param rhs Second `circular_buffer`.
 * @return `true` if the buffers are equal, otherwise `false`.
 * @note Uses `std::equal` for element-wise comparison.
 */
template <class T, class Alloc>
inline bool
operator== (circular_buffer<T, Alloc> const &lhs,
            circular_buffer<T, Alloc> const &rhs)
{
  if (lhs.size () != rhs.size ())
    return false;
  return std::equal (lhs.begin (), lhs.end (), rhs.begin ());
}

/**
 * @brief Inequality comparison of two `circular_buffer` objects.
 *
 * Compares two `circular_buffer` objects for inequality. They are unequal
 * if sizes differ or any logical element differs.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `circular_buffer`.
 * @param rhs Second `circular_buffer`.
 * @return `true` if the buffers are unequal, otherwise `false`.
 */
template <class T, class Alloc>
inline bool
operator!= (circular_buffer<T, Alloc> const &lhs,
            circular_buffer<T, Alloc> const &rhs)
{
  return !(lhs == rhs);
}

/**
 * @brief "less" operator for two `circular_buffer` objects.
 *
 * Lexicographical comparison of two `circular_buffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `circular_buffer`.
 * @param rhs Second `circular_buffer`.
 * @return `true` if `lhs` is lexicographically less than `rhs`, otherwise
 * `false`.
 * @note Uses `std::lexicographical_compare`.
 */
template <class T, class Alloc>
inline bool
operator< (circular_buffer<T, Alloc> const &lhs,
           circular_buffer<T, Alloc> const &rhs)
{
  return std::lexicographical_compare (lhs.begin (), lhs.end (), rhs.begin (),
                                       rhs.end ());
}

/**
 * @brief "less or equal" operator for two `circular_buffer` objects.
 *
 * Lexicographical comparison of two `circular_buffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `circular_buffer`.
 * @param rhs Second `circular_buffer`.
 * @return `true` if `lhs` is lexicographically less or equal to `rhs`,
 * otherwise `false`.
 */
template <class T, class Alloc>
inline bool
operator<= (circular_buffer<T, Alloc> const &lhs,
            circular_buffer<T, Alloc> const &rhs)
{
  return !(rhs < lhs);
}

/**
 * @brief "greater" operator for two `circular_buffer` objects.
 *
 * Lexicographical comparison of two `circular_buffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `circular_buffer`.
 * @param rhs Second `circular_buffer`.
 * @return `true` if `lhs` is lexicographically greater than `rhs`, otherwise
 * `false`.
 */
template <class T, class Alloc>
inline bool
operator> (circular_buffer<T, Alloc> const &lhs,
           circular_buffer<T, Alloc> const &rhs)
{
  return rhs < lhs;
}

/**
 * @brief "greater or equal" operator for two `circular_buffer` objects.
 *
 * Lexicographical comparison of two `circular_buffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `circular_buffer`.
 * @param rhs Second `circular_buffer`.
 * @return `true` if `lhs` is lexicographically greater or equal to `rhs`,
 * otherwise `false`.
 */
template <class T, class Alloc>
inline bool
operator>= (circular_buffer<T, Alloc> const &lhs,
            circular_buffer<T, Alloc> const &rhs)
{
  return !(lhs < rhs);
}

#if LUMEX_HAS_THREE_WAY_COMPARISON && LUMEX_HAS_STD_CONCEPTS
/**
 * @brief Three-way comparison (`<=>`) of two `circular_buffer` objects
 * (C++20).
 *
 * Lexicographical comparison with the `<=>` of the elements, then by size.
 *
 * @tparam T Element type; must be `std::three_way_comparable`.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `circular_buffer`.
 * @param rhs Second `circular_buffer`.
 * @return `std::compare_three_way_result_t<T>`: `std::strong_ordering` for
 * `int`, `std::partial_ordering` for `double`, ...
 * @note This operator is available only in C++20 and later.
 */
template <class T, class Alloc>
  requires std::three_way_comparable<T>
inline std::compare_three_way_result_t<T>
operator<=> (circular_buffer<T, Alloc> const &lhs,
             circular_buffer<T, Alloc> const &rhs)
{
  auto it1 = lhs.begin ();
  auto end1 = lhs.end ();
  auto it2 = rhs.begin ();
  auto end2 = rhs.end ();

  for (; it1 != end1 && it2 != end2; ++it1, ++it2)
    if (auto cmp = (*it1 <=> *it2); cmp != 0)
      return cmp;

  // After comparing the common prefix, the shorter buffer is less.
  return lhs.size () <=> rhs.size ();
}
#endif
} // namespace circular_buffer
} // namespace core
} // namespace lumex

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-type-reinterpret-cast)

#endif // !LUMEX_CORE_CIRCULAR_BUFFER_BUFFER_HPP
