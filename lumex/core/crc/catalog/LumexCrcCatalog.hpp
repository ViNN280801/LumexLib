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
 * @file LumexCrcCatalog.hpp
 * @brief Run-time CRC computation by catalogue index or by explicit
 * parameters, and the process-wide 8-bit transport checksum.
 * @details The functions are compiled into the `lumex::crc` library and take
 * parameters known only at run time: `compute_crc_catalog()` takes an index
 * into `all_crc_specs_t` of `LumexCrcParametric.hpp` (112 algorithms ordered
 * by width, so index 0 is CRC-3/GSM), and `compute_crc_with_rev_eng_params()`
 * takes a `crc_params_t` in CRC RevEng notation (width 1 to 64). The transport
 * functions keep one process-global, mutex-protected setting that
 * `compute_transport_checksum()` uses for a one-byte frame checksum:
 * CRC-8/MAXIM-DOW by default, an 8-bit catalogue entry, or caller-supplied
 * 8-bit parameters. The overloads for `std::vector` and for
 * `lumex::core::span::view::span` (every standard; a `std::span` converts to
 * it) and for `std::string_view` (C++17) are inline wrappers over the pointer
 * and size functions.
 */
#ifndef LUMEX_CORE_CRC_CATALOG_HPP
#define LUMEX_CORE_CRC_CATALOG_HPP

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

#include "lumex/LumexExport.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

