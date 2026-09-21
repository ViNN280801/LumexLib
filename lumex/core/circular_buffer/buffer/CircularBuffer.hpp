/**
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

#ifndef LUMEX_CORE_CIRCULAR_BUFFER_BUFFER_HPP
#define LUMEX_CORE_CIRCULAR_BUFFER_BUFFER_HPP

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-type-reinterpret-cast)

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if __cplusplus >= 202002L
#include <compare>  // For std::strong_ordering
#include <concepts> // For std::three_way_comparable
#endif

namespace lumex
{
namespace core
{
namespace circular_buffer
{
namespace buffer
{
/**
 * @brief Fixed-capacity circular buffer that overwrites when full (similar to
 * Boost's `circular_buffer`).
 *
 * @tparam T Element type stored in the buffer.
 * @tparam Allocator Allocator type; default `std::allocator<T>`.
 *
 * This container mimics the main behavior of Boost `circular_buffer`
 * while staying C++11 and header-only. Capacity is fixed at construction.
 *
 * @section characteristics Characteristics
 *  - Insert into a full buffer overwrites the oldest element.
 *  - `T` need not be default-constructible (elements are created on demand).
 *  - Strong exception safety for insert when not overwriting;
 *    basic exception safety on overwrite (the overwritten element is destroyed
 * before the insert).
 *  - Random access at indexes (`0..size()-1`), where `0` is the oldest
 *    element.
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
 * is O(1).
 *
 * @subsection exceptions Exception guarantees
 *  - Construct/insert on a non-full buffer: strong guarantee
 * (commit-or-rollback).
 *  - Overwrite (full buffer): basic guarantee - the replaced element is
 * destroyed first, then the new one is constructed; on exception the buffer
 * stays consistent.
 *  - `pop_*`, `clear`, `swap` do not throw (as declared `noexcept`/macros).
 *
 * @subsection threads Thread safety
 * The container is not thread-safe. Concurrent access from several threads
 * needs external synchronization.
 *
 * @subsection iterators Iterators
 * Iterators are random-access. Because they store an index, any change of
 * `head` makes them inconsistent with the original elements (see
 * invalidation).
 *
 * @subsection contiguity Memory contiguity
 *             The container does not promise contiguous `T` like
 * `std::vector`. There is no `data()`. Addresses of live elements stay stable
 * across operations that do not destroy that element; overwrite/erase
 * invalidates the address.
 *
 * @subsection ordering Comparison
 *  - `==`, `!=`, `<`, `<=`, `>`, `>=` are lexicographical on the logical
 * sequence; they require the matching operations on `T`.
 *  - Operator `<=>` is available in C++20+ and requires
 * `std::totally_ordered<T>`.
 *
 * @subsection allocator Allocator and swap
 * On `swap`, allocators are exchanged only if
 * `std::allocator_traits<Allocator>::propagate_on_container_swap::value ==
 * true`. Otherwise they are assumed equivalent; elements do not move.
 */
