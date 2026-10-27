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
 * @file LumexProtocolTypeList.hpp
 * @brief `LUMEX_PP_UNWRAP` and the compile-time type lists of the protocol
 * DSL: `type_list`, `list_of`, `list_size`, `list_contains`, `concat`,
 * `append_unique`.
 * @details The DSL never loops over fields in the preprocessor: a
 * parenthesized group of a table row is unwrapped once into a template
 * argument list, and every operation on the rows is a template here. The
 * lists have no length limit beyond the compiler's template argument limit.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_TYPE_LIST_HPP
#define LUMEX_CORE_PROTOCOL_DSL_TYPE_LIST_HPP

#include <cstddef>
#include <type_traits>

/**
 * @brief Removes the parentheses of a group: `LUMEX_PP_UNWRAP (a, b)` is
 * `a, b`.
 * @details Written `LUMEX_PP_UNWRAP group` where `group` is a parenthesized
 * macro argument, it expands the group once, whatever the number of its
 * elements and whatever commas they contain (`std::ratio<1, 100>`). An empty
 * group gives nothing. Works with the traditional MSVC preprocessor too: the
 * elements are never split again by another macro.
 */
#define LUMEX_PP_UNWRAP(...) __VA_ARGS__

namespace lumex
{
namespace core
{
namespace protocol
{
namespace dsl
{
/// @brief An ordered list of types (rows, fields).
template <typename... T> struct type_list
{
};

/**
 * @brief `list_of<Sentinel, T...>::type` is `type_list<T...>`: the first
 * argument is dropped.
 * @details The X-macro expansion of a row list writes `, Row_t` per row
 * after a leading `void`, so that an empty list and the last row need no
 * special case.
 */
template <typename Sentinel, typename... T> struct list_of
{
  /// The list without the sentinel.
  typedef type_list<T...> type;
};

/// @brief The number of types of a `type_list`.
template <typename List> struct list_size;

/// @copydoc list_size
template <typename... T>
struct list_size<type_list<T...>>
    : std::integral_constant<std::size_t, sizeof...(T)>
{
};

/// @brief `list_at<I, List>::type` is the type at index `I` of `List`.
template <std::size_t I, typename List> struct list_at;

/// @copydoc list_at
template <std::size_t I, typename Head, typename... Tail>
struct list_at<I, type_list<Head, Tail...>>
    : list_at<I - 1, type_list<Tail...>>
{
};

/// @copydoc list_at
template <typename Head, typename... Tail>
struct list_at<0, type_list<Head, Tail...>>
{
  /// The type at the index.
  typedef Head type;
};

namespace detail
{
template <typename T> struct type_box
{
};

// A class with one base per type of the list: membership is one
// std::is_base_of, not a recursion over the list.
template <typename... T> struct type_set : type_box<T>...
{
};
} // namespace detail

/// @brief Whether `T` is one of the types of `List`.
template <typename List, typename T> struct list_contains;

/// @copydoc list_contains
template <typename... L, typename T>
struct list_contains<type_list<L...>, T>
    : std::is_base_of<detail::type_box<T>, detail::type_set<L...>>
{
};

/// @brief `concat<type_list<A...>, type_list<B...>, ...>::type` joins lists.
template <typename... Lists> struct concat;

/// @copydoc concat
template <> struct concat<>
{
  /// The empty list.
  typedef type_list<> type;
};

/// @copydoc concat
template <typename... A> struct concat<type_list<A...>>
{
  /// The only list.
  typedef type_list<A...> type;
};

/// @copydoc concat
template <typename... A, typename... B, typename... Rest>
struct concat<type_list<A...>, type_list<B...>, Rest...>
    : concat<type_list<A..., B...>, Rest...>
{
};

/**
 * @brief `append_unique<Base, Added>::type` is `Base` followed by the types
 * of `Added` that `Base` does not hold, in their order.
 * @details A protocol layer resolves the rows of its base in itself first
 * (an overridden row is then the layer's own type, at the base position)
 * and appends its own rows: the overrides are already there, the new rows
 * are added.
 */
template <typename Base, typename Added> struct append_unique;

/// @copydoc append_unique
template <typename... B, typename... N>
struct append_unique<type_list<B...>, type_list<N...>>
{
  /// The joined list.
  typedef typename concat<
      type_list<B...>,
      typename std::conditional<list_contains<type_list<B...>, N>::value,
                                type_list<>, type_list<N>>::type...>::type
      type;
};
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_PROTOCOL_DSL_TYPE_LIST_HPP