#if __cplusplus >= 201703L
#include <string_view>
#endif

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace crc
{
namespace catalog
{
/**
 * @brief CRC parameters in CRC RevEng notation (see LumexCrcParametric.hpp /
 * Greg Cook catalogue).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm
 */
struct crc_params_t
{                  // NOLINT(altera-struct-pack-align)
  int widthBits{}; ///< CRC width in bits (1..64).
  std::uint64_t
      poly{}; ///< Polynomial without the implicit high bit (catalogue form).
  std::uint64_t init{};   ///< Initial register value.
  bool refIn{};           ///< Reflect input bytes (RefIn).
  bool refOut{};          ///< Reflect output before xorOut (RefOut).
  std::uint64_t xorOut{}; ///< Final XOR.
};

/**
 * @brief Process-global 8-bit transport checksum mode (one byte per frame).
 */
enum class TransportCrcMode : std::uint8_t
{
  Default = 0, ///< CRC-8/MAXIM-DOW (`Crc8MaximDow`).
  Catalog
  = 1, ///< Catalogue entry from @c all_crc_specs_t with @c widthBits == 8.
  Custom = 2 ///< Caller-supplied @ref crc_params_t; @c widthBits must be 8.
};

// --- Sentinel values for the process-global transport mode ---

inline std::uint32_t
crc_catalog_legacy_index () LUMEX_NOEXCEPT
{
  return 0xFFFFFFFFU; // NOLINT(*-magic-numbers)
}

/** @brief Value of @ref get_transport_crc_catalog_index when the mode is @c
 * Custom. */
inline std::uint32_t
crc_transport_uses_custom_spec_sentinel () LUMEX_NOEXCEPT
{
  return 0xFFFFFFFEU; // NOLINT(*-magic-numbers)
}

/**
 * @brief Validates parameters before computation (width 1..64).
 */
LUMEX_PUBLIC_API bool
validate_crc_rev_eng_params (crc_params_t const &params) LUMEX_NOEXCEPT;

/**
 * @brief Computes a CRC from explicit RevEng parameters (bit engine,
 * width 1..64).
 * @return 0 for nullptr, zero length, or invalid @p params.
 */
LUMEX_ATTRIBUTE_NODISCARD ("CRC result is required for integrity checks.")
LUMEX_PUBLIC_API std::uint64_t
compute_crc_with_rev_eng_params (crc_params_t const &params,
                                 std::uint8_t const *data,
                                 std::size_t byteCount) LUMEX_NOEXCEPT;

/**
 * @brief CRC for catalogue index @c all_crc_specs_t (0 ... @ref
 * get_crc_catalog_entry_count - 1).
 */
LUMEX_ATTRIBUTE_NODISCARD ("CRC result is required for integrity checks.")
LUMEX_PUBLIC_API std::uint64_t
compute_crc_catalog (std::uint32_t catalogIndex, std::uint8_t const *data,
                     std::size_t byteCount) LUMEX_NOEXCEPT;

/**
 * @brief Number of catalogue algorithms (order matches @c all_crc_specs_t).
 */
LUMEX_ATTRIBUTE_NODISCARD ("Caller may need to validate catalog indices.")
LUMEX_PUBLIC_API std::uint32_t get_crc_catalog_entry_count () LUMEX_NOEXCEPT;

/**
 * @brief Catalogue entry width in bits; -1 on error.
 * @note @ref crc_catalog_legacy_index returns 8 (built-in transport CRC-8).
 */
LUMEX_PUBLIC_API int
get_crc_catalog_bit_width (std::uint32_t catalogIndex) LUMEX_NOEXCEPT;

// ---------- transport (process-global; thread-safe) ----------

/** @brief Restore the default CRC-8/MAXIM-DOW transport mode. */
LUMEX_PUBLIC_API void set_transport_crc_default () LUMEX_NOEXCEPT;

/**
 * @brief Transport: 8-bit catalogue entry by index.
 * @return false if the index is out of range or the width is not 8 bits.
 */
LUMEX_PUBLIC_API bool
set_transport_crc_catalog_index (std::uint32_t catalogIndex) LUMEX_NOEXCEPT;

/**
 * @brief Transport: caller-defined 8-bit CRC (must match the peer).
 * @return false if @c widthBits != 8 or the parameters are invalid.
 */
LUMEX_PUBLIC_API bool
set_transport_crc_rev_eng_params (crc_params_t const &params) LUMEX_NOEXCEPT;

LUMEX_ATTRIBUTE_NODISCARD (
    "Caller may need transport CRC mode for logging or tests.")
LUMEX_PUBLIC_API TransportCrcMode get_transport_crc_mode () LUMEX_NOEXCEPT;

/**
 * @brief Catalogue index in @c Catalog mode; otherwise @ref
 * crc_catalog_legacy_index or
 *        @ref crc_transport_uses_custom_spec_sentinel.
 */
LUMEX_PUBLIC_API std::uint32_t
get_transport_crc_catalog_index () LUMEX_NOEXCEPT;

/** @brief Writes @p out in @c Custom mode; otherwise leaves @p out unchanged.
 * @return true if Custom. */
LUMEX_PUBLIC_API bool
try_get_transport_crc_rev_eng_params (crc_params_t &out) LUMEX_NOEXCEPT;

/**
 * @brief 8-bit transport checksum for the current process-global mode.
 */
LUMEX_ATTRIBUTE_NODISCARD ("Checksum is required for transport framing.")
LUMEX_PUBLIC_API std::uint8_t
compute_transport_checksum (std::uint8_t const *data,
                            std::size_t byteCount) LUMEX_NOEXCEPT;

// ---------- Convenience overloads ----------

/**
 * @brief Catalogue CRC of a byte vector. Available from C++11 so C++14
 *        examples and tests can pass `std::vector` without a pointer+size
 *        pair. Empty vector is the same as a zero-length buffer.
 */
inline std::uint64_t
compute_crc_catalog (std::uint32_t catalogIndex,
                     std::vector<std::uint8_t> const &bytes) LUMEX_NOEXCEPT
{
  return compute_crc_catalog (catalogIndex, bytes.data (), bytes.size ());
}

#if __cplusplus >= 201703L
/** @brief Catalogue CRC of an ASCII/UTF-8 string (no trailing '\\0'). */
inline std::uint64_t
compute_crc_catalog (std::uint32_t catalogIndex,
                     std::string_view text) LUMEX_NOEXCEPT
{
  if (text.empty ())
    return 0;
  return compute_crc_catalog (
      catalogIndex, reinterpret_cast<std::uint8_t const *> (text.data ()),
      text.size ());
}

/** @brief RevEng-parameter CRC of a string (raw bytes, no trailing '\\0'). */
inline std::uint64_t
compute_crc_with_rev_eng_params (crc_params_t const &params,
                                 std::string_view text) LUMEX_NOEXCEPT
{
  if (text.empty ())
    return 0;
  return compute_crc_with_rev_eng_params (
      params, reinterpret_cast<std::uint8_t const *> (text.data ()),
      text.size ());
}

/**
 * @brief Appends the least-significant CRC bytes to @p buffer (LE, first byte
 * = low 8 bits).
 * @details Useful when assembling a frame by hand: payload first, then CRC.
 *          Number of appended bytes = @c (widthBits + 7) / 8 .
 */
inline void
append_crc_least_significant_byte_first (crc_params_t const &params,
                                         std::vector<std::uint8_t> &buffer)
    LUMEX_NOEXCEPT
{
  std::uint64_t const value = compute_crc_with_rev_eng_params (
      params, buffer.data (), buffer.size ());
  int const numBytes = (params.widthBits + 7) / 8;
  for (int i = 0; i < numBytes; ++i)
    buffer.push_back (static_cast<std::uint8_t> ((value >> (8 * i)) & 0xFFU));
}
#endif

/**
 * @brief Catalogue CRC of a `span` of bytes. Available from C++11; a
 *        `std::span` (C++20), a `std::array` and a built-in array convert to
 *        it, a `std::vector` takes the overload above.
 */
inline std::uint64_t
compute_crc_catalog (std::uint32_t catalogIndex,
                     lumex::core::span::view::span<std::uint8_t const> bytes)
    LUMEX_NOEXCEPT
{
  return compute_crc_catalog (catalogIndex, bytes.data (), bytes.size ());
}

/**
 * @brief RevEng-parameter CRC of a `span` of bytes. Available from C++11, see
 *        `compute_crc_catalog` above for what converts to the `span`.
 */
inline std::uint64_t
compute_crc_with_rev_eng_params (
    crc_params_t const &params,
    lumex::core::span::view::span<std::uint8_t const> bytes) LUMEX_NOEXCEPT
{
  return compute_crc_with_rev_eng_params (params, bytes.data (),
                                          bytes.size ());
}

} // namespace catalog
} // namespace crc
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_CRC_CATALOG_HPP
