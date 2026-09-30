#define LUMEX_IMPLEMENTATION
#include <cstddef>
#include <vector>

#include "Decoder.hpp"
#include "lumex/core/base64/validate/Validator.hpp"

using namespace lumex::core::base64::decode;
using namespace lumex::core::base64::validate;
using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::codec::Types;
using namespace lumex::core::base64::codec::detail;

namespace
{
/// @brief Six-bit value of an alphabet character (the validator already
/// rejected every other character).
byte_type
sextet (char chr)
{
  return detail::_decode_table.at (static_cast<byte_type> (chr));
}
} // namespace

LUMEX_PUBLIC_API
bool
Decoder::decode (char const *encoded, std::size_t size,
                 std::vector<byte_type> &out)
{
  out.clear ();
  if (encoded == nullptr)
    return false;
  if (size == 0)
    return true;

  // Length, invalid characters, and padding position and amount.
  if (!Validator::is_valid_base64 (encoded, size))
    return false;

  // Padding only ends the input, so the data characters are a prefix. The
  // padding may also be omitted, which leaves a last group of two or three
  // characters (never one: the validator rejects that length).
  std::size_t data_length = size;
  while (data_length > 0 && encoded[data_length - 1] == '=')
    --data_length;

  out.reserve (data_length * 3 / 4);

  // Each step keeps the low 8 bits of the combined sextets.
  std::size_t pos = 0;
  for (; pos + 4 <= data_length; pos += 4)
    {
      byte_type const byte1 = sextet (encoded[pos]);
      byte_type const byte2 = sextet (encoded[pos + 1]);
      byte_type const byte3 = sextet (encoded[pos + 2]);
      byte_type const byte4 = sextet (encoded[pos + 3]);
      out.push_back (static_cast<byte_type> ((byte1 << 2) | (byte2 >> 4)));
      out.push_back (static_cast<byte_type> ((byte2 << 4) | (byte3 >> 2)));
      out.push_back (static_cast<byte_type> (
          (byte3 << 6) // NOLINT(cppcoreguidelines-avoid-magic-numbers,
                       // readability-magic-numbers)
          | byte4));
    }

  // Last group of two or three data characters: one or two bytes.
  std::size_t const rest = data_length - pos;
  if (rest >= 2)
    {
      byte_type const byte1 = sextet (encoded[pos]);
      byte_type const byte2 = sextet (encoded[pos + 1]);
      out.push_back (static_cast<byte_type> ((byte1 << 2) | (byte2 >> 4)));
      if (rest == 3)
        {
          byte_type const byte3 = sextet (encoded[pos + 2]);
          out.push_back (static_cast<byte_type> ((byte2 << 4) | (byte3 >> 2)));
        }
    }

  return true;
}

LUMEX_PUBLIC_API
std::vector<byte_type>
Decoder::decode (char const *encoded, std::size_t size)
{
  std::vector<byte_type> result;
  decode (encoded, size, result);
  return result;
}
