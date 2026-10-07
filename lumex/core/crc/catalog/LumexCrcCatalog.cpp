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

#define LUMEX_IMPLEMENTATION
#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <tuple>
#include <utility>

#include "lumex/core/crc/catalog/LumexCrcCatalog.hpp"
#include "lumex/core/crc/parametric/LumexCrcParametric.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

namespace lumex
{
namespace core
{
namespace crc
{
namespace catalog
{
using namespace lumex::core::crc::parametric;

namespace
{
using CatalogTuple = all_crc_specs_t;
LUMEX_CONST_NUM std::size_t kCatalogSize
    = std::tuple_size<CatalogTuple>::value;

LUMEX_CONST_NUM int kTransportFrameCrcBitWidth
    = 8; ///< One checksum byte per transport frame
LUMEX_CONST_NUM int kBitsPerByte = 8;

using ComputeFn = std::uint64_t (*) (std::uint8_t const *, std::size_t);

template <std::size_t I>
std::uint64_t
compute_entry (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  using Spec = typename std::tuple_element<I, CatalogTuple>::type;
  return static_cast<std::uint64_t> (
      crc_parametric<Spec>::calculate (data, size));
}

template <std::size_t... I>
std::array<ComputeFn, sizeof...(I)>
make_compute_table (std::index_sequence<I...> /*unusedIndexSequence*/)
    LUMEX_NOEXCEPT
{
  return std::array<ComputeFn, sizeof...(I)>{ { &compute_entry<I>... } };
}

#if __cplusplus >= 201402L
using CatalogIndexSequence = std::make_index_sequence<kCatalogSize>;
#else
using CatalogIndexSequence =
    typename std::make_index_sequence<kCatalogSize>::type;
#endif

std::array<ComputeFn, kCatalogSize> const kComputeTable
    = make_compute_table (CatalogIndexSequence{});

template <std::size_t I>
LUMEX_CONSTEXPR_FUNCTION int
width_entry () LUMEX_NOEXCEPT
{
  using Spec = typename std::tuple_element<I, CatalogTuple>::type;
  return Spec::kWidth;
}

template <std::size_t... I>
LUMEX_CONSTEXPR_FUNCTION std::array<int, sizeof...(I)>
make_width_table (std::index_sequence<I...> /*unusedIndexSequence*/)
    LUMEX_NOEXCEPT
{
  return std::array<int, sizeof...(I)>{ { width_entry<I> ()... } };
}

LUMEX_CONST_NUM std::array<int, kCatalogSize> kWidthTable
    = make_width_table (CatalogIndexSequence{});

std::uint64_t
mask_for_width (int widthBits) LUMEX_NOEXCEPT
{
  if (widthBits <= 0 || widthBits > Detail::kMaxCrcBitWidth)
    return 0;
  if (widthBits == std::numeric_limits<std::uint64_t>::digits)
    return ~std::uint64_t{};
  return (std::uint64_t{ 1ULL } << static_cast<unsigned> (widthBits))
         - std::uint64_t{ 1 };
}

// NOLINTNEXTLINE(altera-struct-pack-align)
struct transport_state_t
{
  TransportCrcMode mode{ TransportCrcMode::Default };
  std::uint32_t catalogIndex{
    0xFFFFFFFFU
  }; // NOLINT(*-magic-numbers) - crc_catalog_legacy_index()
  crc_params_t custom{};
};

std::mutex &
transport_mutex () LUMEX_NOEXCEPT
{
  static std::mutex transport_mutex;
  return transport_mutex;
}

transport_state_t &
transport () LUMEX_NOEXCEPT
{
  static transport_state_t transport_state;
  return transport_state;
}

} // namespace

bool
validate_crc_rev_eng_params (crc_params_t const &params) LUMEX_NOEXCEPT
{
  return params.widthBits >= 1 && params.widthBits <= Detail::kMaxCrcBitWidth;
}

std::uint64_t
compute_crc_with_rev_eng_params (crc_params_t const &params,
                                 std::uint8_t const *data,
                                 std::size_t byteCount) LUMEX_NOEXCEPT
{
  if (!validate_crc_rev_eng_params (params))
    return 0;
  if (data == nullptr || byteCount == 0U)
    return 0;

  int const widthBits = params.widthBits;
  std::uint64_t const mask = mask_for_width (widthBits);
  std::uint64_t const polyMasked = params.poly & mask;
  std::uint64_t const polyReflected
      = Detail::reflect (polyMasked, widthBits) & mask;
  std::uint64_t crcRegister
      = (params.refIn ? Detail::reflect (params.init, widthBits) : params.init)
        & mask;

  for (std::size_t offset = 0; offset < byteCount; ++offset)
    {
      std::uint8_t const messageByte = data
          [offset]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      if (params.refIn)
        {
          for (int bitIndex = 0; bitIndex < kBitsPerByte; ++bitIndex)
            {
              auto const dataBit = static_cast<std::uint8_t> (
                  (static_cast<unsigned> (messageByte) >> bitIndex) & 1U);
              crcRegister ^= static_cast<std::uint64_t> (dataBit);
              if ((crcRegister & 1U) != 0U)
                crcRegister = (crcRegister >> 1) ^ polyReflected;
              else
                crcRegister >>= 1;
              crcRegister &= mask;
            }
        }
      else
        {
          int const msbShiftIndex = widthBits - 1;
          for (int bitIndex = 0; bitIndex < kBitsPerByte; ++bitIndex)
            {
              std::uint8_t const dataBit = static_cast<std::uint8_t> (
                  (static_cast<unsigned> (messageByte)
                   >> (kBitsPerByte - 1 - bitIndex))
                  & 1U);
              std::uint64_t const topBit = (crcRegister >> msbShiftIndex) & 1U;
              crcRegister = (crcRegister << 1) & mask;
              if ((topBit ^ static_cast<std::uint64_t> (dataBit)) != 0U)
                crcRegister ^= polyMasked;
            }
        }
    }
  if (!params.refIn && params.refOut)
    crcRegister = Detail::reflect (crcRegister, widthBits) & mask;
  crcRegister = (crcRegister ^ params.xorOut) & mask;
  return crcRegister;
}

std::uint32_t
get_crc_catalog_entry_count () LUMEX_NOEXCEPT
{
  return static_cast<std::uint32_t> (kCatalogSize);
}

int
get_crc_catalog_bit_width (std::uint32_t catalogIndex) LUMEX_NOEXCEPT
{
  if (catalogIndex == crc_catalog_legacy_index ())
    return kTransportFrameCrcBitWidth;
  if (catalogIndex >= kCatalogSize)
    return -1;
  return kWidthTable.at (catalogIndex);
}

std::uint64_t
compute_crc_catalog (std::uint32_t catalogIndex, std::uint8_t const *data,
                     std::size_t byteCount) LUMEX_NOEXCEPT
{
  if (data == nullptr || byteCount == 0U)
    return 0;
  if (catalogIndex >= kCatalogSize)
    return 0;
  return kComputeTable.at (catalogIndex) (data, byteCount);
}

void
set_transport_crc_default () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (transport_mutex ());
  transport ().mode = TransportCrcMode::Default;
  transport ().catalogIndex = crc_catalog_legacy_index ();
}

bool
set_transport_crc_catalog_index (std::uint32_t catalogIndex) LUMEX_NOEXCEPT
{
  if (catalogIndex == crc_catalog_legacy_index ())
    {
      set_transport_crc_default ();
      return true;
    }
  if (catalogIndex >= kCatalogSize)
    return false;
  if (kWidthTable.at (catalogIndex) != kTransportFrameCrcBitWidth)
    return false;
  std::lock_guard<std::mutex> lock (transport_mutex ());
  transport ().mode = TransportCrcMode::Catalog;
  transport ().catalogIndex = catalogIndex;
  return true;
}

bool
set_transport_crc_rev_eng_params (crc_params_t const &params) LUMEX_NOEXCEPT
{
  if (!validate_crc_rev_eng_params (params))
    return false;
  if (params.widthBits != kTransportFrameCrcBitWidth)
    return false;
  std::lock_guard<std::mutex> lock (transport_mutex ());
  transport ().mode = TransportCrcMode::Custom;
  transport ().custom = params;
  return true;
}

TransportCrcMode
get_transport_crc_mode () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (transport_mutex ());
  return transport ().mode;
}

std::uint32_t
get_transport_crc_catalog_index () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (transport_mutex ());
  if (transport ().mode == TransportCrcMode::Default)
    return crc_catalog_legacy_index ();
  if (transport ().mode == TransportCrcMode::Custom)
    return crc_transport_uses_custom_spec_sentinel ();
  return transport ().catalogIndex;
}

bool
try_get_transport_crc_rev_eng_params (crc_params_t &out) LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (transport_mutex ());
  if (transport ().mode != TransportCrcMode::Custom)
    return false;
  out = transport ().custom;
  return true;
}

