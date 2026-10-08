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
 * @file LumexRanges.hpp
 * @brief `get_nearest_to()`, which finds the element of a sorted range whose
 * value is numerically nearest to a given value.
 * @details Works from C++11. It searches for the first element not ordered
 * before the value (`std::ranges::lower_bound` from C++20, `std::lower_bound`
 * below) and then compares the element found with its predecessor, so the
 * range must be sorted by the same comparison and projection. It accepts an
 * iterator and sentinel pair or a range, plus an optional comparison and
 * projection as the standard range algorithms do, and returns the end
 * iterator only for an empty range. The distance and the numeric trait come
 * from the `lumex::math` module.
 *
 * The constraints are one `std::enable_if` form in every standard, so a call
 * that does not fit finds no overload: the iterators are bidirectional, the
 * sentinel compares with them, the projection is invocable with an element
 * and gives a numeric type, the comparison is invocable with the value and
 * the projected element in both orders, and the value type is numeric. From
 * C++20 (`LUMEX_HAS_STD_RANGES`) the iterator, sentinel, range and comparison
 * requirements are the standard concepts (`std::bidirectional_iterator`,
 * `std::sentinel_for`, `std::ranges::bidirectional_range`,
 * `std::indirect_strict_weak_order`), which also accept the iterators of the
 * C++20 views that claim a weaker `iterator_category`; below C++20 they are
 * the C++11 forms (`iterator_category`, the comparisons `==` and `!=`, the
 * invocability of the callables).
 *
 * A projection is a callable or a pointer to a data member or to a member
 * function without arguments, called as `std::invoke` does: `std::invoke`
 * itself from C++17, a private equivalent over `std::mem_fn` below it. The
 * default projection is `functional::identity` and the default comparison is
 * `functional::less` (`std::identity` with C++20 ranges and `std::less<void>`
 * from C++14, own function objects below).
 *
 * A range argument must outlive the call: an lvalue, or from C++20 a range
 * that is `std::ranges::borrowed_range` (`std::span`, `std::string_view`,
 * `std::views::iota`). A temporary container finds no overload.
 */
#ifndef LUMEX_CORE_UTILITY_RANGES_RANGES_HPP
#define LUMEX_CORE_UTILITY_RANGES_RANGES_HPP

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

#include <algorithm>
#include <functional>
#include <iterator>
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif
#include <type_traits>
#include <utility>