template <class T, class Allocator = std::allocator<T>> class CircularBuffer
{
  using alloc_traits = std::allocator_traits<Allocator>;

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

public:
  /**
   * @brief Random-access iterator over the logical element sequence.
   *
   * This iterator walks CircularBuffer elements
   * with random access. It behaves like ordinary pointers or iterators
   * of standard containers such as `std::vector`.
   */
  class iterator
  {
  public:
    using iterator_category
        = std::random_access_iterator_tag; ///< Iterator category.
    using value_type = T; ///< Element type the iterator points to.
    using difference_type = std::ptrdiff_t; ///< Type used for the difference
                                            ///< between two iterators.
    using pointer = T *;                    ///< Pointer type to an element.
    using reference = T &;                  ///< Reference type to an element.

    /// @brief Default constructor. Creates an uninitialized iterator.
    iterator () = default;

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
     * @brief Prefix increment. Advances the iterator to the next element.
     * @return Reference to this iterator after increment.
     */
    iterator &
    operator++ ()
    {
      ++m_idx;
      return *this;
    }

    /**
     * @brief Postfix increment. Advances the iterator to the next element.
     * @return Copy of the iterator before increment.
     */
    iterator
    operator++ (int)
    {
      iterator tmp (*this);
      ++*this;
      return tmp;
    }

    /**
     * @brief Prefix decrement. Moves the iterator to the previous element.
     * @return Reference to this iterator after decrement.
     */
    iterator &
    operator-- ()
    {
      --m_idx;
      return *this;
    }

    /**
     * @brief Postfix decrement. Moves the iterator to the previous element.
     * @return Copy of the iterator before decrement.
     */
    iterator
    operator-- (int)
    {
      iterator tmp (*this);
      --*this;
      return tmp;
    }

    /**
     * @brief Addition assignment. Advances the iterator by `n` positions.
     * @param n Number of positions to move.
     * @return Reference to this iterator after the move.
     */
    iterator &
    operator+= (difference_type n)
    {
      m_idx += n;
      return *this;
    }

    /**
     * @brief Subtraction assignment. Moves the iterator back by `n` positions.
     * @param n Number of positions to move.
     * @return Reference to this iterator after the move.
     */
    iterator &
    operator-= (difference_type n)
    {
      m_idx -= n;
      return *this;
    }

    /**
     * @brief Addition. Creates a new iterator advanced by `n` positions.
     * @param n Number of positions to shift.
     * @return New iterator shifted by `n` positions.
     */
    iterator
    operator+ (difference_type n) const
    {
      iterator tmp (*this);
      return tmp += n;
    }

    /**
     * @brief Friend addition. Creates a new iterator advanced by `n`
     * positions.
     * @param n Number of positions to shift.
     * @param iter Source iterator.
     * @return New iterator shifted by `n` positions.
     */
    friend iterator
    operator+ (difference_type n, iterator iter)
    {
      return iter += n;
    }

    /**
     * @brief Subtraction. Creates a new iterator moved back by `n` positions.
     * @param n Number of positions to shift.
     * @return New iterator shifted by `n` positions.
     */
    iterator
    operator- (difference_type n) const
    {
      iterator tmp (*this);
      return tmp -= n;
    }

    /**
     * @brief Difference. distance between two iterators.
     * @param other The other iterator.
     * @return Position difference between this iterator and the other.
     */
    difference_type
    operator- (iterator const &other) const
    {
      return difference_type (m_idx) - difference_type (other.m_idx);
    }

    /**
     * @brief Equality comparison.
     * @param other The other iterator.
     * @return `true` if both iterators point at the same element in the same
     * buffer, otherwise `false`.
     */
    bool
    operator== (iterator const &other) const
    {
      return m_buf == other.m_buf && m_idx == other.m_idx;
    }

    /**
     * @brief Inequality comparison.
     * @param other The other iterator.
     * @return `true` if the iterators point at different elements or different
     * buffers, otherwise `false`.
     */
    bool
    operator!= (iterator const &other) const
    {
      return !(*this == other);
    }

    /**
     * @brief "less" operator. Compares the positions of two iterators.
     * @param other The other iterator.
     * @return `true` if this iterator precedes `other`, otherwise `false`.
     */
    bool
    operator< (iterator const &other) const
    {
      return m_idx < other.m_idx;
    }

    /**
     * @brief "greater" operator. Compares the positions of two iterators.
     * @param other The other iterator.
     * @return `true` if this iterator follows `other`, otherwise `false`.
     */
    bool
    operator> (iterator const &other) const
    {
      return other < *this;
    }

    /**
     * @brief "less or equal" operator. Compares the positions of two
     * iterators.
     * @param other The other iterator.
     * @return `true` if this iterator precedes or equals `other`, otherwise
     * `false`.
     */
    bool
    operator<= (iterator const &other) const
    {
      return !(other < *this);
    }

    /**
     * @brief "greater or equal" operator. Compares the positions of two
     * iterators.
     * @param other The other iterator.
     * @return `true` if this iterator follows or equals `other`, otherwise
     * `false`.
     */
    bool
    operator>= (iterator const &other) const
    {
      return !(*this < other);
    }

  private:
    friend class CircularBuffer;

    /// @brief Private constructor.
    /// @param buf Pointer to the CircularBuffer that owns the iterator.
    /// @param idx Logical element index.
    iterator (CircularBuffer *buf, size_type idx) : m_buf (buf), m_idx (idx) {}

    CircularBuffer *m_buf{}; ///< Pointer to the buffer owned by this iterator.
    size_type m_idx{};       ///< Logical element index in the buffer.
  };

  /**
   * @brief Const random-access iterator.
   *
   * This iterator walks CircularBuffer elements
   * with random access, but it cannot modify elements.
   * It behaves like `const` pointers or iterators of standard containers.
   */
  class const_iterator
  {
  public:
    using iterator_category
        = std::random_access_iterator_tag; ///< Iterator category.
    using value_type = T; ///< Element type the iterator points to.
    using difference_type = std::ptrdiff_t; ///< Type used for the difference
                                            ///< between two iterators.
    using pointer = T const *;   ///< Const pointer type to an element.
    using reference = T const &; ///< Const reference type to an element.

    /// @brief Default constructor. Creates an uninitialized const iterator.
    const_iterator () : m_buf (0), m_idx (0) {}

    /// @brief Copy constructor from a non-const iterator.
    /// @param iter Source iterator.
    const_iterator (iterator const &iter)
        : m_buf (iter.m_buf), m_idx (iter.m_idx)
    {
    }

    /**
     * @brief Dereference operator.
     * @return Const reference to the element the iterator points to.
     */
    reference
    operator* () const
    {
      return (*m_buf)[m_idx];
    }

    /**
     * @brief Member-access operator.
     * @return Const pointer to the element the iterator points to.
     */
    pointer
    operator->() const
    {
      return std::addressof ((*m_buf)[m_idx]);
    }

    /**
     * @brief Prefix increment. Advances the iterator to the next element.
     * @return Reference to this iterator after increment.
     */
    const_iterator &
    operator++ ()
    {
      ++m_idx;
      return *this;
    }

    /**
     * @brief Postfix increment. Advances the iterator to the next element.
     * @return Copy of the iterator before increment.
     */
    const_iterator
    operator++ (int)
    {
      const_iterator tmp (*this);
      ++*this;
      return tmp;
    }

    /**
     * @brief Prefix decrement. Moves the iterator to the previous element.
     * @return Reference to this iterator after decrement.
     */
    const_iterator &
    operator-- ()
    {
      --m_idx;
      return *this;
    }

    /**
     * @brief Postfix decrement. Moves the iterator to the previous element.
     * @return Copy of the iterator before decrement.
     */
    const_iterator
    operator-- (int)
    {
      const_iterator tmp (*this);
      --*this;
      return tmp;
    }

    /**
     * @brief Addition assignment. Advances the iterator by `n` positions.
     * @param n Number of positions to move.
     * @return Reference to this iterator after the move.
     */
    const_iterator &
    operator+= (difference_type n)
    {
      m_idx += n;
      return *this;
    }

    /**
     * @brief Subtraction assignment. Moves the iterator back by `n` positions.
     * @param n Number of positions to move.
     * @return Reference to this iterator after the move.
     */
    const_iterator &
    operator-= (difference_type n)
    {
      m_idx -= n;
      return *this;
    }

    /**
     * @brief Addition. Creates a new const iterator advanced by `n` positions.
     * @param n Number of positions to shift.
     * @return New const iterator shifted by `n` positions.
     */
    const_iterator
    operator+ (difference_type n) const
    {
      const_iterator tmp (*this);
      return tmp += n;
    }

    /**
     * @brief Friend addition. Creates a new const iterator advanced by `n`
     * positions forward.
     * @param n Number of positions to shift.
     * @param iter Source const iterator.
     * @return New const iterator shifted by `n` positions.
     */
    friend const_iterator
    operator+ (difference_type n, const_iterator iter)
    {
      return iter += n;
    }

    /**
     * @brief Subtraction. Creates a new const iterator moved back by `n`
     * positions.
     * @param n Number of positions to shift.
     * @return New const iterator shifted by `n` positions.
     */
    const_iterator
    operator- (difference_type n) const
    {
      const_iterator tmp (*this);
      return tmp -= n;
    }

    /**
     * @brief Difference. distance between two const iterators.
     * @param other The other const iterator.
     * @return Position difference between this iterator and the other.
     */
    difference_type
    operator- (const_iterator const &other) const
    {
      return difference_type (m_idx) - difference_type (other.m_idx);
    }

    /**
     * @brief Equality comparison.
     * @param other The other const iterator.
     * @return `true` if both iterators point at the same element in the same
     * buffer, otherwise `false`.
     */
    bool
    operator== (const_iterator const &other) const
    {
      return m_buf == other.m_buf && m_idx == other.m_idx;
    }

    /**
     * @brief Inequality comparison.
     * @param other The other const iterator.
     * @return `true` if the iterators point at different elements or different
     * buffers, otherwise `false`.
     */
    bool
    operator!= (const_iterator const &other) const
    {
      return !(*this == other);
    }

    /**
     * @brief "less" operator. Compares the positions of two const iterators.
     * @param other The other const iterator.
     * @return `true` if this iterator precedes `other`, otherwise `false`.
     */
    bool
    operator< (const_iterator const &other) const
    {
      return m_idx < other.m_idx;
    }

    /**
     * @brief "greater" operator. Compares the positions of two const
     * iterators.
     * @param other The other const iterator.
     * @return `true` if this iterator follows `other`, otherwise `false`.
     */
    bool
    operator> (const_iterator const &other) const
    {
      return other < *this;
    }

    /**
     * @brief "less or equal" operator. Compares the positions of two const
     * iterators.
     * @param other The other const iterator.
     * @return `true` if this iterator precedes or equals `other`, otherwise
     * `false`.
     */
    bool
    operator<= (const_iterator const &other) const
    {
      return !(other < *this);
    }

    /**
     * @brief "greater or equal" operator. Compares the positions of two const
     * iterators.
     * @param other The other const iterator.
     * @return `true` if this iterator follows or equals `other`, otherwise
     * `false`.
     */
    bool
    operator>= (const_iterator const &other) const
    {
      return !(*this < other);
    }

  private:
    friend class CircularBuffer;

    /// @brief Private constructor.
    /// @param buf Pointer to the const CircularBuffer that owns the iterator.
    /// @param idx Logical element index.
    const_iterator (CircularBuffer const *buf, size_type idx)
        : m_buf (buf), m_idx (idx)
    {
    }

    CircularBuffer const
        *m_buf;      ///< Pointer to the const buffer owned by this iterator.
    size_type m_idx; ///< Logical element index in the buffer.
  };

  using reverse_iterator
      = std::reverse_iterator<iterator>; ///< Reverse-iterator type.
  using const_reverse_iterator
      = std::reverse_iterator<const_iterator>; ///< Const reverse-iterator
                                               ///< type.

  // ============= ctors / dtors =============

  /**
   * @brief Creates a buffer with the given capacity.
   *
   * @param capacity Positive maximum number of elements the buffer can hold.
   * @param alloc Allocator used to allocate memory.
   * @throws std::length_error if `capacity == 0`, because a zero-capacity
   * buffer is not allowed.
   * @note The buffer starts empty; elements are added later.
   */
  explicit CircularBuffer (size_type capacity,
                           allocator_type const &alloc = allocator_type ())
      : m_allocator (alloc), m_slots_alloc (m_allocator), m_capacity (capacity)
  {
    if (m_capacity == 0)
      throw std::length_error ("CircularBuffer capacity must be > 0");
    allocate_storage ();
  }

  /**
   * @brief Copy constructor.
   *
   * Creates a new circular buffer that is a copy of `other`.
   * Elements are copied in logical order from `other` into the new buffer.
   * If the allocator has `propagate_on_container_copy_construction`, it is
   * copied too.
   *
   * @param other Buffer copied from.
   * @throws Exceptions thrown by `T` copy constructors or the allocator.
   */
  CircularBuffer (CircularBuffer const &other)
      : m_allocator (alloc_traits::select_on_container_copy_construction (
            other.m_allocator)),
        m_slots_alloc (m_allocator), // Initialize from the new m_allocator
        m_capacity (other.m_capacity)
  {
    allocate_storage ();
    // Copy existing elements in logical order
    for (size_type i = 0; i < other.m_size; ++i)
      emplace_back (other[i]);
  }

  /**
   * @brief Copy assignment operator.
   *
   * Assigns the contents of `other` to this buffer.
   * Uses copy-and-swap for the strong exception guarantee.
   *
   * @param other Buffer assigned from.
   * @return Reference to this buffer.
   * @throws Exceptions thrown by the `CircularBuffer` copy constructor or the
   * allocator.
   */
  CircularBuffer &
  operator= (CircularBuffer const &other)
  {
    if (this == &other)
      return *this;
    CircularBuffer tmp (other);
    swap (tmp);
    return *this;
  }

  /**
   * @brief Move constructor.
   *
   * Creates a new circular buffer by moving resources from `other`.
   * After the call, `other` is valid but unspecified.
   *
   * @param other Buffer whose resources are moved.
   * @note This constructor is `noexcept` so move is well-behaved.
   */
  CircularBuffer (CircularBuffer &&other) LUMEX_NOEXCEPT
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
   * After the call, `other` is valid but unspecified.
   *
   * @param other Buffer whose resources are moved.
   * @return Reference to this buffer.
   * @note This operator is `noexcept` so move is well-behaved.
   */
  CircularBuffer &
  operator= (CircularBuffer &&other) LUMEX_NOEXCEPT
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
  ~CircularBuffer ()
  {
    clear ();
    deallocate_storage ();
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
   * @note This function is `noexcept`.
   */
  bool
  full () const LUMEX_NOEXCEPT
  {
    return m_size == m_capacity;
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
    if (!std::is_trivially_destructible<T>::value)
      for (size_type i = 0; i < m_size; ++i)
        alloc_traits::destroy (m_allocator, std::addressof (at_slot (i)));

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
    return at_slot_ref (idx);
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
    return at_slot_cref (idx);
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
    return at_slot_ref (idx);
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
    return at_slot_cref (idx);
  }

  /**
   * @brief Access the oldest element (front).
   *
   * Returns a reference to the first (oldest) element.
   *
   * @see front_safe() - bounds-checked version that throws std::out_of_range.
   * @pre The buffer must not be empty (`!empty()`) - UB.
   * @return Reference to the first element.
   * @throws Undefined if the buffer is empty.
   */
  reference
  front ()
  {
#ifndef NDEBUG
    LUMEX_ASSERT (!empty () && "circular_buffer::front() on empty buffer");
#endif
    return (*this)[0];
  }

  /**
   * @brief Bounds-checked access to the oldest element (front).
   *
   * Returns a reference to the first (oldest) element.
   *
   * @see front() - unchecked version; UB if empty.
   * @return Reference to the first element.
   * @throws Undefined if the buffer is empty.
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
   * @throws Undefined if the buffer is empty.
   */
  const_reference
  front () const
  {
#ifndef NDEBUG
    LUMEX_ASSERT (!empty () && "circular_buffer::front() on empty buffer");
#endif
    return (*this)[0];
  }

  /**
   * @brief Const bounds-checked access to the oldest element (front).
   *
   * Returns a const reference to the first (oldest) element.
   *
   * @see back() - unchecked version; UB if empty.
   * @return Const reference to the first element.
   * @throws Undefined if the buffer is empty.
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
   * @throws Undefined if the buffer is empty.
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
   * @throws Undefined if the buffer is empty.
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
   * @throws Undefined if the buffer is empty.
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
   * If the buffer is full, the oldest element is overwritten.
   *
   * @param value Value to append.
   * @throws Exceptions thrown by the `T` copy constructor or the allocator.
   */
  void
  push_back (T const &value)
  {
    emplace_back (value);
  }

  /**
   * @brief Appends a moved element.
   *
   * If the buffer is full, the oldest element is overwritten.
   *
   * @param value Value to append (moved).
   * @throws Exceptions thrown by the `T` move constructor or the allocator.
   */
  void
  push_back (T &&value)
  {
    emplace_back (std::move (value));
  }

  /**
   * @brief Prepends a copy of an element.
   *
   * If the buffer is full, the newest element is overwritten (lost).
   * Unlike `push_back`, which overwrites the oldest element.
   *
   * @param value Value to append.
   * @throws Exceptions thrown by the `T` copy constructor or the allocator.
   * @note The element becomes the new front.
   */
  void
  push_front (T const &value)
  {
    emplace_front (value);
  }

  /**
   * @brief Prepends a moved element.
   *
   * If the buffer is full, the newest element is overwritten (lost).
   * Unlike `push_back`, which overwrites the oldest element.
   *
   * @param value Value to append (moved).
   * @throws Exceptions thrown by the `T` move constructor or the allocator.
   * @note The element becomes the new front.
   */
  void
  push_front (T &&value)
  {
    emplace_front (std::move (value));
  }

  /**
   * @brief Constructs an element in place at the back.
   *
   * Constructs a new element in the buffer from the forwarded arguments.
   * If the buffer is full, the oldest element is destroyed before insert.
   *
   * @tparam Args Argument types forwarded to the `T` constructor.
   * @param args Arguments forwarded to the `T` constructor.
   * @return Reference to the newly inserted element.
   * @throws Exceptions thrown by the `T` constructor or the allocator.
   * @note Basic exception safety: if the `T` constructor throws
   *       during overwrite, the old element is already destroyed and the
   * buffer stays consistent. If the buffer was not full, the strong exception
   * guarantee holds.
   *
   * @note
   *  - Non-full buffer: iterators stay valid except `end()`.
   *  - Full buffer: every iterator is invalidated; references/pointers to the
   * oldest element are invalid.
   * @note Complexity: O(1).
   * @throws `T` constructor or the allocator.
   */
  template <class... Args>
  reference
  emplace_back (Args &&...args)
  {
    if (full ())
      {
        // same as pop_front(): drop the oldest, make the buffer non-full
        alloc_traits::destroy (m_allocator, std::addressof (at_slot (0)));
        m_head = next_index (m_head);
        --m_size;
      }
    size_type const pos = physical_index (m_size); // buffer is no longer full
    alloc_traits::construct (m_allocator,
                             reinterpret_cast<T *> (slot_ptr (pos)),
                             std::forward<Args> (args)...);
    ++m_size;
    return *reinterpret_cast<T *> (slot_ptr (pos));
  }

  /**
   * @brief Constructs an element in place at the front.
   *
   * Constructs a new element in the buffer from the forwarded arguments.
   * If the buffer is full, the newest element is destroyed before insert.
   *
   * @tparam Args Argument types forwarded to the `T` constructor.
   * @param args Arguments forwarded to the `T` constructor.
   * @return Reference to the newly inserted element.
   * @throws Exceptions thrown by the `T` constructor or the allocator.
   * @note Basic exception safety: if the `T` constructor throws
   *       during overwrite, the old element is already destroyed and the
   * buffer stays consistent. If the buffer was not full, the strong exception
   * guarantee holds.
   */
  template <class... Args>
  reference
  emplace_front (Args &&...args)
  {
    size_type const pos = (m_size == 0) ? m_head : prev_index (m_head);

    if (full ())
      {
        // free the target slot: prev_index(m_head)
        alloc_traits::destroy (
            m_allocator,
            std::addressof (at_slot (0 + m_size - 1))); // former newest
        --m_size; // now capacity-1; head is not moved
      }

    // construct at pos; invariants hold if it throws
    alloc_traits::construct (m_allocator,
                             reinterpret_cast<T *> (slot_ptr (pos)),
                             std::forward<Args> (args)...);
    m_head = (m_size == 0) ? m_head : pos;
    ++m_size;
    return *reinterpret_cast<T *> (slot_ptr (m_head));
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
  pop_front ()
  {
    if (empty ())
      return;
    alloc_traits::destroy (m_allocator, std::addressof (at_slot (0)));
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
  pop_back ()
  {
    if (empty ())
      return;
    alloc_traits::destroy (m_allocator, std::addressof (at_slot (m_size - 1)));
    --m_size;
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
   * `std::allocator_traits<Allocator>::propagate_on_container_swap::value` is
   * `true`, allocators are swapped too. Otherwise allocators are not swapped,
   * and they are assumed equivalent.
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
  swap (CircularBuffer &other) LUMEX_NOEXCEPT_IF (is_swap_noexcept)
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

    // Swap internal data (pointer and metadata)
    std::swap (m_slots, other.m_slots);
    std::swap (m_capacity, other.m_capacity);
    std::swap (m_size, other.m_size);
    std::swap (m_head, other.m_head);
  }

  /**
   * @brief Non-member `swap` that exchanges two buffers.
   *
   * Free `swap` overload for `CircularBuffer` that
   * exchanges two buffers efficiently.
   *
   * @param other_1 First buffer.
   * @param other_2 Second buffer.
   * @note This function is `noexcept` under allocator-dependent conditions.
   */
  friend void
  swap (CircularBuffer &other_1, CircularBuffer &other_2)
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
                        ///< elements.
  size_type m_capacity; ///< Maximum number of elements the buffer can hold.
  size_type m_size{};   ///< Current number of constructed (live) elements.
  size_type m_head{};   ///< Physical index in `m_slots` of the logical start
                        ///< (the oldest element).
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

  /**
   * @brief Maps a logical index to a physical index in the backing array.
   * @param logical_idx Logical element index (`0` to `m_size - 1`).
   * @return Physical index in `m_slots`.
   * @note This function is `noexcept`.
   */
  size_type
  physical_index (size_type logical_idx) const LUMEX_NOEXCEPT
  {
    return (m_head + logical_idx) % m_capacity;
  }

  // Helpers to move head circularly
  /**
   * @brief Next physical index in circular order.
   * @param idx Current physical index.
   * @return Next physical index.
   * @note This function is `noexcept`.
   */
  size_type
  next_index (size_type idx) const LUMEX_NOEXCEPT
  {
    return (idx + 1) % m_capacity;
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
    return (idx + m_capacity - 1) % m_capacity;
  }

  /**
   * @brief Returns a mutable reference at a logical index without bounds
   * checks.
   * @param logical_idx Logical element index.
   * @return Mutable reference to the element.
   */
  reference
  at_slot_ref (size_type logical_idx)
  {
    return at_slot (logical_idx);
  }

  /**
   * @brief Returns a const reference at a logical index without bounds checks.
   * @param logical_idx Logical element index.
   * @return Const reference to the element.
   */
  const_reference
  at_slot_cref (size_type logical_idx) const
  {
    return at_slot_const (logical_idx);
  }

  /**
   * @brief Returns a pointer to raw (byte) storage at a physical index.
   * @param phys_idx Physical slot index.
   * @return `void*` to the slot's raw data.
   * @note This function is `noexcept`.
   */
  void *
  slot_ptr (size_type phys_idx) LUMEX_NOEXCEPT
  {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif
    // address of the byte pocket for T, not the Slot object
    return static_cast<void *> (
        m_slots[phys_idx]
            .data); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  }

  /**
   * @brief Returns a const pointer to raw (byte) storage at a physical index.
   * @param phys_idx Physical slot index.
   * @return `void const*` to the slot's raw data.
   * @note This function is `noexcept`.
   */
  void const *
  slot_ptr (size_type phys_idx) const LUMEX_NOEXCEPT
  {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif
    // address of the byte pocket for T, not the Slot object
    return static_cast<void const *> (
        m_slots[phys_idx]
            .data); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  }

  /**
   * @brief Returns a mutable reference at a logical index.
   * @param logical_idx Logical element index.
   * @return Mutable reference to a `T` element.
   */
  reference
  at_slot (size_type logical_idx)
  {
    return *reinterpret_cast<T *> (slot_ptr (physical_index (logical_idx)));
  }

  /**
   * @brief Returns a const reference at a logical index.
   * @param logical_idx Logical element index.
   * @return Const reference to a `T` element.
   */
  const_reference
  at_slot_const (size_type logical_idx) const
  {
    return *reinterpret_cast<T const *> (
        slot_ptr (physical_index (logical_idx)));
  }

  /**
   * @brief Allocates memory for `m_capacity` slots.
   *
   * Uses `m_slots_alloc` to allocate the required memory.
   *
   * @throws Exceptions thrown by the allocator.
   */
  void
  allocate_storage ()
  {
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
};

// ============= non-member comparison operators =============

/**
 * @brief Equality comparison of two `CircularBuffer` objects.
 *
 * Compares two `CircularBuffer` objects element-wise. They are equal
 * if they have the same size and every logical element compares equal.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `CircularBuffer`.
 * @param rhs Second `CircularBuffer`.
 * @return `true` if the buffers are equal, otherwise `false`.
 * @note Uses `std::equal` for element-wise comparison.
 */
template <class T, class Alloc>
inline bool
operator== (CircularBuffer<T, Alloc> const &lhs,
            CircularBuffer<T, Alloc> const &rhs)
{
  if (lhs.size () != rhs.size ())
    return false;
  return std::equal (lhs.begin (), lhs.end (), rhs.begin ());
}

/**
 * @brief Inequality comparison of two `CircularBuffer` objects.
 *
 * Compares two `CircularBuffer` objects for inequality. They are unequal
 * if sizes differ or any logical element differs.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `CircularBuffer`.
 * @param rhs Second `CircularBuffer`.
 * @return `true` if the buffers are unequal, otherwise `false`.
 */
template <class T, class Alloc>
inline bool
operator!= (CircularBuffer<T, Alloc> const &lhs,
            CircularBuffer<T, Alloc> const &rhs)
{
  return !(lhs == rhs);
}

/**
 * @brief "less" operator for two `CircularBuffer` objects.
 *
 * Lexicographical comparison of two `CircularBuffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `CircularBuffer`.
 * @param rhs Second `CircularBuffer`.
 * @return `true` if `lhs` is lexicographically less than `rhs`, otherwise
 * `false`.
 * @note Uses `std::lexicographical_compare`.
 */
template <class T, class Alloc>
inline bool
operator< (CircularBuffer<T, Alloc> const &lhs,
           CircularBuffer<T, Alloc> const &rhs)
{
  return std::lexicographical_compare (lhs.begin (), lhs.end (), rhs.begin (),
                                       rhs.end ());
}

/**
 * @brief "less or equal" operator for two `CircularBuffer` objects.
 *
 * Lexicographical comparison of two `CircularBuffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `CircularBuffer`.
 * @param rhs Second `CircularBuffer`.
 * @return `true` if `lhs` is lexicographically less or equal to `rhs`,
 * otherwise `false`.
 */
template <class T, class Alloc>
inline bool
operator<= (CircularBuffer<T, Alloc> const &lhs,
            CircularBuffer<T, Alloc> const &rhs)
{
  return !(rhs < lhs);
}

/**
 * @brief "greater" operator for two `CircularBuffer` objects.
 *
 * Lexicographical comparison of two `CircularBuffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `CircularBuffer`.
 * @param rhs Second `CircularBuffer`.
 * @return `true` if `lhs` is lexicographically greater than `rhs`, otherwise
 * `false`.
 */
template <class T, class Alloc>
inline bool
operator> (CircularBuffer<T, Alloc> const &lhs,
           CircularBuffer<T, Alloc> const &rhs)
{
  return rhs < lhs;
}

/**
 * @brief "greater or equal" operator for two `CircularBuffer` objects.
 *
 * Lexicographical comparison of two `CircularBuffer` objects.
 *
 * @tparam T Element type in the buffer.
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `CircularBuffer`.
 * @param rhs Second `CircularBuffer`.
 * @return `true` if `lhs` is lexicographically greater or equal to `rhs`,
 * otherwise `false`.
 */
template <class T, class Alloc>
inline bool
operator>= (CircularBuffer<T, Alloc> const &lhs,
            CircularBuffer<T, Alloc> const &rhs)
{
  return !(lhs < rhs);
}

#if __cplusplus >= 202002L
/**
 * @brief Three-way comparison (`<=>`) of two `CircularBuffer` objects (C++20).
 *
 * Lexicographical comparison using `std::strong_ordering`.
 *
 * @tparam Alloc Buffer allocator type.
 * @param lhs First `CircularBuffer`.
 * @param rhs Second `CircularBuffer`.
 * @return Comparison result `std::strong_ordering::less`,
 * `std::strong_ordering::equal`, or `std::strong_ordering::greater`.
 * @note This operator is available only in C++20 and later.
 */
template <class T, class Alloc>
  requires std::three_way_comparable<T>
inline std::strong_ordering
operator<=> (CircularBuffer<T, Alloc> const &lhs,
             CircularBuffer<T, Alloc> const &rhs)
{
  // Implementation based on pairwise lexicographical element comparison
  auto it1 = lhs.begin ();
  auto end1 = lhs.end ();
  auto it2 = rhs.begin ();
  auto end2 = rhs.end ();

  for (; it1 != end1 && it2 != end2; ++it1, ++it2)
    if (auto cmp = (*it1 <=> *it2); cmp != std::strong_ordering::equal)
      return cmp;

  // After comparing the common prefix, compare sizes
  if (it1 == end1 && it2 == end2)
    return std::strong_ordering::equal;
  if (it1 == end1)
    { // lhs is shorter than rhs
      return std::strong_ordering::less;
    }
  // Otherwise (it2 == end2), rhs is shorter than lhs
  return std::strong_ordering::greater;
}
#endif
} // namespace buffer
} // namespace circular_buffer
} // namespace core
} // namespace lumex

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,
// cppcoreguidelines-pro-type-reinterpret-cast)

#endif // !LUMEX_CORE_CIRCULAR_BUFFER_BUFFER_HPP
