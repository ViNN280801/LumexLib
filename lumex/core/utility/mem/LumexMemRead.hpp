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

/// @warning Requires C++20 (concepts, <span>).

#ifndef LUMEX_CORE_UTILITY_MEM_HPP
#define LUMEX_CORE_UTILITY_MEM_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <cassert>
#include <concepts>
#include <cstddef> // std::byte, std::size_t
#include <cstdint> // for std::uintptr_t
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
template <typename T>
concept Extractible
    = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
      && !std::is_pointer_v<T> && !std::is_reference_v<T>;

template <typename TSource>
concept DataSource = requires (TSource const &source) {
  { source.GetData () } -> std::convertible_to<void const *>;
  { source.GetDataSize () } -> std::convertible_to<int>;
};

template <typename T>
concept ByteLike = std::same_as<T, std::byte> || std::same_as<T, char>
                   || std::same_as<T, unsigned char>;
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
template <Detail::Extractible T>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
std::optional<T> As (void const *data, std::size_t size) LUMEX_NOEXCEPT
{
  if ((data == nullptr) || (size < sizeof (T)))
    return std::nullopt;

  T res{};
  std::memcpy (std::addressof (res), data, sizeof (T));
  return res;
}

/**
 * @brief Overload of As() that reads from a source object exposing
 * GetData()/GetDataSize().
 * @tparam TSource Type satisfying the DataSource concept (GetData() -> const
 * void*, GetDataSize() -> int).
 */
template <Detail::Extractible T, Detail::DataSource TSource>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
std::optional<T> As (TSource const &source) LUMEX_NOEXCEPT
{
  int const rawSize{ source.GetDataSize () };
  if (rawSize < 0)
    return std::nullopt;
  return As<T> (source.GetData (), static_cast<std::size_t> (rawSize));
}

/**
 * @brief Overload of As() that reads from a std::span of byte-like elements.
 */
template <Detail::Extractible T, Detail::ByteLike ByteType>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
std::optional<T> As (std::span<ByteType const> span) LUMEX_NOEXCEPT
{
  return As<T> (span.data (), span.size ());
}
} // namespace mem
} // namespace utility
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_MEM_HPP