std::uint8_t
compute_transport_checksum (std::uint8_t const *data,
                            std::size_t byteCount) LUMEX_NOEXCEPT
{
  if (data == nullptr || byteCount == 0U)
    return 0;
  transport_state_t local;
  {
    std::lock_guard<std::mutex> lock (transport_mutex ());
    local = transport ();
  }
  switch (local.mode)
    {
    case TransportCrcMode::Default:
      return Crc8MaximDow::calculate (data, byteCount);
    case TransportCrcMode::Catalog:
      {
        if (local.catalogIndex >= kCatalogSize)
          return Crc8MaximDow::calculate (data, byteCount);
        if (kWidthTable.at (local.catalogIndex) != kTransportFrameCrcBitWidth)
          return Crc8MaximDow::calculate (data, byteCount);
        return static_cast<std::uint8_t> (
            kComputeTable.at (local.catalogIndex) (data, byteCount)
            & static_cast<std::uint64_t> (
                std::numeric_limits<std::uint8_t>::max ()));
      }
    case TransportCrcMode::Custom:
      return static_cast<std::uint8_t> (
          compute_crc_with_rev_eng_params (local.custom, data, byteCount)
          & static_cast<std::uint64_t> (
              std::numeric_limits<std::uint8_t>::max ()));
    default:
      break;
    }
  return Crc8MaximDow::calculate (data, byteCount);
}
} // namespace catalog
} // namespace crc
} // namespace core
} // namespace lumex

