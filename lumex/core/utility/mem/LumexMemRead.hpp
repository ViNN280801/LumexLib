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
 * @file LumexMemRead.hpp
 * @brief `as<T>()`, which reads a trivially copyable value out of a raw byte
 * buffer through `std::memcpy`, so unaligned data is read without undefined
 * behavior.
 * @details It works from C++11. It returns an empty optional for a null
 * pointer or a buffer shorter than `sizeof(T)`. The result is always
 * `lumex::core::optional::opt::optional<T>` of the optional module
 * (`optional_t<T>` names it), whatever the standard; from C++17 it converts
 * implicitly to and from `std::optional<T>`, so `std::optional<int> value
 * = mem::as<int> (data, size);` compiles. The header does not put `optional`
 * or `nullopt` at global scope, test the result with `has_value ()`. The
 * overloads take a pointer and a size, a `lumex::core::span::view::span` of
 * `char`, `unsigned char`, `std::byte` (C++17) or the `byte` of the span
 * module, a `std::span` of the same element types (C++20), or an object with
 * `get_data()` and `get_data_size()`. The value type must
 * satisfy `traits::meta::is_extractible` of `LumexTypeTraits.hpp`: trivially
 * copyable, standard layout, neither a pointer nor a reference, and neither
 * `const` nor `volatile` (the bytes are copied into a local object of `T`, so
 * `as<int const>` finds no overload instead of failing inside the function);
 * any other type finds no overload. The constraints are the same SFINAE form
 * in every standard (the concepts `Extractible` and `ByteLike` of C++20 stay
 * in `LumexTypeTraits.hpp` for their other users).
 */
#ifndef LUMEX_CORE_UTILITY_MEM_HPP
#define LUMEX_CORE_UTILITY_MEM_HPP

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

#include <cassert>
#include <cstddef> // std::size_t
#include <cstdint> // for std::uintptr_t
#include <cstring>
#include <memory> // std::addressof
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif
#include <type_traits>
#include <utility> // std::declval

// The result is the optional of this library in every standard; its types
// header declares nothing at global scope (LumexOptional is the umbrella with
// the global aliases).
#include "lumex/core/optional/opt/LumexOptional.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace mem
{
/**
 * @brief The result of `as`: `lumex::core::optional::opt::optional<T>` in
 * every standard (not an alias of `std::optional`; it converts to and from
 * `std::optional<T>` from C++17).
 */
template <typename T>
using optional_t = lumex::core::optional::opt::optional<T>;

namespace Detail
{
/**
 * @brief `TSource` has `get_data ()` convertible to `void const *` and
 * `get_data_size ()` convertible to `int`, both callable on a const object.
 */
template <typename TSource, typename = void>
struct is_data_source : std::false_type
{
};

template <typename TSource>
struct is_data_source<
    TSource, traits::meta::void_t<
                 decltype (std::declval<TSource const &> ().get_data ()),
                 decltype (std::declval<TSource const &> ().get_data_size ())>>
    : std::integral_constant<
          bool,
          std::is_convertible<
              decltype (std::declval<TSource const &> ().get_data ()),
              void const *>::value
              && std::is_convertible<
                  decltype (std::declval<TSource const &> ().get_data_size ()),
                  int>::value>
{
};

/**
 * @brief An element type of a byte span that `as` accepts: `char`,
 * `unsigned char`, `std::byte` (C++17) or the `byte` of the span module (an
 * own enumeration in every standard).
 */
template <typename ByteType>
struct is_byte_element
    : std::integral_constant<
          bool, traits::meta::is_byte_like<ByteType>::value
                    || std::is_same<ByteType, core::span::view::byte>::value>
{
};
} // namespace Detail

/**
 * @brief Safely reinterprets a raw memory block as a value of type T.
 * @tparam T Trivially-copyable, standard-layout, non-pointer, non-reference,
 * non-cv-qualified type.
 * @param data Pointer to the source bytes (may be unaligned; memcpy handles
 * that correctly).
 * @param size Number of bytes available at `data`.
 * @return `T` decoded from the first `sizeof(T)` bytes, or an empty
 * `optional_t<T>` if `data` is null or `size < sizeof(T)`.
 * @note Unaligned pointers are expected for binary protocols coming from
 * external devices/wire formats, so no alignment assertion is performed -
 * memcpy handles this correctly.
 */
template <typename T, typename std::enable_if<
                          traits::meta::is_extractible<T>::value, int>::type
                      = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
optional_t<T> as (void const *data, std::size_t size) LUMEX_NOEXCEPT
{
  if ((data == nullptr) || (size < sizeof (T)))
    return optional_t<T> ();

  T res{};
  std::memcpy (std::addressof (res), data, sizeof (T));
  return optional_t<T> (res);
}

/**
 * @brief Overload of as() that reads from a source object exposing
 * get_data()/get_data_size().
 * @tparam TSource Type with `get_data ()` convertible to `void const *` and
 * `get_data_size ()` convertible to `int` (a negative size gives an empty
 * result).
 */
template <
    typename T, typename TSource,
    typename std::enable_if<traits::meta::is_extractible<T>::value
                                && Detail::is_data_source<TSource>::value,
                            int>::type
    = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
optional_t<T> as (TSource const &source) LUMEX_NOEXCEPT
{
  int const rawSize{ source.get_data_size () };
  if (rawSize < 0)
    return optional_t<T> ();
  return as<T> (source.get_data (), static_cast<std::size_t> (rawSize));
}

/**
 * @brief Overload of as() that reads from a `span` of this library (the
 * `lumex::core::span` module) of byte-like elements (`char`,
 * `unsigned char`, `std::byte` or the `byte` of the span module).
 * @details A template deduces the element type from the exact type, so a
 * `std::span` takes the overload below and a `std::vector` or a `std::array`
 * takes neither: pass its `data ()` and `size ()`, or make a `span` first.
 */
template <
    typename T, typename ByteType,
    typename std::enable_if<traits::meta::is_extractible<T>::value
                                && Detail::is_byte_element<ByteType>::value,
                            int>::type
    = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
optional_t<T> as (core::span::view::span<ByteType const> span) LUMEX_NOEXCEPT
{
  return as<T> (span.data (), span.size ());
}

#if LUMEX_HAS_STD_SPAN
/**
 * @brief Overload of as() that reads from a `std::span` (C++20) of byte-like
 * elements.
 */
template <
    typename T, typename ByteType,
    typename std::enable_if<traits::meta::is_extractible<T>::value
                                && Detail::is_byte_element<ByteType>::value,
                            int>::type
    = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
optional_t<T> as (std::span<ByteType const> span) LUMEX_NOEXCEPT
{
  return as<T> (span.data (), span.size ());
}
#endif
} // namespace mem
} // namespace utility
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_MEM_HPP
