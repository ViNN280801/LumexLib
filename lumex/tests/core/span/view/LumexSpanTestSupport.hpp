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
 * @file LumexSpanTestSupport.hpp
 * @brief Shared helpers of the span tests.
 * @details Detection traits for what the constructors accept (the explicit
 * and implicit forms are told apart with copy-list-initialization and
 * conversion), two iterator classes over a pointer (one that is registered
 * as contiguous and one that is not), element types for the qualification
 * conversion rules and a counting element type for the tests that must see
 * that no element is copied.
 */
#ifndef LUMEX_TESTS_CORE_SPAN_VIEW_SPAN_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_SPAN_VIEW_SPAN_TEST_SUPPORT_HPP

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

#include "lumex/core/span/LumexSpan"

namespace lumex_span_test
{
template <typename T> struct make_void
{
  using type = void;
};

/// True when `Span f ({a, b})` compiles: copy-list-initialization from two
/// values, which fails when the chosen constructor is explicit.
template <typename Span, typename A, typename B, typename = void>
struct is_implicitly_listable : std::false_type
{
};

template <typename Span, typename A, typename B>
struct is_implicitly_listable<
    Span, A, B,
    typename make_void<decltype (std::declval<void (&) (Span)> () (
        { std::declval<A> (), std::declval<B> () }))>::type> : std::true_type
{
};

/// True when a `Span` can be made from `Source` with direct-initialization.
template <typename Span, typename Source>
using is_explicit_or_implicit = std::is_constructible<Span, Source>;

/// True when `Source` converts to `Span` implicitly.
template <typename Span, typename Source>
using is_implicit = std::is_convertible<Source, Span>;

/// True when `Source` makes a `Span` only with a direct-initialization.
template <typename Span, typename Source>
struct is_explicit_only
    : std::integral_constant<bool,
                             std::is_constructible<Span, Source>::value
                                 && !std::is_convertible<Source, Span>::value>
{
};

/// A contiguous iterator class: registered with `is_contiguous_iterator`.
template <typename T> class pointer_iterator
{
public:
  using iterator_category = std::random_access_iterator_tag;
#if LUMEX_SPAN_HAS_CONCEPTS
  using iterator_concept = std::contiguous_iterator_tag;
#endif
  using value_type = typename std::remove_cv<T>::type;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

  pointer_iterator () : m_ptr (nullptr) {}
  explicit pointer_iterator (T *ptr) : m_ptr (ptr) {}

  reference
  operator* () const
  {
    return *m_ptr;
  }
  pointer
  operator->() const
  {
    return m_ptr;
  }
  reference
  operator[] (difference_type n) const
  {
    return m_ptr[n];
  }
  pointer_iterator &
  operator++ ()
  {
    ++m_ptr;
    return *this;
  }
  pointer_iterator
  operator++ (int)
  {
    pointer_iterator copy (*this);
    ++m_ptr;
    return copy;
  }
  pointer_iterator &
  operator-- ()
  {
    --m_ptr;
    return *this;
  }
  pointer_iterator
  operator-- (int)
  {
    pointer_iterator copy (*this);
    --m_ptr;
    return copy;
  }
  pointer_iterator &
  operator+= (difference_type n)
  {
    m_ptr += n;
    return *this;
  }
  pointer_iterator &
  operator-= (difference_type n)
  {
    m_ptr -= n;
    return *this;
  }
  friend pointer_iterator
  operator+ (pointer_iterator it, difference_type n)
  {
    return it += n;
  }
  friend pointer_iterator
  operator+ (difference_type n, pointer_iterator it)
  {
    return it += n;
  }
  friend pointer_iterator
  operator- (pointer_iterator it, difference_type n)
  {
    return it -= n;
  }
  friend difference_type
  operator- (pointer_iterator lhs, pointer_iterator rhs)
  {
    return lhs.m_ptr - rhs.m_ptr;
  }
  friend bool
  operator== (pointer_iterator lhs, pointer_iterator rhs)
  {
    return lhs.m_ptr == rhs.m_ptr;
  }
  friend bool
  operator!= (pointer_iterator lhs, pointer_iterator rhs)
  {
    return lhs.m_ptr != rhs.m_ptr;
  }
  friend bool
  operator< (pointer_iterator lhs, pointer_iterator rhs)
  {
    return lhs.m_ptr < rhs.m_ptr;
  }
  friend bool
  operator> (pointer_iterator lhs, pointer_iterator rhs)
  {
    return lhs.m_ptr > rhs.m_ptr;
  }
  friend bool
  operator<= (pointer_iterator lhs, pointer_iterator rhs)
  {
    return lhs.m_ptr <= rhs.m_ptr;
  }
  friend bool
  operator>= (pointer_iterator lhs, pointer_iterator rhs)
  {
    return lhs.m_ptr >= rhs.m_ptr;
  }

private:
  T *m_ptr;
};

/// A random-access iterator class that is not registered as contiguous.
template <typename T> class unregistered_iterator
{
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = typename std::remove_cv<T>::type;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

  unregistered_iterator () : m_ptr (nullptr) {}
  explicit unregistered_iterator (T *ptr) : m_ptr (ptr) {}

  reference
  operator* () const
  {
    return *m_ptr;
  }
  pointer
  operator->() const
  {
    return m_ptr;
  }
  unregistered_iterator &
  operator++ ()
  {
    ++m_ptr;
    return *this;
  }
  friend difference_type
  operator- (unregistered_iterator lhs, unregistered_iterator rhs)
  {
    return lhs.m_ptr - rhs.m_ptr;
  }
  friend bool
  operator== (unregistered_iterator lhs, unregistered_iterator rhs)
  {
    return lhs.m_ptr == rhs.m_ptr;
  }
  friend bool
  operator!= (unregistered_iterator lhs, unregistered_iterator rhs)
  {
    return lhs.m_ptr != rhs.m_ptr;
  }

private:
  T *m_ptr;
};

/// A contiguous iterator class whose operator* throws at the end of its range,
/// the way a checked iterator reports a dereference of end (): a span of an
/// empty range must not dereference it. operator-> stays unchecked, as
/// std::to_address requires.
template <typename T> class guarded_iterator
{
public:
  using iterator_category = std::random_access_iterator_tag;
#if LUMEX_SPAN_HAS_CONCEPTS
  using iterator_concept = std::contiguous_iterator_tag;
#endif
  using value_type = typename std::remove_cv<T>::type;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

  guarded_iterator () : m_ptr (nullptr), m_end (nullptr) {}
  guarded_iterator (T *ptr, T *end) : m_ptr (ptr), m_end (end) {}

  reference
  operator* () const
  {
    if (m_ptr == m_end)
      throw 0;
    return *m_ptr;
  }
  pointer
  operator->() const
  {
    return m_ptr;
  }
  reference
  operator[] (difference_type n) const
  {
    return m_ptr[n];
  }
  guarded_iterator &
  operator++ ()
  {
    ++m_ptr;
    return *this;
  }
  guarded_iterator
  operator++ (int)
  {
    guarded_iterator copy (*this);
    ++m_ptr;
    return copy;
  }
  guarded_iterator &
  operator-- ()
  {
    --m_ptr;
    return *this;
  }
  guarded_iterator
  operator-- (int)
  {
    guarded_iterator copy (*this);
    --m_ptr;
    return copy;
  }
  guarded_iterator &
  operator+= (difference_type n)
  {
    m_ptr += n;
    return *this;
  }
  guarded_iterator &
  operator-= (difference_type n)
  {
    m_ptr -= n;
    return *this;
  }
  friend guarded_iterator
  operator+ (guarded_iterator it, difference_type n)
  {
    return it += n;
  }
  friend guarded_iterator
  operator+ (difference_type n, guarded_iterator it)
  {
    return it += n;
  }
  friend guarded_iterator
  operator- (guarded_iterator it, difference_type n)
  {
    return it -= n;
  }
  friend difference_type
  operator- (guarded_iterator lhs, guarded_iterator rhs)
  {
    return lhs.m_ptr - rhs.m_ptr;
  }
  friend bool
  operator== (guarded_iterator lhs, guarded_iterator rhs)
  {
    return lhs.m_ptr == rhs.m_ptr;
  }
  friend bool
  operator!= (guarded_iterator lhs, guarded_iterator rhs)
  {
    return lhs.m_ptr != rhs.m_ptr;
  }
  friend bool
  operator< (guarded_iterator lhs, guarded_iterator rhs)
  {
    return lhs.m_ptr < rhs.m_ptr;
  }
  friend bool
  operator> (guarded_iterator lhs, guarded_iterator rhs)
  {
    return lhs.m_ptr > rhs.m_ptr;
  }
  friend bool
  operator<= (guarded_iterator lhs, guarded_iterator rhs)
  {
    return lhs.m_ptr <= rhs.m_ptr;
  }
  friend bool
  operator>= (guarded_iterator lhs, guarded_iterator rhs)
  {
    return lhs.m_ptr >= rhs.m_ptr;
  }

private:
  T *m_ptr;
  T *m_end;
};

/// A sentinel that is a count: it converts to std::size_t, compares with a
/// pointer_iterator and can also be subtracted from it. The count form of the
/// span constructor must take it, not the pair form.
struct count_sentinel
{
  std::size_t count;
  operator std::size_t () const { return count; }
};

template <typename T>
std::ptrdiff_t
operator- (count_sentinel sentinel, pointer_iterator<T> iterator)
{
  (void)iterator;
  // Deliberately not the count: a pair form built on this difference would
  // give a different size than the count form does.
  return static_cast<std::ptrdiff_t> (sentinel.count) + 100;
}

template <typename T>
std::ptrdiff_t
operator- (pointer_iterator<T> iterator, count_sentinel sentinel)
{
  return -(sentinel - iterator);
}

template <typename T>
bool
operator== (pointer_iterator<T> iterator, count_sentinel sentinel)
{
  (void)iterator;
  (void)sentinel;
  return false;
}

template <typename T>
bool
operator!= (pointer_iterator<T> iterator, count_sentinel sentinel)
{
  return !(iterator == sentinel);
}

template <typename T>
bool
operator== (count_sentinel sentinel, pointer_iterator<T> iterator)
{
  return iterator == sentinel;
}

template <typename T>
bool
operator!= (count_sentinel sentinel, pointer_iterator<T> iterator)
{
  return !(iterator == sentinel);
}

/// Element type pair for the conversion rules: a derived class converts to its
/// base by a pointer conversion, which a span must not allow.
struct base_element
{
  int value;
};

struct derived_element : base_element
{
  int extra;
};

/// A class with a user-provided copy constructor that counts its copies.
struct counted_element
{
  int value;

  static int &
  copies ()
  {
    static int count = 0;
    return count;
  }

  counted_element () : value (0) {}
  explicit counted_element (int v) : value (v) {}
  counted_element (counted_element const &other) : value (other.value)
  {
    ++copies ();
  }
  counted_element &
  operator= (counted_element const &other)
  {
    value = other.value;
    ++copies ();
    return *this;
  }
};
} // namespace lumex_span_test

namespace lumex
{
namespace core
{
namespace span
{
namespace view
{
template <typename T>
struct is_contiguous_iterator<::lumex_span_test::pointer_iterator<T>>
    : std::true_type
{
};

template <typename T>
struct is_contiguous_iterator<::lumex_span_test::guarded_iterator<T>>
    : std::true_type
{
};
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

#endif // !LUMEX_TESTS_CORE_SPAN_VIEW_SPAN_TEST_SUPPORT_HPP
