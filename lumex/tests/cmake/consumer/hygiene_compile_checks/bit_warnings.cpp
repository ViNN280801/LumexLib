// The bit header under the strictest flags of a consumer, built with -Wall
// -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
// -Werror by cmake.hygiene_compile_checks: count_leading_zeros narrowed the
// result of a 32-bit intrinsic to std::uint8_t for the one-byte and two-byte
// types (-Wconversion, -Wsign-conversion), and the function templates must
// be instantiated for every width to show it.

#include <cstdint>

#include "lumex/core/utility/bit/LumexBit.hpp"

namespace
{

template <typename T>
std::uint8_t
leading_zeros_of (T value)
{
  return lumex::core::utility::bit::count_leading_zeros (value);
}

template <typename T>
T
swapped (T value)
{
  return lumex::core::utility::bit::byte_swap (value);
}

} // namespace

std::uint8_t
bit_widths (std::uint64_t value)
{
  return static_cast<std::uint8_t> (
      leading_zeros_of (static_cast<unsigned char> (value))
      + leading_zeros_of (static_cast<std::uint8_t> (value))
      + leading_zeros_of (static_cast<unsigned short> (value))
      + leading_zeros_of (static_cast<std::uint32_t> (value))
      + leading_zeros_of (static_cast<unsigned long> (value))
      + leading_zeros_of (static_cast<unsigned long long> (value)));
}

std::uint64_t
bit_swaps (std::uint64_t value)
{
  std::uint64_t sum = static_cast<std::uint64_t> (
      swapped (static_cast<std::int16_t> (value)));
  sum += swapped (static_cast<std::uint16_t> (value));
  sum += static_cast<std::uint64_t> (
      swapped (static_cast<std::int32_t> (value)));
  sum += swapped (static_cast<std::uint32_t> (value));
  sum += static_cast<std::uint64_t> (
      swapped (static_cast<std::int64_t> (value)));
  sum += swapped (value);
  sum += lumex::core::utility::bit::is_little_endian () ? 1U : 0U;
  return sum;
}
