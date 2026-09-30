#define LUMEX_IMPLEMENTATION

#include <cstddef>

#include "Validator.hpp"

using namespace lumex::core::base64::validate;
using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::codec::Types;
using namespace lumex::core::base64::codec::Constants;

namespace
{
bool
validate_base64_characters (char const *str, std::size_t data_length)
{
  for (std::size_t i = 0; i < data_length; ++i)
    {
      auto current_char = static_cast<byte_type> (str[i]);
      if (detail::_decode_table.at (current_char)
          == Constants::kBase64DecodeInvalidChar)
        return false;
    }
  return true;
}

bool
validate_padding_format (char const *str, std::size_t size,
                         std::size_t padding_start)
{
  if (size % 4 != 0)
    return false;

  std::size_t padding_count = size - padding_start;
  if (padding_count > 2)
    return false;

  for (std::size_t i = padding_start; i < size; ++i)
    if (str[i] != '=')
      return false;

  return true;
}

bool
validate_padding_correctness (
    std::size_t data_length,
    std::size_t padding_count) // NOLINT(bugprone-easily-swappable-parameters)
{
  std::size_t remainder = data_length % 4;
  if (remainder == 0 && padding_count > 0)
    return false;
  if (remainder == 1)
    return false;
  if (remainder == 2 && padding_count != 2)
    return false;
  if (remainder == 3 && padding_count != 1)
    return false;
  return true;
}
} // namespace

LUMEX_PUBLIC_API
bool
Validator::is_valid_base64 (char const *str, std::size_t size)
{
  if (str == nullptr)
    return false;
  if (size == 0)
    return true;

  // The first '=' inside the range; `size` when there is none.
  std::size_t padding_start = 0;
  while (padding_start < size && str[padding_start] != '=')
    ++padding_start;
  std::size_t const data_length = padding_start;

  if (!validate_base64_characters (str, data_length))
    return false;

  if (padding_start != size)
    {
      if (!validate_padding_format (str, size, padding_start))
        return false;
      std::size_t padding_count = size - padding_start;
      return validate_padding_correctness (data_length, padding_count);
    }

  // No padding - validate unpadded Base64
  std::size_t remainder = data_length % 4;
  return remainder != 1; // Only remainder == 1 is invalid for unpadded Base64
}