#include "lumex/core/math/LumexMath"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/functional/LumexFunctional.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace ranges
{
namespace Algorithm
{
namespace Detail
{
// distance and the numeric trait come from the real lumex::core::Math module
// (see lumex/core/math/LumexMath).
using lumex::core::math::ops::distance;
using lumex::core::math::ops::traits::is_numeric;

// --- calling a projection ---------------------------------------------- //

#if LUMEX_HAS_STD_INVOKE
/**
 * @brief `projection (argument)` with the INVOKE rules: a callable, or a
 * pointer to a data member or a member function (`std::invoke`, C++17).
 */
template <typename Projection, typename Argument>
LUMEX_CONSTEXPR auto
invoke_projection (Projection &projection, Argument &&argument)
    -> decltype (std::invoke (projection, std::forward<Argument> (argument)))
{
  return std::invoke (projection, std::forward<Argument> (argument));
}
#else
/**
 * @brief A pointer to a member is called through `std::mem_fn`, which has the
 * INVOKE rules of member pointers (object, reference, pointer or smart
 * pointer to the object).
 */
template <typename Projection, typename Argument>
auto
invoke_projection (Projection &projection, Argument &&argument, std::true_type)
    -> decltype (std::mem_fn (projection) (std::forward<Argument> (argument)))
{
  return std::mem_fn (projection) (std::forward<Argument> (argument));
}

/** @brief Any other callable is called as it is. */
template <typename Projection, typename Argument>
auto
invoke_projection (Projection &projection, Argument &&argument,
                   std::false_type)
    -> decltype (projection (std::forward<Argument> (argument)))
{
  return projection (std::forward<Argument> (argument));
}

/**
 * @brief `projection (argument)` with the INVOKE rules: a callable, or a
 * pointer to a data member or a member function. `std::invoke` is C++17;
 * the library has no `invoke` function of its own, only the traits.
 */
template <typename Projection, typename Argument>
auto
invoke_projection (Projection &projection, Argument &&argument)
    -> decltype (invoke_projection (
        projection, std::forward<Argument> (argument),
        std::is_member_pointer<typename std::remove_cv<Projection>::type> ()))
{
  return invoke_projection (
      projection, std::forward<Argument> (argument),
      std::is_member_pointer<typename std::remove_cv<Projection>::type> ());
}
#endif

/// @brief `invoke_projection (projection, *iterator)` is well-formed;
/// `type` is its result.
template <typename Iterator, typename Projection, typename = void>
struct projected_result
{
};

template <typename Iterator, typename Projection>
struct projected_result<
    Iterator, Projection,
    typename std::enable_if<traits::invoke::is_invocable<
        Projection &, decltype (*std::declval<Iterator &> ())>::value>::type>
{
  using type = decltype (invoke_projection (std::declval<Projection &> (),
                                            *std::declval<Iterator &> ()));
};

// --- what the iterators, the range and the callables must be ----------- //

#if LUMEX_HAS_STD_RANGES
// C++20: the standard concepts decide, so that the iterators of the views
// whose iterator_category is weaker than their iterator_concept still fit.
template <typename Iterator>
struct is_bidirectional_iterator
    : std::integral_constant<bool, std::bidirectional_iterator<Iterator>>
{
};

template <typename Sentinel, typename Iterator>
struct is_sentinel_for
    : std::integral_constant<bool, std::sentinel_for<Sentinel, Iterator>>
{
};

/// @brief `pred (projected element, value)` and `pred (value, projected
/// element)` are the calls of a strict weak order.
template <typename Iterator, typename Value, typename Predicate,
          typename Projection>
struct is_order_for
    : std::integral_constant<bool, std::indirect_strict_weak_order<
                                       Predicate, Value const *,
                                       std::projected<Iterator, Projection>>>
{
};

/// @brief A range that outlives the call and can be walked in both
/// directions.
template <typename Range>
struct is_usable_range
    : std::integral_constant<bool, std::ranges::bidirectional_range<Range>
                                       && std::ranges::borrowed_range<Range>>
{
};

template <typename Range, bool = is_usable_range<Range>::value>
struct range_types
{
};

template <typename Range> struct range_types<Range, true>
{
  using iterator = std::ranges::iterator_t<Range>;
  using sentinel = std::ranges::sentinel_t<Range>;
};

template <typename Range>
LUMEX_CONSTEXPR auto
begin_of (Range &&range)
    -> decltype (std::ranges::begin (std::forward<Range> (range)))
{
  return std::ranges::begin (std::forward<Range> (range));
}

template <typename Range>
LUMEX_CONSTEXPR auto
end_of (Range &&range)
    -> decltype (std::ranges::end (std::forward<Range> (range)))
{
  return std::ranges::end (std::forward<Range> (range));
}

template <typename Iterator, typename Value, typename Predicate,
          typename Projection>
LUMEX_CONSTEXPR Iterator
lower_bound_of (Iterator first, Iterator last, Value const &value,
                Predicate &predicate, Projection &projection)
{
  return std::ranges::lower_bound (first, last, value, predicate, projection);
}
#else
/// @brief `std::iterator_traits<Iterator>::iterator_category`; no `type` for
/// anything without one. A pointer to a function or to `void` is left out
/// before the traits are asked: libstdc++ 8 forms `void &` in
/// `iterator_traits<void *>` and stops with an error instead of leaving the
/// traits empty.
template <typename Iterator, typename = void> struct category_of_iterator
{
};

template <typename Iterator>
struct category_of_iterator<
    Iterator, traits::meta::void_t<
                  typename std::iterator_traits<Iterator>::iterator_category>>
{
  using type = typename std::iterator_traits<Iterator>::iterator_category;
};

template <typename Iterator,
          bool = !std::is_pointer<Iterator>::value
                 || std::is_object<
                     typename std::remove_pointer<Iterator>::type>::value>
struct iterator_category_of
{
};

template <typename Iterator>
struct iterator_category_of<Iterator, true> : category_of_iterator<Iterator>
{
};

template <typename Iterator, typename = void>
struct is_bidirectional_iterator : std::false_type
{
};

template <typename Iterator>
struct is_bidirectional_iterator<
    Iterator,
    traits::meta::void_t<typename iterator_category_of<Iterator>::type>>
    : std::is_convertible<typename iterator_category_of<Iterator>::type,
                          std::bidirectional_iterator_tag>
{
};

template <typename Sentinel, typename Iterator, typename = void>
struct is_sentinel_for : std::false_type
{
};

template <typename Sentinel, typename Iterator>
struct is_sentinel_for<
    Sentinel, Iterator,
    traits::meta::void_t<decltype (std::declval<Iterator const &> ()
                                   == std::declval<Sentinel const &> ()),
                         decltype (std::declval<Iterator const &> ()
                                   != std::declval<Sentinel const &> ())>>
    : std::integral_constant<
          bool,
          std::is_convertible<decltype (std::declval<Iterator const &> ()
                                        == std::declval<Sentinel const &> ()),
                              bool>::value
              && std::is_convertible<
                  decltype (std::declval<Iterator const &> ()
                            != std::declval<Sentinel const &> ()),
                  bool>::value>
{
};

/// @brief The comparison can be called as `pred (projected, value)` and
/// `pred (value, projected)` and gives something convertible to `bool`.
template <typename Predicate, typename Value, typename Projected,
          typename = void>
struct is_order_between : std::false_type
{
};

template <typename Predicate, typename Value, typename Projected>
struct is_order_between<
    Predicate, Value, Projected,
    traits::meta::void_t<
        decltype (std::declval<Predicate &> () (
            std::declval<Projected> (), std::declval<Value const &> ())),
        decltype (std::declval<Predicate &> () (std::declval<Value const &> (),
                                                std::declval<Projected> ()))>>
    : std::integral_constant<
          bool,
          std::is_convertible<decltype (std::declval<Predicate &> () (
                                  std::declval<Projected> (),
                                  std::declval<Value const &> ())),
                              bool>::value
              && std::is_convertible<decltype (std::declval<Predicate &> () (
                                         std::declval<Value const &> (),
                                         std::declval<Projected> ())),
                                     bool>::value>
{
};

template <typename Iterator, typename Value, typename Predicate,
          typename Projection, typename = void>
struct is_order_for : std::false_type
{
};

template <typename Iterator, typename Value, typename Predicate,
          typename Projection>
struct is_order_for<Iterator, Value, Predicate, Projection,
                    traits::meta::void_t<
                        typename projected_result<Iterator, Projection>::type>>
    : is_order_between<Predicate, Value,
                       typename projected_result<Iterator, Projection>::type>
{
};

using std::begin;
using std::end;

/// @brief The iterator and sentinel types of an lvalue range (`Range` is a
/// reference type); no members for anything else, so a temporary range finds
/// no overload.
template <typename Range, typename = void> struct range_types
{
};

template <typename Range>
struct range_types<
    Range,
    typename std::enable_if<
        std::is_lvalue_reference<Range>::value,
        traits::meta::void_t<decltype (begin (std::declval<Range> ())),
                             decltype (end (std::declval<Range> ()))>>::type>
{
  using iterator = decltype (begin (std::declval<Range> ()));
  using sentinel = decltype (end (std::declval<Range> ()));
};

template <typename Range>
auto
begin_of (Range &&range) -> decltype (begin (std::forward<Range> (range)))
{
  return begin (std::forward<Range> (range));
}

template <typename Range>
auto
end_of (Range &&range) -> decltype (end (std::forward<Range> (range)))
{
  return end (std::forward<Range> (range));
}

/**
 * @brief "The element is ordered before the value": `pred (projected
 * element, value)`, the call `std::lower_bound` makes.
 */
template <typename Predicate, typename Projection> struct element_before_value
{
  Predicate predicate;
  Projection projection;

  template <typename Element, typename Value>
  bool
  operator() (Element &&element, Value const &value)
  {
    return static_cast<bool> (predicate (
        invoke_projection (projection, std::forward<Element> (element)),
        value));
  }
};

template <typename Iterator, typename Value, typename Predicate,
          typename Projection>
Iterator
lower_bound_of (Iterator first, Iterator last, Value const &value,
                Predicate &predicate, Projection &projection)
{
  element_before_value<Predicate, Projection> before
      = { predicate, projection };
  return std::lower_bound (first, last, value, before);
}
#endif

/**
 * @brief The iterator that `last` stands for: `last` itself when it is an
 * iterator of the same type, the iterator reached by walking from `first`
 * when it is a sentinel of another type.
 */
template <typename Iterator>
LUMEX_CONSTEXPR_CXX14 Iterator
iterator_at (Iterator, Iterator last)
{
  return last;
}

template <typename Iterator, typename Sentinel>
LUMEX_CONSTEXPR_CXX14 Iterator
iterator_at (Iterator first, Sentinel const &last)
{
  while (first != last)
    ++first;
  return first;
}

// --- the constraints, checked step by step ----------------------------- //

/// @brief Third step: the projection is invocable with an element, gives a
/// numeric type, and the comparison fits the value and that type.
template <
    typename Iterator, typename Value, typename Predicate, typename Projection,
    bool
    = traits::meta::has_type<projected_result<Iterator, Projection>>::value>
struct has_numeric_projection : std::false_type
{
};

template <typename Iterator, typename Value, typename Predicate,
          typename Projection>
struct has_numeric_projection<Iterator, Value, Predicate, Projection, true>
    : std::integral_constant<
          bool,
          is_numeric<typename std::decay<typename projected_result<
              Iterator, Projection>::type>::type>::value
              && is_order_for<Iterator, Value, Predicate, Projection>::value>
{
};

/// @brief Second step: the iterator is bidirectional and the sentinel fits it;
/// only then are the callables looked at.
template <typename Iterator, typename Sentinel, typename Value,
          typename Predicate, typename Projection,
          bool = is_bidirectional_iterator<Iterator>::value
                 && is_sentinel_for<Sentinel, Iterator>::value>
struct has_valid_iterators : std::false_type
{
};

template <typename Iterator, typename Sentinel, typename Value,
          typename Predicate, typename Projection>
struct has_valid_iterators<Iterator, Sentinel, Value, Predicate, Projection,
                           true>
    : has_numeric_projection<Iterator, Value, Predicate, Projection>
{
};

/**
 * @brief The iterator and sentinel overload of `get_nearest_to` exists for
 * these types.
 */
template <typename Iterator, typename Sentinel, typename Value,
          typename Predicate, typename Projection>
struct is_nearest_call
    : std::integral_constant<
          bool, is_numeric<Value>::value
                    && has_valid_iterators<Iterator, Sentinel, Value,
                                           Predicate, Projection>::value>
{
};

/// @brief The range overload of `get_nearest_to` exists for these types.
template <typename Range, typename Value, typename Predicate,
          typename Projection, typename = void>
struct is_nearest_range_call : std::false_type
{
};

template <typename Range, typename Value, typename Predicate,
          typename Projection>
struct is_nearest_range_call<
    Range, Value, Predicate, Projection,
    traits::meta::void_t<typename range_types<Range>::iterator>>
    : is_nearest_call<typename range_types<Range>::iterator,
                      typename range_types<Range>::sentinel, Value, Predicate,
                      Projection>
{
};
} // namespace Detail

/**
 * @brief Finds the iterator whose (projected) value is numerically nearest to
 * `value`.
 * @details Requires the range to be sorted according to `pred`/`proj` (uses
 * `std::ranges::lower_bound` from C++20 and `std::lower_bound` below), then
 * compares the found element and its predecessor to pick whichever is closer
 * to `value`; on a tie the predecessor wins. A sentinel of another type than
 * the iterator is walked to once, before the search.
 * @tparam IteratorType Bidirectional iterator over the range.
 * @tparam Sentinel Sentinel type for `IteratorType`.
 * @tparam ValueType Numeric type of `value` (see `is_numeric` of the math
 * module).
 * @tparam Projection Projection applied to elements before comparison
 * (defaults to `functional::identity`); a callable or a pointer to a member.
 * @tparam Predicate Strict-weak-order predicate used for the search (defaults
 * to `functional::less`).
 * @return Iterator to the nearest element, or the end iterator if
 * `first == last`.
 */
template <typename IteratorType, typename Sentinel, typename ValueType,
          typename Projection = functional::identity,
          typename Predicate = functional::less,
          typename std::enable_if<
              Detail::is_nearest_call<IteratorType, Sentinel, ValueType,
                                      Predicate, Projection>::value,
              int>::type
          = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
LUMEX_CONSTEXPR_CXX14 IteratorType
    get_nearest_to (IteratorType first, Sentinel last, ValueType const &value,
                    Predicate pred = Predicate (),
                    Projection proj = Projection ())
{
  IteratorType const end_position = Detail::iterator_at (first, last);
  if (first == end_position)
    return end_position;

  IteratorType iter
      = Detail::lower_bound_of (first, end_position, value, pred, proj);
  if (iter == end_position)
    {
      --iter;
      return iter;
    }
  if (iter == first)
    return iter;

  IteratorType previous = iter;
  --previous;

  auto const distBetweenFoundAndSpecified
      = Detail::distance (Detail::invoke_projection (proj, *iter), value);
  auto const distBetweenPrevAndSpecified
      = Detail::distance (Detail::invoke_projection (proj, *previous), value);

  return (distBetweenFoundAndSpecified < distBetweenPrevAndSpecified)
             ? iter
             : previous;
}

/**
 * @brief Range-based overload of get_nearest_to().
 * @details `RangeType` is deduced as a forwarding reference. The range must
 * outlive the call (see the file comment): below C++20 only an lvalue range
 * is accepted, from C++20 also a borrowed range.
 */
template <typename RangeType, typename ValueType,
          typename Projection = functional::identity,
          typename Predicate = functional::less,
          typename std::enable_if<
              Detail::is_nearest_range_call<RangeType, ValueType, Predicate,
                                            Projection>::value,
              int>::type
          = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
LUMEX_CONSTEXPR_CXX14 typename Detail::range_types<RangeType>::iterator
    get_nearest_to (RangeType &&range, ValueType const &value,
                    Predicate pred = Predicate (),
                    Projection proj = Projection ())
{
  return get_nearest_to (Detail::begin_of (std::forward<RangeType> (range)),
                         Detail::end_of (std::forward<RangeType> (range)),
                         value, pred, proj);
}
} // namespace Algorithm
} // namespace ranges
} // namespace utility
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_RANGES_RANGES_HPP
