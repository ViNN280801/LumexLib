#define LUMEX_IMPLEMENTATION
#include "lumex/core/base64/Validator.hpp"

using namespace Lumex::Core::Base64;

namespace
{
  bool
  validate_base64_characters(string_type_t str, size_t data_length)
  {
    for(size_t i = 0; i < data_length; ++i)
    {
      auto current_char = static_cast<byte_type>(str[i]);
      if(detail::_decode_table.at(current_char) == Constants::kBase64DecodeInvalidChar) return false;
    }
    return true;
  }

  bool
  validate_padding_format(string_type_t str, size_t padding_start)
  {
    if(str.length() % 4 != 0) return false;

    size_t padding_count = str.length() - padding_start;
    if(padding_count > 2) return false;

    for(size_t i = padding_start; i < str.length(); ++i)
      if(str[i] != '=') return false;

    return true;
  }

  bool
  validate_padding_correctness(size_t data_length, size_t padding_count) // NOLINT(bugprone-easily-swappable-parameters)
  {
    size_t remainder = data_length % 4;
    if(remainder == 0 && padding_count > 0) return false;
    if(remainder == 1) return false;
    if(remainder == 2 && padding_count != 2) return false;
    if(remainder == 3 && padding_count != 1) return false;
    return true;
  }
} // namespace anonymous

LUMEX_PUBLIC_API
bool
Validator::is_valid_base64(string_type_t str)
{
  if(str.empty()) return true;

  size_t padding_start = str.find('=');
  size_t data_length   = (padding_start == std::string::npos) ? str.length() : padding_start;

  if(!validate_base64_characters(str, data_length)) return false;

  if(padding_start != std::string::npos)
  {
    if(!validate_padding_format(str, padding_start)) return false;
    size_t padding_count = str.length() - padding_start;
    return validate_padding_correctness(data_length, padding_count);
  }

  // No padding - validate unpadded Base64
  size_t remainder = data_length % 4;
  return remainder != 1; // Only remainder == 1 is invalid for unpadded Base64
}
