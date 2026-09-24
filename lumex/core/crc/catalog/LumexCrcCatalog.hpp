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

#ifndef LUMEX_CORE_CRC_CATALOG_HPP
#define LUMEX_CORE_CRC_CATALOG_HPP

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

#include <cstddef>
#include <cstdint>
#include <vector>

#if __cplusplus >= 201703L
#include <string_view>
#endif
#if __cplusplus >= 202002L
#include <span>
#endif

#include "lumex/LumexExport.hpp"
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
CrcCatalogLegacyIndex () LUMEX_NOEXCEPT
{
  return 0xFFFFFFFFU; // NOLINT(*-magic-numbers)
}

/** @brief Value of @ref GetTransportCrcCatalogIndex when the mode is @c
 * Custom. */
inline std::uint32_t
CrcTransportUsesCustomSpecSentinel () LUMEX_NOEXCEPT
{
  return 0xFFFFFFFEU; // NOLINT(*-magic-numbers)
}

/**
 * @brief Validates parameters before computation (width 1..64).
 */
LUMEX_PUBLIC_API bool
ValidateCrcRevEngParams (crc_params_t const &params) LUMEX_NOEXCEPT;

/**
 * @brief Computes a CRC from explicit RevEng parameters (bit engine,
 * width 1..64).
 * @return 0 for nullptr, zero length, or invalid @p params.
 */
LUMEX_ATTRIBUTE_NODISCARD ("CRC result is required for integrity checks.")
LUMEX_PUBLIC_API std::uint64_t
ComputeCrcWithRevEngParams (crc_params_t const &params,
                            std::uint8_t const *data,
                            std::size_t byteCount) LUMEX_NOEXCEPT;

/**
 * @brief CRC for catalogue index @c all_crc_specs_t (0 ... @ref
 * GetCrcCatalogEntryCount - 1).
 */
LUMEX_ATTRIBUTE_NODISCARD ("CRC result is required for integrity checks.")
LUMEX_PUBLIC_API std::uint64_t
ComputeCrcCatalog (std::uint32_t catalogIndex, std::uint8_t const *data,
                   std::size_t byteCount) LUMEX_NOEXCEPT;

/**
 * @brief Number of catalogue algorithms (order matches @c all_crc_specs_t).
 */
LUMEX_ATTRIBUTE_NODISCARD ("Caller may need to validate catalog indices.")
LUMEX_PUBLIC_API std::uint32_t GetCrcCatalogEntryCount () LUMEX_NOEXCEPT;

/**
 * @brief Catalogue entry width in bits; -1 on error.
 * @note @ref CrcCatalogLegacyIndex returns 8 (built-in transport CRC-8).
 */
LUMEX_PUBLIC_API int
GetCrcCatalogBitWidth (std::uint32_t catalogIndex) LUMEX_NOEXCEPT;

// ---------- Transport (process-global; thread-safe) ----------

/** @brief Restore the default CRC-8/MAXIM-DOW transport mode. */
LUMEX_PUBLIC_API void SetTransportCrcDefault () LUMEX_NOEXCEPT;

/**
 * @brief Transport: 8-bit catalogue entry by index.
 * @return false if the index is out of range or the width is not 8 bits.
 */
LUMEX_PUBLIC_API bool
SetTransportCrcCatalogIndex (std::uint32_t catalogIndex) LUMEX_NOEXCEPT;

/**
 * @brief Transport: caller-defined 8-bit CRC (must match the peer).
 * @return false if @c widthBits != 8 or the parameters are invalid.
 */
LUMEX_PUBLIC_API bool
SetTransportCrcRevEngParams (crc_params_t const &params) LUMEX_NOEXCEPT;

LUMEX_ATTRIBUTE_NODISCARD (
    "Caller may need transport CRC mode for logging or tests.")
LUMEX_PUBLIC_API TransportCrcMode GetTransportCrcMode () LUMEX_NOEXCEPT;

/**
 * @brief Catalogue index in @c Catalog mode; otherwise @ref
 * CrcCatalogLegacyIndex or
 *        @ref CrcTransportUsesCustomSpecSentinel.
 */
LUMEX_PUBLIC_API std::uint32_t GetTransportCrcCatalogIndex () LUMEX_NOEXCEPT;

/** @brief Writes @p out in @c Custom mode; otherwise leaves @p out unchanged.
 * @return true if Custom. */
LUMEX_PUBLIC_API bool
TryGetTransportCrcRevEngParams (crc_params_t &out) LUMEX_NOEXCEPT;

/**
 * @brief 8-bit transport checksum for the current process-global mode.
 */
LUMEX_ATTRIBUTE_NODISCARD ("Checksum is required for transport framing.")
LUMEX_PUBLIC_API std::uint8_t
ComputeTransportChecksum (std::uint8_t const *data,
                          std::size_t byteCount) LUMEX_NOEXCEPT;

// ---------- Convenience overloads ----------

/**
 * @brief Catalogue CRC of a byte vector. Available from C++11 so C++14
 *        examples and tests can pass `std::vector` without a pointer+size
 *        pair. Empty vector is the same as a zero-length buffer.
 */
inline std::uint64_t
ComputeCrcCatalog (std::uint32_t catalogIndex,
                   std::vector<std::uint8_t> const &bytes) LUMEX_NOEXCEPT
{
  return ComputeCrcCatalog (catalogIndex, bytes.data (), bytes.size ());
}

#if __cplusplus >= 201703L
/** @brief Catalogue CRC of an ASCII/UTF-8 string (no trailing '\\0'). */
inline std::uint64_t
ComputeCrcCatalog (std::uint32_t catalogIndex,
                   std::string_view text) LUMEX_NOEXCEPT
{
  if (text.empty ())
    return 0;
  return ComputeCrcCatalog (
      catalogIndex, reinterpret_cast<std::uint8_t const *> (text.data ()),
      text.size ());
}

/** @brief RevEng-parameter CRC of a string (raw bytes, no trailing '\\0'). */
inline std::uint64_t
ComputeCrcWithRevEngParams (crc_params_t const &params,
                            std::string_view text) LUMEX_NOEXCEPT
{
  if (text.empty ())
    return 0;
  return ComputeCrcWithRevEngParams (
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
AppendCrcLeastSignificantByteFirst (crc_params_t const &params,
                                    std::vector<std::uint8_t> &buffer)
    LUMEX_NOEXCEPT
{
  std::uint64_t const value
      = ComputeCrcWithRevEngParams (params, buffer.data (), buffer.size ());
  int const numBytes = (params.widthBits + 7) / 8;
  for (int i = 0; i < numBytes; ++i)
    buffer.push_back (static_cast<std::uint8_t> ((value >> (8 * i)) & 0xFFU));
}
#endif

#if __cplusplus >= 202002L
inline std::uint64_t
ComputeCrcCatalog (std::uint32_t catalogIndex,
                   std::span<std::uint8_t const> bytes) LUMEX_NOEXCEPT
{
  return ComputeCrcCatalog (catalogIndex, bytes.data (), bytes.size ());
}

inline std::uint64_t
ComputeCrcWithRevEngParams (crc_params_t const &params,
                            std::span<std::uint8_t const> bytes) LUMEX_NOEXCEPT
{
  return ComputeCrcWithRevEngParams (params, bytes.data (), bytes.size ());
}
#endif

} // namespace catalog
} // namespace crc
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_CRC_CATALOG_HPP
