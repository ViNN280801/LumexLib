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
 * @file LumexSpan.hpp
 * @brief C++11 implementation of `std::span` (C++20): a non-owning view of a
 * contiguous sequence of objects.
 * @details `span<T, Extent>` refers to `Extent` consecutive objects of type
 * `T`, or to a run-time number of them when `Extent` is `dynamic_extent`. It
 * has the interface of the C++20 `std::span` plus the later additions: the
 * member types `const_iterator` and `const_reverse_iterator`, `cbegin`,
 * `cend`, `crbegin` and `crend` (C++23) and `at` (C++26). It compiles from
 * C++11. A static extent costs no storage: `sizeof (span<T, N>)` is the size
 * of a pointer.
 *
 * The C++20 concepts the standard constrains the constructors with are
 * replaced before C++20 by detection of the members they imply. A range is
 * accepted when it has `data ()` returning a pointer, `size ()`, is not an
 * array, `std::array` or `span` (those have constructors of their own), has an
 * element type that converts to the element type of the span by a
 * qualification conversion, and is an lvalue or the element type is const
 * (the borrowed-range rule). An iterator is accepted when it is a pointer or
 * `is_contiguous_iterator` is true for it; see that trait.
 *
 * With C++20 and a library that has `<ranges>` the class opts into
 * `std::ranges::enable_borrowed_range` and `std::ranges::enable_view`, and its
 * iterators are pointers, so it is a contiguous range. It converts to and from
 * `std::span` through the range constructors of both: a `lumex` span
 * initializes a `std::span`, and a `std::span` (also an rvalue) initializes a
 * `lumex` span, with the explicit rules of the standard for static extents.
 * There is one class for every standard; it is never an alias of `std::span`,
 * so overload sets and types do not change with the standard.
 *
 * Deliberate differences from `std::span`: the iterator is a pointer; a
 * braced list does not convert to a span (the constructor from a
 * `std::initializer_list` of C++26 is not provided, because it makes a call
 * such as `f ({1, 2, 3})` ambiguous between a `std::vector` overload and a
 * `span` overload, which existing code has); a `std::initializer_list` object
 * does convert, like any range, to a span of const elements; before C++20 an
 * iterator class has to be named by `is_contiguous_iterator`.
 *
 * @note A span does not own what it views. Using one after the viewed storage
 * is destroyed, or passing arguments that break a documented precondition, is
 * undefined behavior; nothing is checked except by `at`.
 */
#ifndef LUMEX_CORE_SPAN_VIEW_SPAN_HPP
#define LUMEX_CORE_SPAN_VIEW_SPAN_HPP

#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

