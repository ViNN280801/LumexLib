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
 * @details It returns `std::nullopt` for a null pointer or a buffer shorter
 * than `sizeof(T)`. The overloads take a pointer and a size, a `std::span` or
 * a `lumex::core::span::view::span` of `std::byte`, `char` or `unsigned char`,
 * or an object with `get_data()` and `get_data_size()`. The value type must
 * satisfy `Extractible` of `LumexTypeTraits.hpp`: trivially copyable, standard
 * layout, and neither a pointer nor a reference.
 * @warning Requires C++20 (concepts and `<span>`); with an older standard the
 * header declares nothing.
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
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<concepts>)
#include <concepts>
#endif
#endif
#include <cstddef> // std::byte, std::size_t
#include <cstdint> // for std::uintptr_t
#include <cstring>
#if __cplusplus >= 201703L
#include <optional>
#endif
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif
#include <type_traits>

// The header declares nothing before C++20, so the span of this library is
// not parsed for the translation units of the older standards.
#if __cplusplus > 201703L
#include "lumex/core/span/LumexSpan"
#endif
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// Needs C++20 <concepts> and <span>; without them the header declares
// nothing.
#if LUMEX_HAS_STD_CONCEPTS && LUMEX_HAS_STD_SPAN

namespace lumex
{
namespace core
{
namespace utility
{
namespace mem
{
namespace Detail
{
template <typename TSource>
concept DataSource = requires (TSource const &source) {
  { source.get_data () } -> std::convertible_to<void const *>;
  { source.get_data_size () } -> std::convertible_to<int>;
};

} // namespace Detail

/**
 * @brief Safely reinterprets a raw memory block as a value of type T.
 * @tparam T Trivially-copyable, standard-layout, non-pointer, non-reference
 * type.
 * @param data Pointer to the source bytes (may be unaligned; memcpy handles
 * that correctly).
 * @param size Number of bytes available at `data`.
 * @return `T` decoded from the first `sizeof(T)` bytes, or `std::nullopt` if
 * `data` is null or `size < sizeof(T)`.
 * @note Unaligned pointers are expected for binary protocols coming from
 * external devices/wire formats, so no alignment assertion is performed -
 * memcpy handles this correctly.
 */
template <traits::meta::Extractible T>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
std::optional<T> as (void const *data, std::size_t size) LUMEX_NOEXCEPT
{
  if ((data == nullptr) || (size < sizeof (T)))
    return std::nullopt;

  T res{};
  std::memcpy (std::addressof (res), data, sizeof (T));
  return res;
}

/**
 * @brief Overload of as() that reads from a source object exposing
 * get_data()/GetDataSize().
 * @tparam TSource Type satisfying the DataSource concept (get_data() -> const
 * void*, get_data_size() -> int).
 */
template <traits::meta::Extractible T, Detail::DataSource TSource>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
std::optional<T> as (TSource const &source) LUMEX_NOEXCEPT
{
  int const rawSize{ source.get_data_size () };
  if (rawSize < 0)
    return std::nullopt;
  return as<T> (source.get_data (), static_cast<std::size_t> (rawSize));
}

/**
 * @brief Overload of as() that reads from a std::span of byte-like elements.
 */
template <traits::meta::Extractible T, traits::meta::ByteLike ByteType>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
std::optional<T> as (std::span<ByteType const> span) LUMEX_NOEXCEPT
{
  return as<T> (span.data (), span.size ());
}

/**
 * @brief Overload of as() that reads from a `span` of this library (the
 * `lumex::core::span` module) of byte-like elements.
 * @details A template deduces the element type from the exact type, so a
 * `std::span` takes the overload above and a `std::vector` or a `std::array`
 * takes neither: pass its `data ()` and `size ()`, or make a `span` first.
 */
template <traits::meta::Extractible T, traits::meta::ByteLike ByteType>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
std::optional<T> as (core::span::view::span<ByteType const> span)
    LUMEX_NOEXCEPT
{
  return as<T> (span.data (), span.size ());
}
} // namespace mem
} // namespace utility
} // namespace core
} // namespace lumex

#endif // LUMEX_HAS_STD_CONCEPTS && ...

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_MEM_HPP
