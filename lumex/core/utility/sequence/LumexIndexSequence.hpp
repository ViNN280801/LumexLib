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
 * @file LumexIndexSequence.hpp
 * @brief `integer_sequence`, `index_sequence`, `make_integer_sequence`,
 * `make_index_sequence` and `index_sequence_for`, the compile-time lists of
 * integers that the standard library has since C++14.
 * @details `integer_sequence` is a class template of this library with the
 * members of the standard one (`value_type`, `size ()`), and the other four
 * names are alias templates over it, in every standard from C++11. They are
 * never aliases of the `std::` ones: a function that takes
 * `index_sequence<I...>` deduces the indices of a list made by
 * `make_index_sequence`, and the two families do not mix (a function of the
 * standard library that takes `std::index_sequence<I...>` does not take this
 * one; there is no value to convert, only a list of numbers in the type, so
 * pass a `std::` list made by `std::make_index_sequence` there).
 * `make_integer_sequence` builds the list by halving, so its depth is
 * logarithmic in the count, and refuses a count above 16384 (the alias has no
 * type then). The header is C++11 and declares nothing at global scope.
 */
#ifndef LUMEX_CORE_UTILITY_SEQUENCE_HPP
#define LUMEX_CORE_UTILITY_SEQUENCE_HPP

#include <cstddef>
#include <utility>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace sequence
{
/**
 * @brief The compile-time list of the integers `Values...` of type `T`
 * (the counterpart of `std::integer_sequence`, C++14).
 * @tparam T An integer type.
 * @tparam Values The integers.
 */
template <typename T, T... Values> struct integer_sequence
{
  using value_type = T;

  /** @brief The number of integers in the list. */
  static LUMEX_CONSTEXPR std::size_t
  size () LUMEX_NOEXCEPT
  {
    return sizeof...(Values);
  }

#if __cplusplus >= 201402L
  /** @brief The empty object; trivial, as the standard one. */
  integer_sequence () = default;

  /**
   * @brief Implicit construction from the `std::integer_sequence` (C++14) of
   * the same list. The objects are empty; the list is in the type.
   */
  LUMEX_CONSTEXPR_CTOR
  integer_sequence (std::integer_sequence<T, Values...> /*unused*/)
      LUMEX_NOEXCEPT
  {
  }

  /**
   * @brief Implicit conversion to the `std::integer_sequence` (C++14) of the
   * same list, for the places that name the standard type. A function
   * template of the standard library that deduces `std::index_sequence<I...>`
   * from its argument does not deduce through a conversion: such a call needs
   * `std::make_index_sequence` itself.
   */
  LUMEX_CONSTEXPR
  operator std::integer_sequence<T, Values...> () const LUMEX_NOEXCEPT
  {
    return std::integer_sequence<T, Values...> ();
  }
#endif
};

/**
 * @brief A list of `std::size_t` indices (the counterpart of
 * `std::index_sequence`, C++14).
 */
template <std::size_t... Indices>
using index_sequence = integer_sequence<std::size_t, Indices...>;

namespace detail
{
/**
 * @brief The most integers one own sequence holds. A longer one is refused
 * (see `make_integer_sequence`); the standard library has no limit of its
 * own, only the resources of the compiler. The value stays well below 32768:
 * Clang 23 builds a list of 32768 or 65536 integers as an empty one without a
 * diagnostic (32767 is still right).
 */
LUMEX_CONSTEXPR std::size_t k_max_sequence_size = 16384;

/**
 * @brief Joins two lists, the integers of the second moved up by the length
 * of the first.
 */
template <typename Left, typename Right> struct sequence_concat;

template <typename T, T... Left, T... Right>
struct sequence_concat<integer_sequence<T, Left...>,
                       integer_sequence<T, Right...>>
{
  using type = integer_sequence<
      T, Left...,
      static_cast<T> (sizeof...(Left) + static_cast<std::size_t> (Right))...>;
};

/**
 * @brief `type` is the list `0 .. Count - 1` of type `T`, built by halving, so
 * the depth is `log2 (Count)`. No `type` for a count above
 * `k_max_sequence_size`.
 */
template <typename T, std::size_t Count,
          bool Buildable = (Count <= k_max_sequence_size)>
struct sequence_builder
{
};

template <typename T, std::size_t Count>
struct sequence_builder<T, Count, true>
    : sequence_concat<typename sequence_builder<T, Count / 2>::type,
                      typename sequence_builder<T, Count - Count / 2>::type>
{
};

template <typename T> struct sequence_builder<T, 0, true>
{
  using type = integer_sequence<T>;
};

template <typename T> struct sequence_builder<T, 1, true>
{
  using type = integer_sequence<T, static_cast<T> (0)>;
};
} // namespace detail

/**
 * @brief The list `0, 1, ..., Count - 1` of type `T`
 * (the counterpart of `std::make_integer_sequence`, C++14).
 * @details A negative count and a count above 16384 are refused: the alias
 * has no type then, so `make_index_sequence<N - 1>` with `N == 0` is an error
 * at once instead of a compiler running out of memory on a list of 2^64
 * integers (a negative count converts to a huge unsigned one).
 * @tparam T An integer type.
 * @tparam Count The length.
 */
template <typename T, T Count>
using make_integer_sequence =
    typename detail::sequence_builder<T,
                                      static_cast<std::size_t> (Count)>::type;

/**
 * @brief The list of indices `0, 1, ..., Count - 1`
 * (the counterpart of `std::make_index_sequence`, C++14).
 * @tparam Count The length, at most 16384.
 */
template <std::size_t Count>
using make_index_sequence = make_integer_sequence<std::size_t, Count>;

/**
 * @brief The list of indices of a pack, `0 .. sizeof... (Types) - 1`
 * (the counterpart of `std::index_sequence_for`, C++14).
 * @tparam Types The pack.
 */
template <typename... Types>
using index_sequence_for = make_index_sequence<sizeof...(Types)>;
} // namespace sequence
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_SEQUENCE_HPP
