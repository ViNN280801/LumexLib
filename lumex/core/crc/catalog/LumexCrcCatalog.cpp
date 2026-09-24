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
ComputeEntry (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  using Spec = typename std::tuple_element<I, CatalogTuple>::type;
  return static_cast<std::uint64_t> (
      CrcParametric<Spec>::calculate (data, size));
}

template <std::size_t... I>
std::array<ComputeFn, sizeof...(I)>
MakeComputeTable (std::index_sequence<I...> /*unusedIndexSequence*/)
    LUMEX_NOEXCEPT
{
  return std::array<ComputeFn, sizeof...(I)>{ { &ComputeEntry<I>... } };
}

#if __cplusplus >= 201402L
using CatalogIndexSequence = std::make_index_sequence<kCatalogSize>;
#else
using CatalogIndexSequence =
    typename std::make_index_sequence<kCatalogSize>::type;
#endif

std::array<ComputeFn, kCatalogSize> const kComputeTable
    = MakeComputeTable (CatalogIndexSequence{});

template <std::size_t I>
LUMEX_CONSTEXPR_FUNCTION int
WidthEntry () LUMEX_NOEXCEPT
{
  using Spec = typename std::tuple_element<I, CatalogTuple>::type;
  return Spec::kWidth;
}

template <std::size_t... I>
LUMEX_CONSTEXPR_FUNCTION std::array<int, sizeof...(I)>
MakeWidthTable (std::index_sequence<I...> /*unusedIndexSequence*/)
    LUMEX_NOEXCEPT
{
  return std::array<int, sizeof...(I)>{ { WidthEntry<I> ()... } };
}

LUMEX_CONST_NUM std::array<int, kCatalogSize> kWidthTable
    = MakeWidthTable (CatalogIndexSequence{});

std::uint64_t
MaskForWidth (int widthBits) LUMEX_NOEXCEPT
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
  }; // NOLINT(*-magic-numbers) - CrcCatalogLegacyIndex()
  crc_params_t custom{};
};

std::mutex &
TransportMutex () LUMEX_NOEXCEPT
{
  static std::mutex transport_mutex;
  return transport_mutex;
}

transport_state_t &
Transport () LUMEX_NOEXCEPT
{
  static transport_state_t transport_state;
  return transport_state;
}

} // namespace

bool
ValidateCrcRevEngParams (crc_params_t const &params) LUMEX_NOEXCEPT
{
  return params.widthBits >= 1 && params.widthBits <= Detail::kMaxCrcBitWidth;
}

std::uint64_t
ComputeCrcWithRevEngParams (crc_params_t const &params,
                            std::uint8_t const *data,
                            std::size_t byteCount) LUMEX_NOEXCEPT
{
  if (!ValidateCrcRevEngParams (params))
    return 0;
  if (data == nullptr || byteCount == 0U)
    return 0;

  int const widthBits = params.widthBits;
  std::uint64_t const mask = MaskForWidth (widthBits);
  std::uint64_t const polyMasked = params.poly & mask;
  std::uint64_t const polyReflected
      = Detail::Reflect (polyMasked, widthBits) & mask;
  std::uint64_t crcRegister
      = (params.refIn ? Detail::Reflect (params.init, widthBits) : params.init)
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
    crcRegister = Detail::Reflect (crcRegister, widthBits) & mask;
  crcRegister = (crcRegister ^ params.xorOut) & mask;
  return crcRegister;
}

std::uint32_t
GetCrcCatalogEntryCount () LUMEX_NOEXCEPT
{
  return static_cast<std::uint32_t> (kCatalogSize);
}

int
GetCrcCatalogBitWidth (std::uint32_t catalogIndex) LUMEX_NOEXCEPT
{
  if (catalogIndex == CrcCatalogLegacyIndex ())
    return kTransportFrameCrcBitWidth;
  if (catalogIndex >= kCatalogSize)
    return -1;
  return kWidthTable.at (catalogIndex);
}

std::uint64_t
ComputeCrcCatalog (std::uint32_t catalogIndex, std::uint8_t const *data,
                   std::size_t byteCount) LUMEX_NOEXCEPT
{
  if (data == nullptr || byteCount == 0U)
    return 0;
  if (catalogIndex >= kCatalogSize)
    return 0;
  return kComputeTable.at (catalogIndex) (data, byteCount);
}

void
SetTransportCrcDefault () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (TransportMutex ());
  Transport ().mode = TransportCrcMode::Default;
  Transport ().catalogIndex = CrcCatalogLegacyIndex ();
}

bool
SetTransportCrcCatalogIndex (std::uint32_t catalogIndex) LUMEX_NOEXCEPT
{
  if (catalogIndex == CrcCatalogLegacyIndex ())
    {
      SetTransportCrcDefault ();
      return true;
    }
  if (catalogIndex >= kCatalogSize)
    return false;
  if (kWidthTable.at (catalogIndex) != kTransportFrameCrcBitWidth)
    return false;
  std::lock_guard<std::mutex> lock (TransportMutex ());
  Transport ().mode = TransportCrcMode::Catalog;
  Transport ().catalogIndex = catalogIndex;
  return true;
}

bool
SetTransportCrcRevEngParams (crc_params_t const &params) LUMEX_NOEXCEPT
{
  if (!ValidateCrcRevEngParams (params))
    return false;
  if (params.widthBits != kTransportFrameCrcBitWidth)
    return false;
  std::lock_guard<std::mutex> lock (TransportMutex ());
  Transport ().mode = TransportCrcMode::Custom;
  Transport ().custom = params;
  return true;
}

TransportCrcMode
GetTransportCrcMode () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (TransportMutex ());
  return Transport ().mode;
}

std::uint32_t
GetTransportCrcCatalogIndex () LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (TransportMutex ());
  if (Transport ().mode == TransportCrcMode::Default)
    return CrcCatalogLegacyIndex ();
  if (Transport ().mode == TransportCrcMode::Custom)
    return CrcTransportUsesCustomSpecSentinel ();
  return Transport ().catalogIndex;
}

bool
TryGetTransportCrcRevEngParams (crc_params_t &out) LUMEX_NOEXCEPT
{
  std::lock_guard<std::mutex> lock (TransportMutex ());
  if (Transport ().mode != TransportCrcMode::Custom)
    return false;
  out = Transport ().custom;
  return true;
}

std::uint8_t
ComputeTransportChecksum (std::uint8_t const *data,
                          std::size_t byteCount) LUMEX_NOEXCEPT
{
  if (data == nullptr || byteCount == 0U)
    return 0;
  transport_state_t local;
  {
    std::lock_guard<std::mutex> lock (TransportMutex ());
    local = Transport ();
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
          ComputeCrcWithRevEngParams (local.custom, data, byteCount)
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
