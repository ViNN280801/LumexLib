#define LUMEX_IMPLEMENTATION
#include "lumex/core/base64/Decoder.hpp"
#include "lumex/core/base64/Validator.hpp"

using namespace Lumex::Core::Base64;

LUMEX_PUBLIC_API
#if __cplusplus >= 201703L
bool
Decoder::decode(std::string_view encoded, std::vector<byte_type> &out)
#else
bool
Decoder::decode(std::string const &encoded, std::vector<byte_type> &out)
#endif
{
  if(encoded.empty())
  {
    out.clear();
    return true;
  }

  // This handles length, invalid characters, and incorrect padding positions/amounts.
  if(!Validator::is_valid_base64(encoded))
  {
    out.clear(); // Ensure output is empty on failure
    return false;
  }

  // Calculate output size - This part is correct and depends on padding
  size_t output_size = (encoded.length() / 4) * 3;
  if(encoded[encoded.length() - 1] == '=') output_size--;
  if(encoded[encoded.length() - 2] == '=') output_size--;

  out.clear();
  out.reserve(output_size);

  for(size_t i = 0UL; i < encoded.length(); i += 4)
  {
    // These calls use detail::_decode_table, which is correct for mapping characters to their values.
    byte_type byte1 = detail::_decode_table.at(static_cast<byte_type>(encoded[i]));
    byte_type byte2 = detail::_decode_table.at(static_cast<byte_type>(encoded[i + 1]));
    byte_type byte3 = (encoded[i + 2] == '=') ? 0 : detail::_decode_table.at(static_cast<byte_type>(encoded[i + 2]));
    byte_type byte4 = (encoded[i + 3] == '=') ? 0 : detail::_decode_table.at(static_cast<byte_type>(encoded[i + 3]));

    // 1. Get the first 6 bits of the first byte
    out.push_back((byte1 << 2) | (byte2 >> 4));

    // 2. Get the last 4 bits of the second byte and the first 2 bits of the third byte
    if(encoded[i + 2] != '=') out.push_back((byte2 << 4) | (byte3 >> 2));

    // 3. Get the last 6 bits of the third byte
    if(encoded[i + 3] != '=')
      out.push_back((byte3 << 6) // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
                    | byte4);
  }

  return true;
}

LUMEX_PUBLIC_API
#if __cplusplus >= 201703L
std::vector<byte_type>
Decoder::decode(std::string_view encoded)
#else
std::vector<byte_type>
Decoder::decode(std::string const &encoded)
#endif
{
  std::vector<byte_type> result;
  decode(encoded, result);
  return result;
}
