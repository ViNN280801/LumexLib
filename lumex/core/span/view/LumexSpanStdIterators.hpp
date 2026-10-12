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
 * @file LumexSpanStdIterators.hpp
 * @brief `is_contiguous_iterator` for the iterator classes of the standard
 * libraries below C++20.
 * @details From C++20 the trait is `std::contiguous_iterator` and this header
 * adds nothing. Before, the language cannot tell the iterator of a
 * `std::vector` from the iterator of a `std::deque`, so the iterator classes
 * that wrap a raw pointer are named here, one library at a time:
 * - libstdc++: `__gnu_cxx::__normal_iterator<Pointer, Container>` (the
 *   iterators of `std::vector` and `std::basic_string`);
 * - libc++: `std::__wrap_iter<Iterator>` (`std::vector`, `std::basic_string`,
 *   and `std::array` and `std::basic_string_view` before the bounded
 *   iterators), `std::__bounded_iter<Iterator>` and
 *   `std::__static_bounded_iter<Iterator, Size>` (the hardened modes of
 *   libc++);
 * - the standard library of MSVC: `std::_Vector_iterator`,
 *   `std::_Vector_const_iterator`, `std::_String_iterator`,
 *   `std::_String_const_iterator`, `std::_Array_iterator` and
 *   `std::_Array_const_iterator`.
 * Each of them holds a pointer (or, for the bounded ones of libc++, an
 * iterator that is contiguous itself) and moves it by pointer arithmetic, so
 * `*it` and `*(it + n)` are `n` elements apart: the promise the trait makes.
 * A wrapper counts as contiguous exactly when what it wraps is. The iterators
 * of `std::vector<bool>`, `std::deque` and `std::list`, reverse iterators,
 * the iterators of the checked debug mode of libstdc++ (`_GLIBCXX_DEBUG`) and
 * any iterator class of the user are not named here and stay not
 * contiguous; a program names them by a specialization of
 * `is_contiguous_iterator`, which also overrides this header. The
 * pointer iterators of `std::array` and `std::basic_string_view` in
 * libstdc++, and the begin and end of a `std::valarray` and of a
 * `std::initializer_list`, are raw pointers and need nothing.
 * The names of the standard libraries are private to them; the part for MSVC
 * follows the declarations of its headers and is not compiled by the tests of
 * this library.
 */
#ifndef LUMEX_CORE_SPAN_VIEW_SPAN_STD_ITERATORS_HPP
#define LUMEX_CORE_SPAN_VIEW_SPAN_STD_ITERATORS_HPP

#include <iterator>
#include <type_traits>

// The iterator classes of the MSVC STL are declared in the header of their
// container (`_Vector_iterator` in <vector>, `_String_iterator` in <xstring>
// through <string>), not in <iterator>, so a translation unit that includes
// the span before <vector> would not find the names. Never compiled here.
#if defined(_MSVC_STL_VERSION)
#include <string>
#include <vector>
#endif

#if defined(_LIBCPP_VERSION) && defined(__has_include)
#if __has_include(<__iterator/wrap_iter.h>)
#include <__iterator/wrap_iter.h>
#endif
#if __has_include(<__iterator/bounded_iter.h>)
#include <__iterator/bounded_iter.h>
#define LUMEX_SPAN_HAS_LIBCPP_BOUNDED_ITER 1
#endif
#if __has_include(<__iterator/static_bounded_iter.h>)
#include <__iterator/static_bounded_iter.h>
#define LUMEX_SPAN_HAS_LIBCPP_STATIC_BOUNDED_ITER 1
#endif
#endif

#include "lumex/core/span/view/LumexSpanTraits.hpp"

#if !LUMEX_SPAN_HAS_CONCEPTS

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace span
{
namespace view
{
#if defined(__GLIBCXX__)
/**
 * @brief libstdc++: the iterator of `std::vector` and `std::basic_string`.
 */
template <typename Iterator, typename Container>
struct is_contiguous_iterator<
    __gnu_cxx::__normal_iterator<Iterator, Container>>
    : is_contiguous_iterator<Iterator>
{
};
#endif

#if defined(_LIBCPP_VERSION)
/**
 * @brief libc++: the iterator of `std::vector`, `std::basic_string` and
 * (before the bounded iterators) `std::array`.
 */
template <typename Iterator>
struct is_contiguous_iterator<std::__wrap_iter<Iterator>>
    : is_contiguous_iterator<Iterator>
{
};

#if defined(LUMEX_SPAN_HAS_LIBCPP_BOUNDED_ITER)
/**
 * @brief libc++ in a hardened mode: the bounded iterator.
 */
template <typename Iterator>
struct is_contiguous_iterator<std::__bounded_iter<Iterator>>
    : is_contiguous_iterator<Iterator>
{
};
#endif

#if defined(LUMEX_SPAN_HAS_LIBCPP_STATIC_BOUNDED_ITER)
/**
 * @brief libc++ in a hardened mode: the bounded iterator of `std::array`.
 */
template <typename Iterator, std::size_t Size>
struct is_contiguous_iterator<std::__static_bounded_iter<Iterator, Size>>
    : is_contiguous_iterator<Iterator>
{
};
#endif
#endif

#if defined(_MSVC_STL_VERSION)
template <typename Vector>
struct is_contiguous_iterator<std::_Vector_iterator<Vector>> : std::true_type
{
};

template <typename Vector>
struct is_contiguous_iterator<std::_Vector_const_iterator<Vector>>
    : std::true_type
{
};

template <typename String>
struct is_contiguous_iterator<std::_String_iterator<String>> : std::true_type
{
};

template <typename String>
struct is_contiguous_iterator<std::_String_const_iterator<String>>
    : std::true_type
{
};

#if _HAS_CXX17
// `std::string_view` (and so its iterator) exists from C++17.
template <typename Traits>
struct is_contiguous_iterator<std::_String_view_iterator<Traits>>
    : std::true_type
{
};
#endif // _HAS_CXX17

template <typename Element, std::size_t Size>
struct is_contiguous_iterator<std::_Array_iterator<Element, Size>>
    : std::true_type
{
};

template <typename Element, std::size_t Size>
struct is_contiguous_iterator<std::_Array_const_iterator<Element, Size>>
    : std::true_type
{
};
#endif
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

#endif // !LUMEX_SPAN_HAS_CONCEPTS

#endif // !LUMEX_CORE_SPAN_VIEW_SPAN_STD_ITERATORS_HPP
