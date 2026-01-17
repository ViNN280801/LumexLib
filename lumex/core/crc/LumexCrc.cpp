#define LUMEX_IMPLEMENTATION
#include <iterator>

#include "lumex/core/crc/LumexCrc.hpp"

LUMEX_PUBLIC_API
uint8_t
Lumex::Core::crc::Crc4::calculateCrc4(uint8_t data) LUMEX_NOEXCEPT_FUNCTION
{
  try
  {
    uint8_t crc4Result = Constants::kCrc4Table.at(data >> 4);
    crc4Result         = Constants::kCrc4Table.at(crc4Result ^ (data & Constants::kLowerNibbleMask));
    return crc4Result;
  }
  catch(...)
  {
    return 0U;
  }
}

LUMEX_PUBLIC_API
uint8_t
Lumex::Core::crc::Crc8::calculateCrc8(uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT_FUNCTION
{
  try
  {
    if(data == nullptr || size == 0UL) return 0;
    return calculateCrc8(std::vector<uint8_t>(data, std::next(data, static_cast<std::ptrdiff_t>(size))));
  }
  catch(...)
  {
    return 0U;
  }
}

LUMEX_PUBLIC_API
uint8_t
Lumex::Core::crc::Crc8::calculateCrc8(std::vector<uint8_t> const &data) LUMEX_NOEXCEPT_FUNCTION
{
  try
  {
    if(data.empty()) return 0;
    uint8_t crc8Byte = 0;
    for(unsigned char index : data) crc8Byte = Constants::kCrc8Table.at(crc8Byte ^ index);
    return crc8Byte;
  }
  catch(...)
  {
    return 0U;
  }
}

#if __cplusplus >= 202002L
LUMEX_PUBLIC_API
std::uint8_t
Lumex::Core::crc::Crc8::calculateCrc8(std::span<std::uint8_t const> data) LUMEX_NOEXCEPT_FUNCTION
{
  return calculateCrc8(data.data(), data.size());
}
#endif