#if __cplusplus < 201703L
namespace lumex
{
namespace core
{
namespace crc
{
namespace parametric
{
// Namespace-scope definitions of the static members of every CRC spec.
// Before C++17 an in-class `static const` member that is odr-used (bound to a
// reference, as EXPECT_EQ does) needs exactly one such definition, and this
// translation unit is always compiled as C++14 (the module's standard), so it
// provides them for every consumer built before C++17. From C++17 the members
// are inline variables. Every spec of LumexCrcParametric.hpp must be listed.
// clang-format off
#define LUMEX_CRC_SPEC_LIST(X) \
  X (crc3_gsm_spec_t) \
  X (crc5_usb_spec_t) \
  X (crc8_maxim_dow_spec_t) \
  X (crc6_cdma2000_a_spec_t) \
  X (crc6_cdma2000_b_spec_t) \
  X (crc8_cdma2000_spec_t) \
  X (crc10_cdma2000_spec_t) \
  X (crc12_cdma2000_spec_t) \
  X (crc16_cdma2000_spec_t) \
  X (crc30_cdma_spec_t) \
  X (crc16_ibm3740_spec_t) \
  X (crc16_kermit_spec_t) \
  X (crc16_modbus_spec_t) \
  X (crc24_open_pgp_spec_t) \
  X (crc32_iso_hdlc_spec_t) \
  X (crc32_iscsi_spec_t) \
  X (crc64_ecma182_spec_t) \
  X (crc3_rohc_spec_t) \
  X (crc4_g704_spec_t) \
  X (crc4_interlaken_spec_t) \
  X (crc5_epc_c1_g2_spec_t) \
  X (crc5_g704_spec_t) \
  X (crc6_darc_spec_t) \
  X (crc6_g704_spec_t) \
  X (crc6_gsm_spec_t) \
  X (crc7_mmc_spec_t) \
  X (crc7_rohc_spec_t) \
  X (crc7_umts_spec_t) \
  X (crc8_autosar_spec_t) \
  X (crc8_bluetooth_spec_t) \
  X (crc8_darc_spec_t) \
  X (crc8_dvb_s2_spec_t) \
  X (crc8_gsm_a_spec_t) \
  X (crc8_gsm_b_spec_t) \
  X (crc8_hitag_spec_t) \
  X (crc8_i4321_spec_t) \
  X (crc8_i_code_spec_t) \
  X (crc8_lte_spec_t) \
  X (crc8_mifare_mad_spec_t) \
  X (crc8_nrsc5_spec_t) \
  X (crc8_opensafety_spec_t) \
  X (crc8_rohc_spec_t) \
  X (crc8_sae_j1850_spec_t) \
  X (crc8_smbus_spec_t) \
  X (crc8_tech3250_spec_t) \
  X (crc8_wcdma_spec_t) \
  X (crc10_atm_spec_t) \
  X (crc10_gsm_spec_t) \
  X (crc11_flexray_spec_t) \
  X (crc11_umts_spec_t) \
  X (crc12_dect_spec_t) \
  X (crc12_gsm_spec_t) \
  X (crc12_umts_spec_t) \
  X (crc13_bbc_spec_t) \
  X (crc14_darc_spec_t) \
  X (crc14_gsm_spec_t) \
  X (crc15_can_spec_t) \
  X (crc15_mpt1327_spec_t) \
  X (crc16_arc_spec_t) \
  X (crc16_cms_spec_t) \
  X (crc16_dds110_spec_t) \
  X (crc16_dect_r_spec_t) \
  X (crc16_dect_x_spec_t) \
  X (crc16_dnp_spec_t) \
  X (crc16_en13757_spec_t) \
  X (crc16_genibus_spec_t) \
  X (crc16_gsm_spec_t) \
  X (crc16_ibm_sdlc_spec_t) \
  X (crc16_iso_iec14443_3_a_spec_t) \
  X (crc16_lj1200_spec_t) \
  X (crc16_m17_spec_t) \
  X (crc16_maxim_dow_spec_t) \
  X (crc16_mcrf4xx_spec_t) \
  X (crc16_nrsc5_spec_t) \
  X (crc16_opensafety_a_spec_t) \
  X (crc16_opensafety_b_spec_t) \
  X (crc16_profibus_spec_t) \
  X (crc16_riello_spec_t) \
  X (crc16_spi_fujitsu_spec_t) \
  X (crc16_t10_dif_spec_t) \
  X (crc16_teledisk_spec_t) \
  X (crc16_tms37157_spec_t) \
  X (crc16_umts_spec_t) \
  X (crc16_usb_spec_t) \
  X (crc16_xmodem_spec_t) \
  X (crc17_can_fd_spec_t) \
  X (crc21_can_fd_spec_t) \
  X (crc24_ble_spec_t) \
  X (crc24_flexray_a_spec_t) \
  X (crc24_flexray_b_spec_t) \
  X (crc24_interlaken_spec_t) \
  X (crc24_lte_a_spec_t) \
  X (crc24_lte_b_spec_t) \
  X (crc24_os9_spec_t) \
  X (crc31_philips_spec_t) \
  X (crc32_aixm_spec_t) \
  X (crc32_autosar_spec_t) \
  X (crc32_base91_d_spec_t) \
  X (crc32_bzip2_spec_t) \
  X (crc32_cd_rom_edc_spec_t) \
  X (crc32_cksum_spec_t) \
  X (crc32_jamcrc_spec_t) \
  X (crc32_mef_spec_t) \
  X (crc32_mpeg2_spec_t) \
  X (crc32_xfer_spec_t) \
  X (crc40_gsm_spec_t) \
  X (crc64_go_iso_spec_t) \
  X (crc64_ms_spec_t) \
  X (crc64_nvme_spec_t) \
  X (crc64_redis_spec_t) \
  X (crc64_we_spec_t) \
  X (crc64_xz_spec_t)
// clang-format on

#define LUMEX_CRC_DEFINE_SPEC_STORAGE(Spec)                                   \
  decltype (Spec::kWidth) Spec::kWidth;                                       \
  decltype (Spec::kPoly) Spec::kPoly;                                         \
  decltype (Spec::kInit) Spec::kInit;                                         \
  decltype (Spec::kRefIn) Spec::kRefIn;                                       \
  decltype (Spec::kRefOut) Spec::kRefOut;                                     \
  decltype (Spec::kXorOut) Spec::kXorOut;                                     \
  decltype (Spec::kCatalogCheck) Spec::kCatalogCheck;

LUMEX_CRC_SPEC_LIST (LUMEX_CRC_DEFINE_SPEC_STORAGE)

#undef LUMEX_CRC_DEFINE_SPEC_STORAGE
#undef LUMEX_CRC_SPEC_LIST
} // namespace parametric
} // namespace crc
} // namespace core
} // namespace lumex
#endif