#include "lumex/core/span/view/LumexSpanTraits.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace span
{
namespace view
{
/**
 * @brief A non-owning view of a contiguous sequence of objects.
 * @details See the file description for the rules each constructor follows.
 * `span` is trivially copyable. Every member function is `constexpr` as far as
 * the standard in use allows, and none throws except `at`.
 * @tparam ElementType The type of the viewed objects: an object type, not
 * `void`, complete where an element is accessed (a class may hold a span of
 * itself); may be const.
 * @tparam Extent The number of objects, or `dynamic_extent` when it is known
 * at run time.
 */
template <typename ElementType, std::size_t Extent> class span
{
  static_assert (!std::is_reference<ElementType>::value,
                 "span: the element type must not be a reference");
  static_assert (!std::is_void<ElementType>::value,
                 "span: the element type must not be void");

public:
  // -- Member types and constants --
  /// The type of the viewed objects.
  using element_type = ElementType;
  /// The element type without cv qualifiers.
  using value_type = typename std::remove_cv<ElementType>::type;
  /// The type of sizes and indices.
  using size_type = std::size_t;
  /// The type of differences of iterators.
  using difference_type = std::ptrdiff_t;
  /// Pointer to an element.
  using pointer = element_type *;
  /// Pointer to a const element.
  using const_pointer = element_type const *;
  /// Reference to an element.
  using reference = element_type &;
  /// Reference to a const element.
  using const_reference = element_type const &;
  /// The iterator: a raw pointer, so the view is a contiguous range.
  using iterator = pointer;
  /// The constant iterator (C++23 in the standard): a pointer to const.
  using const_iterator = const_pointer;
  /// The reverse iterator.
  using reverse_iterator = std::reverse_iterator<iterator>;
  /// The constant reverse iterator (C++23 in the standard).
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /// The number of elements, or `dynamic_extent`.
  static LUMEX_CONSTEXPR size_type extent = Extent;

  // -- Construction --
  /**
   * @brief Constructs an empty span.
   * @details Takes part in overload resolution only when `Extent` is
   * `dynamic_extent` or 0. `data ()` is `nullptr`.
   */
  template <
      std::size_t Size = Extent,
      typename std::enable_if<Size == dynamic_extent || Size == 0, int>::type
      = 0>
  LUMEX_CONSTEXPR_CTOR
  span () LUMEX_NOEXCEPT : m_storage (nullptr, 0)
  {
  }

  /**
   * @brief Constructs a span of `count` elements starting at `first`.
   * @details Implicit when `Extent` is `dynamic_extent`, explicit otherwise.
   * Takes part in overload resolution when `Iterator` is a contiguous
   * iterator (see `is_contiguous_iterator`) whose elements convert to
   * `element_type` by a qualification conversion.
   * @pre `[first, first + count)` is a valid range. When `Extent` is not
   * dynamic, `count == Extent`.
   * @param first The first element.
   * @param count The number of elements.
   */
  template <typename Iterator,
            typename std::enable_if<Extent == dynamic_extent
                                        && detail::is_compatible_iterator<
                                            Iterator, element_type>::value,
                                    int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  span (Iterator first, size_type count) LUMEX_NOEXCEPT
      : m_storage (detail::iterator_address (first, count), count)
  {
  }

  /// @copydoc span(Iterator,size_type)
  template <typename Iterator,
            typename std::enable_if<Extent != dynamic_extent
                                        && detail::is_compatible_iterator<
                                            Iterator, element_type>::value,
                                    long>::type
            = 0>
  explicit LUMEX_CONSTEXPR_CTOR
  span (Iterator first, size_type count) LUMEX_NOEXCEPT
      : m_storage (detail::iterator_address (first, count), count)
  {
  }

  /**
   * @brief Constructs a span of the range `[first, last)`.
   * @details Implicit when `Extent` is `dynamic_extent`, explicit otherwise.
   * Takes part in overload resolution when `Iterator` is a contiguous
   * iterator whose elements convert to `element_type`, and `last - first` is
   * valid and `Sentinel` does not convert to `size_type` (that would be the
   * count form).
   * @pre `[first, last)` is a valid range. When `Extent` is not dynamic,
   * `last - first == Extent`.
   * @param first The first element.
   * @param last The end of the range.
   */
  template <
      typename Iterator, typename Sentinel,
      typename std::enable_if<
          Extent == dynamic_extent
              && detail::is_compatible_iterator<Iterator, element_type>::value
              && detail::is_sized_sentinel<Sentinel, Iterator>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR_CTOR
  span (Iterator first, Sentinel last)
      LUMEX_NOEXCEPT_IF (noexcept (std::declval<Sentinel &> ()
                                   - std::declval<Iterator &> ()))
      : m_storage (detail::iterator_address (
                       first, static_cast<size_type> (last - first)),
                   static_cast<size_type> (last - first))
  {
  }

  /// @copydoc span(Iterator,Sentinel)
  template <
      typename Iterator, typename Sentinel,
      typename std::enable_if<
          Extent != dynamic_extent
              && detail::is_compatible_iterator<Iterator, element_type>::value
              && detail::is_sized_sentinel<Sentinel, Iterator>::value,
          long>::type
      = 0>
  explicit LUMEX_CONSTEXPR_CTOR
  span (Iterator first, Sentinel last)
      LUMEX_NOEXCEPT_IF (noexcept (std::declval<Sentinel &> ()
                                   - std::declval<Iterator &> ()))
      : m_storage (detail::iterator_address (
                       first, static_cast<size_type> (last - first)),
                   static_cast<size_type> (last - first))
  {
  }

  /**
   * @brief Constructs a span of a built-in array.
   * @details Implicit. Takes part in overload resolution when `Extent` is
   * `dynamic_extent` or equals `N`.
   * @tparam N The number of elements of the array.
   * @param array The array.
   */
  template <std::size_t N,
            typename std::enable_if<
                detail::is_extent_compatible<Extent, N>::value, int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
      span (typename std::enable_if<true, element_type>::type (&array)[N])
          LUMEX_NOEXCEPT : m_storage (array, N)
  {
  }

  /**
   * @brief Constructs a span of a `std::array`.
   * @details Implicit. Takes part in overload resolution when `Extent` is
   * `dynamic_extent` or equals `N`, and the element type of the array
   * converts to `element_type` by a qualification conversion.
   * @tparam U The element type of the array.
   * @tparam N The number of elements of the array.
   * @param array The array.
   */
  template <typename U, std::size_t N,
            typename std::enable_if<
                detail::is_extent_compatible<Extent, N>::value
                    && detail::is_array_convertible<U, element_type>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  span (std::array<U, N> &array) LUMEX_NOEXCEPT : m_storage (array.data (), N)
  {
  }

  /// @copydoc span(std::array<U,N>&)
  template <
      typename U, std::size_t N,
      typename std::enable_if<
          detail::is_extent_compatible<Extent, N>::value
              && detail::is_array_convertible<U const, element_type>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR_CTOR
  span (std::array<U, N> const &array) LUMEX_NOEXCEPT
      : m_storage (array.data (), N)
  {
  }

  /**
   * @brief Constructs a span of a contiguous range.
   * @details Implicit when `Extent` is `dynamic_extent`, explicit otherwise.
   * The range needs a `data ()` that returns a pointer and a `size ()`; it
   * must not be an array, a `std::array` or a `span`; its element type must
   * convert to `element_type` by a qualification conversion; and it must be an
   * lvalue, a type that is a borrowed range (C++20), or `element_type` must
   * be const. A `std::vector<bool>`, a `std::deque` and a `std::list` have no
   * such `data ()` and are rejected. A range that states its extent in its
   * type (`std::span<T, N>`) is rejected when `N` and `Extent` are both static
   * and differ.
   * @pre When `Extent` is not dynamic, `range.size () == Extent`. The range
   * keeps its storage valid while the span is used.
   * @tparam Range The type of the range (a forwarding reference).
   * @param range The range.
   */
  template <typename Range, typename std::enable_if<
                                Extent == dynamic_extent
                                    && detail::is_compatible_range<
                                        Range, element_type, Extent>::value,
                                int>::type
                            = 0>
  LUMEX_CONSTEXPR_CTOR
  span (Range &&range) LUMEX_NOEXCEPT_IF (
      noexcept (detail::range_data (std::declval<Range &> ())) && noexcept (
          std::declval<Range &> ().size ()))
      : m_storage (detail::range_data (range),
                   static_cast<size_type> (range.size ()))
  {
  }

  /// @copydoc span(Range&&)
  template <typename Range, typename std::enable_if<
                                Extent != dynamic_extent
                                    && detail::is_compatible_range<
                                        Range, element_type, Extent>::value,
                                long>::type
                            = 0>
  explicit LUMEX_CONSTEXPR_CTOR
  span (Range &&range) LUMEX_NOEXCEPT_IF (
      noexcept (detail::range_data (std::declval<Range &> ())) && noexcept (
          std::declval<Range &> ().size ()))
      : m_storage (detail::range_data (range),
                   static_cast<size_type> (range.size ()))
  {
  }

  /**
   * @brief Converts a span of another element type or extent.
   * @details Takes part in overload resolution when the extents are
   * compatible (one of them dynamic, or equal) and `U` converts to
   * `element_type` by a qualification conversion. Implicit unless a static
   * extent is made from a dynamic one, which is explicit.
   * @pre When `Extent` is not dynamic, `other.size () == Extent`.
   * @tparam U The element type of `other`.
   * @tparam N The extent of `other`.
   * @param other The span to convert.
   */
  template <typename U, std::size_t N,
            typename std::enable_if<
                detail::is_extent_compatible<Extent, N>::value
                    && detail::is_array_convertible<U, element_type>::value
                    && (Extent == dynamic_extent || N != dynamic_extent),
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  span (span<U, N> const &other) LUMEX_NOEXCEPT
      : m_storage (other.data (), other.size ())
  {
  }

  /// @copydoc span(span<U,N> const&)
  template <typename U, std::size_t N,
            typename std::enable_if<
                detail::is_extent_compatible<Extent, N>::value
                    && detail::is_array_convertible<U, element_type>::value
                    && (Extent != dynamic_extent && N == dynamic_extent),
                long>::type
            = 0>
  explicit LUMEX_CONSTEXPR_CTOR
  span (span<U, N> const &other) LUMEX_NOEXCEPT
      : m_storage (other.data (), other.size ())
  {
  }

  // -- Subviews --
  /**
   * @brief The first `Count` elements as a span of static extent.
   * @pre `Count <= size ()`. For a static `Extent` it is checked at compile
   * time.
   * @tparam Count The number of elements.
   * @return A `span<element_type, Count>`.
   */
  template <std::size_t Count>
  LUMEX_ATTRIBUTE_NODISCARD ("the subview is the result")
  LUMEX_CONSTEXPR span<element_type, Count> first () const LUMEX_NOEXCEPT
  {
    static_assert (Count <= Extent, "span::first<Count>: Count out of range");
    return span<element_type, Count> (m_storage.data (), Count);
  }

  /**
   * @brief The last `Count` elements as a span of static extent.
   * @pre `Count <= size ()`. For a static `Extent` it is checked at compile
   * time.
   * @tparam Count The number of elements.
   * @return A `span<element_type, Count>`.
   */
  template <std::size_t Count>
  LUMEX_ATTRIBUTE_NODISCARD ("the subview is the result")
  LUMEX_CONSTEXPR span<element_type, Count> last () const LUMEX_NOEXCEPT
  {
    static_assert (Count <= Extent, "span::last<Count>: Count out of range");
    return span<element_type, Count> (
        m_storage.data () + (m_storage.size () - Count), Count);
  }

  /**
   * @brief The elements from `Offset` on, `Count` of them or all the rest, as
   * a span of static extent where the sizes are known.
   * @details The extent of the result is `Count` when `Count` is not
   * `dynamic_extent`, otherwise `Extent - Offset` for a static `Extent` and
   * `dynamic_extent` for a dynamic one.
   * @pre `Offset <= size ()` and `Count == dynamic_extent` or `Count <=
   * size () - Offset`. For a static `Extent` it is checked at compile time.
   * @tparam Offset The index of the first element.
   * @tparam Count The number of elements, or `dynamic_extent` for the rest.
   * @return The subview.
   */
  template <std::size_t Offset, std::size_t Count = dynamic_extent>
  LUMEX_ATTRIBUTE_NODISCARD ("the subview is the result")
  LUMEX_CONSTEXPR
      span<element_type,
           detail::subspan_extent<Extent, Offset, Count>::value> subspan ()
          const LUMEX_NOEXCEPT
  {
    static_assert (Offset <= Extent,
                   "span::subspan<Offset, Count>: Offset out of range");
    static_assert (
        Count == dynamic_extent
            || (Offset <= Extent && Count <= Extent - Offset),
        "span::subspan<Offset, Count>: Offset + Count out of range");
    return span<element_type,
                detail::subspan_extent<Extent, Offset, Count>::value> (
        m_storage.data () + Offset,
        Count == dynamic_extent ? m_storage.size () - Offset : Count);
  }

  /**
   * @brief The first `count` elements.
   * @pre `count <= size ()`.
   * @param count The number of elements.
   * @return A span of dynamic extent.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the subview is the result")
  LUMEX_CONSTEXPR span<element_type, dynamic_extent>
  first (size_type count) const LUMEX_NOEXCEPT
  {
    return span<element_type, dynamic_extent> (m_storage.data (), count);
  }

  /**
   * @brief The last `count` elements.
   * @pre `count <= size ()`.
   * @param count The number of elements.
   * @return A span of dynamic extent.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the subview is the result")
  LUMEX_CONSTEXPR span<element_type, dynamic_extent>
  last (size_type count) const LUMEX_NOEXCEPT
  {
    return span<element_type, dynamic_extent> (
        m_storage.data () + (m_storage.size () - count), count);
  }

  /**
   * @brief The elements from `offset` on, `count` of them or all the rest.
   * @pre `offset <= size ()` and `count == dynamic_extent` or `count <=
   * size () - offset`.
   * @param offset The index of the first element.
   * @param count The number of elements, or `dynamic_extent` for the rest.
   * @return A span of dynamic extent.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the subview is the result")
  LUMEX_CONSTEXPR span<element_type, dynamic_extent>
  subspan (size_type offset,
           size_type count = dynamic_extent) const LUMEX_NOEXCEPT
  {
    return span<element_type, dynamic_extent> (
        m_storage.data () + offset,
        count == dynamic_extent ? m_storage.size () - offset : count);
  }

  // -- Observers --
  /**
   * @brief The number of elements.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the size is the result")
  LUMEX_CONSTEXPR size_type
  size () const LUMEX_NOEXCEPT
  {
    return m_storage.size ();
  }

  /**
   * @brief The size of the viewed sequence in bytes.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the size is the result")
  LUMEX_CONSTEXPR size_type
  size_bytes () const LUMEX_NOEXCEPT
  {
    return m_storage.size () * sizeof (element_type);
  }

  /**
   * @brief Tells whether the span has no elements.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("empty () does not empty the span")
  LUMEX_CONSTEXPR bool
  empty () const LUMEX_NOEXCEPT
  {
    return m_storage.size () == 0;
  }

  // -- Element access --
  /**
   * @brief The element at `index`.
   * @pre `index < size ()`.
   * @param index The index.
   * @return A reference to the element.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the element is the result")
  LUMEX_CONSTEXPR reference
  operator[] (size_type index) const LUMEX_NOEXCEPT
  {
    return m_storage.data ()[index];
  }

  /**
   * @brief The element at `index`, checked (C++26 in the standard).
   * @param index The index.
   * @return A reference to the element.
   * @throws std::out_of_range If `index >= size ()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the element is the result")
  LUMEX_CONSTEXPR reference
  at (size_type index) const
  {
    return index < m_storage.size ()
               ? m_storage.data ()[index]
               : (detail::throw_out_of_range (
                      "lumex::span::at: index out of range"),
                  m_storage.data ()[index]);
  }

  /**
   * @brief The first element.
   * @pre `!empty ()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the element is the result")
  LUMEX_CONSTEXPR reference
  front () const LUMEX_NOEXCEPT
  {
    return *m_storage.data ();
  }

  /**
   * @brief The last element.
   * @pre `!empty ()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the element is the result")
  LUMEX_CONSTEXPR reference
  back () const LUMEX_NOEXCEPT
  {
    return m_storage.data ()[m_storage.size () - 1];
  }

  /**
   * @brief A pointer to the first element; `nullptr` or a past-the-end
   * pointer when the span is empty.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the pointer is the result")
  LUMEX_CONSTEXPR pointer
  data () const LUMEX_NOEXCEPT
  {
    return m_storage.data ();
  }

  // -- Iterators --
  /**
   * @brief An iterator to the first element.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR iterator
  begin () const LUMEX_NOEXCEPT
  {
    return m_storage.data ();
  }

  /**
   * @brief An iterator to the end of the span.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR iterator
  end () const LUMEX_NOEXCEPT
  {
    return m_storage.data () + m_storage.size ();
  }

  /**
   * @brief A constant iterator to the first element (C++23 in the standard).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR const_iterator
  cbegin () const LUMEX_NOEXCEPT
  {
    return m_storage.data ();
  }

  /**
   * @brief A constant iterator to the end of the span (C++23 in the
   * standard).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR const_iterator
  cend () const LUMEX_NOEXCEPT
  {
    return m_storage.data () + m_storage.size ();
  }

  /**
   * @brief A reverse iterator to the last element.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR reverse_iterator
  rbegin () const LUMEX_NOEXCEPT
  {
    return reverse_iterator (end ());
  }

  /**
   * @brief A reverse iterator to the end of the reversed span.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR reverse_iterator
  rend () const LUMEX_NOEXCEPT
  {
    return reverse_iterator (begin ());
  }

  /**
   * @brief A constant reverse iterator to the last element (C++23 in the
   * standard).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR const_reverse_iterator
  crbegin () const LUMEX_NOEXCEPT
  {
    return const_reverse_iterator (cend ());
  }

  /**
   * @brief A constant reverse iterator to the end of the reversed span (C++23
   * in the standard).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the iterator is the result")
  LUMEX_CONSTEXPR const_reverse_iterator
  crend () const LUMEX_NOEXCEPT
  {
    return const_reverse_iterator (cbegin ());
  }

private:
  detail::span_storage<element_type, Extent> m_storage;
};

#if __cplusplus < 201703L
template <typename ElementType, std::size_t Extent>
LUMEX_CONSTEXPR typename span<ElementType, Extent>::size_type
    span<ElementType, Extent>::extent;
#endif

// -- Views of the object representation --
/**
 * @brief The bytes of a span as a read-only span of `byte`.
 * @details The extent is `sizeof (T) * Extent`, or dynamic. Does not take part
 * in overload resolution for a volatile element type.
 * @tparam T The element type.
 * @tparam Extent The extent.
 * @param view The span.
 * @return A span of `view.size_bytes ()` bytes starting at `view.data ()`.
 */
template <typename T, std::size_t Extent>
LUMEX_ATTRIBUTE_NODISCARD ("the byte view is the result")
typename std::enable_if<
    !std::is_volatile<T>::value,
    span<byte const, detail::bytes_extent<T, Extent>::value>>::type
    as_bytes (span<T, Extent> view) LUMEX_NOEXCEPT
{
  return span<byte const, detail::bytes_extent<T, Extent>::value> (
      reinterpret_cast<byte const *> ( // NOLINT
          view.data ()),
      view.size_bytes ());
}

/**
 * @brief The bytes of a span as a writable span of `byte`.
 * @details The extent is `sizeof (T) * Extent`, or dynamic. Does not take part
 * in overload resolution for a const or volatile element type.
 * @tparam T The element type.
 * @tparam Extent The extent.
 * @param view The span.
 * @return A span of `view.size_bytes ()` bytes starting at `view.data ()`.
 */
template <typename T, std::size_t Extent>
LUMEX_ATTRIBUTE_NODISCARD ("the byte view is the result")
typename std::enable_if<
    !std::is_const<T>::value && !std::is_volatile<T>::value,
    span<byte, detail::bytes_extent<T, Extent>::value>>::type
    as_writable_bytes (span<T, Extent> view) LUMEX_NOEXCEPT
{
  return span<byte, detail::bytes_extent<T, Extent>::value> (
      reinterpret_cast<byte *> ( // NOLINT
          view.data ()),
      view.size_bytes ());
}

// Deduction guides. GCC 8 reports 201606L for class template argument
// deduction at C++17, which it supports; 201703L is the later revision.
#if __cplusplus >= 201703L && LUMEX_FEATURE_DEDUCTION_GUIDES >= 201606L
template <typename Iterator, typename EndOrSize,
          typename = typename std::enable_if<
              is_contiguous_iterator<Iterator>::value>::type>
span (Iterator, EndOrSize) -> span<typename std::remove_reference<
    decltype (*std::declval<Iterator &> ())>::type>;

template <typename T, std::size_t N> span (T (&)[N]) -> span<T, N>;

template <typename T, std::size_t N> span (std::array<T, N> &) -> span<T, N>;

template <typename T, std::size_t N>
span (std::array<T, N> const &) -> span<T const, N>;

template <
    typename Range,
    typename = typename std::enable_if<
        !detail::is_span<typename detail::remove_cvref<Range>::type>::value
        && !detail::is_std_array<
            typename detail::remove_cvref<Range>::type>::value
        && !std::is_array<typename detail::remove_cvref<Range>::type>::value>::
        type>
span (Range &&) -> span<typename detail::range_element<Range>::type>;

template <typename T, std::size_t Extent>
span (span<T, Extent>) -> span<T, Extent>;
#endif

} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

#if LUMEX_SPAN_HAS_RANGES
namespace std
{
namespace ranges
{
/**
 * @brief A `span` is a borrowed range: its iterators outlive it.
 */
template <typename ElementType, std::size_t Extent>
LUMEX_INLINE_VARIABLE LUMEX_CONSTEXPR bool
    enable_borrowed_range<::lumex::core::span::view::span<ElementType, Extent>>
    = true;

/**
 * @brief A `span` is a view: copying it is cheap and does not copy elements.
 */
template <typename ElementType, std::size_t Extent>
LUMEX_INLINE_VARIABLE LUMEX_CONSTEXPR bool
    enable_view<::lumex::core::span::view::span<ElementType, Extent>>
    = true;
} // namespace ranges
} // namespace std
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_SPAN_VIEW_SPAN_HPP
