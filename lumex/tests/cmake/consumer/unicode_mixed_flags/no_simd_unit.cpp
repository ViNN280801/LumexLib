// Built with the macro that forces the scalar routine.
#define LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD

#include "unit.hpp"

#include "lumex/core/unicode/utf/LumexUtf8Validate.hpp"

unit_result
no_simd_unit (char const *data, std::size_t size)
{
  unit_result result;
  result.variant = static_cast<int> (
      lumex::core::unicode::utf::detail::utf8_selected_variant ());
  result.prefix
      = lumex::core::unicode::utf::utf8_valid_prefix_length (data, size);
  result.valid = lumex::core::unicode::utf::is_valid_utf8 (data, size) ? 1 : 0;
  return result;
}
