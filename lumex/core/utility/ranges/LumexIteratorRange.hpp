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

/*
 * The structure of the classes in this file is taken from pugixml
 * (https://pugixml.org, MIT license, Copyright (c) 2006-2026 Arseny
 * Kapoulkine); the functions are Lumex's own code. The notice is in
 * THIRD-PARTY-NOTICES.md, which is installed with LumexLib.
 */

/**
 * @file LumexIteratorRange.hpp
 * @brief `iterator_range`, a pair of iterators that lets a range-based for
 * loop walk any sequence the iterators describe.
 * @details The class template stores a begin and an end iterator and exposes
 * them through `begin()` and `end()`, plus `empty()`, and `size()` when the
 * iterators are random access. It owns nothing: it is valid as long as the
 * iterators are, and the sequence they walk must outlive it. Header-only,
 * works from C++11, no dependency beyond the attribute macros and the
 * standard headers. The XML module returns it from `XmlNode::children` and
 * `XmlNode::attributes`; `core_dump_generator::get_memory_filters_range`
 * returns it in every standard (`size()` matches the `size()` of a
 * `std::ranges::ref_view`). With C++20 and a library that has `<ranges>` the
 * class opts into `std::ranges::enable_borrowed_range` and
 * `std::ranges::enable_view`, as `span` does: it is a view whose iterators
 * outlive it, so it can be piped into views as an rvalue.
 */
#ifndef LUMEX_CORE_UTILITY_RANGES_ITERATOR_RANGE_HPP
#define LUMEX_CORE_UTILITY_RANGES_ITERATOR_RANGE_HPP

#include <cstddef>
#include <iterator>
#include <type_traits>
#if __cplusplus >= 202002L && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace utility
{
namespace ranges
{
/**
 * @brief A lightweight view on a sequence, defined by a pair of iterators.
 * @details `iterator_range` gives a pair of iterators the interface a
 * range-based for loop needs, without copying or modifying the sequence:
 * `for (auto const &x : iterator_range<It> (first, last)) { ... }`. The range
 * has no mutating members; it is copyable whenever the iterator is.
 * @tparam Iterator A forward iterator type (copyable, comparable for equality,
 * dereferenceable). A single-pass input iterator also works in one loop, as
 * the range only hands out copies of the stored iterators.
 * @warning The range does not own the data the iterators point into. The
 * lifetime of the underlying sequence must be managed by the caller.
 */
template <typename Iterator> class iterator_range
{
public:
  /// @brief The iterator type; the same as `iterator`, since the range gives
  /// no mutating access of its own.
  using const_iterator = Iterator;

  /// @brief The iterator type.
  using iterator = Iterator;

  /**
   * @brief Builds a range from its two ends.
   * @param[in] begin Iterator pointing to the first element of the range.
   * @param[in] end Iterator pointing to the position after the last element.
   */
  iterator_range (Iterator begin, Iterator end) : m_begin (begin), m_end (end)
  {
  }

  /**
   * @brief Returns the iterator to the beginning of the range.
   * @return Iterator pointing to the first element of the range.
   */
  Iterator
  begin () const
  {
    return m_begin;
  }

  /**
   * @brief Returns the iterator to the end of the range.
   * @return Iterator pointing to the position after the last element.
   */
  Iterator
  end () const
  {
    return m_end;
  }

  /**
   * @brief Returns the number of elements in the range.
   * @details Present only when
   * `std::iterator_traits<Iterator>::iterator_category` is
   * `std::random_access_iterator_tag` or a tag derived from it (so a pointer,
   * a `std::vector`, `std::deque`, `std::string` or `std::array` iterator
   * qualifies and a `std::list`, `std::set` or `std::forward_list` iterator
   * does not): counting the elements of any other range would walk it, which
   * `size()` must not hide. The member does not exist for those iterators, so
   * a call is a compile-time error and `has_size` style detection reports
   * `false`. It makes the range usable where `std::ranges::ref_view` was used
   * from C++20, which has `size()` for a vector.
   * @tparam RangeIterator The iterator type of the range (always `Iterator`;
   * a member template argument of its own keeps the constraint a
   * substitution failure of the member instead of an error of the class).
   * @return `end() - begin()` as `std::size_t`. The range must be valid: a
   * range whose end lies before its beginning gives a meaningless value.
   */
  template <typename RangeIterator = Iterator,
            typename std::enable_if<
                std::is_base_of<std::random_access_iterator_tag,
                                typename std::iterator_traits<
                                    RangeIterator>::iterator_category>::value,
                int>::type
            = 0>
  LUMEX_ATTRIBUTE_NODISCARD ("The returned value is the number of elements; "
                             "discarding it negates the purpose of the "
                             "getter.")
  std::size_t size () const
  {
    return static_cast<std::size_t> (m_end - m_begin);
  }

  /**
   * @brief Checks if the range is empty.
   * @details The range is empty when the beginning iterator equals the end
   * iterator.
   * @return `true` if the range is empty; `false` otherwise.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The returned value tells whether the range "
                             "has elements; discarding it negates the "
                             "purpose of the getter.")
  bool
  empty () const
  {
    return m_begin == m_end;
  }

private:
  Iterator m_begin; ///< Iterator pointing to the beginning of the range.
  Iterator m_end;   ///< Iterator pointing to the end of the range (position
                    ///< after the last element).
};
} // namespace ranges
} // namespace utility
} // namespace core
} // namespace lumex

#if LUMEX_HAS_STD_RANGES
namespace std
{
namespace ranges
{
/**
 * @brief An `iterator_range` is a borrowed range: its iterators outlive it.
 */
template <typename Iterator>
inline constexpr bool enable_borrowed_range<
    ::lumex::core::utility::ranges::iterator_range<Iterator>>
    = true;

/**
 * @brief An `iterator_range` is a view: copying it copies two iterators, not
 * the elements.
 */
template <typename Iterator>
inline constexpr bool
    enable_view<::lumex::core::utility::ranges::iterator_range<Iterator>>
    = true;
} // namespace ranges
} // namespace std
#endif

#endif // !LUMEX_CORE_UTILITY_RANGES_ITERATOR_RANGE_HPP
