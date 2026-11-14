// Built with the options of the program, nothing else.
#include <cstdint>

#include "unit.hpp"

#include "lumex/core/unicode/utf/LumexUtf8Validate.hpp"

unit_result
plain_unit (char const *data, std::size_t size)
{
  unit_result result;
  result.variant = static_cast<int> (
      lumex::core::unicode::utf::detail::utf8_selected_variant ());
  result.prefix
      = lumex::core::unicode::utf::utf8_valid_prefix_length (data, size);
  result.valid = lumex::core::unicode::utf::is_valid_utf8 (data, size) ? 1 : 0;
  return result;
}

// Where this unit has the SSSE3 routine: the same address as the other unit's
// when the linker has kept one copy for both (0 where there is none).
std::uintptr_t
plain_unit_ssse3_address ()
{
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  return reinterpret_cast<std::uintptr_t> (
      &lumex::core::unicode::utf::detail::utf8_valid_prefix_length_ssse3);
#else
  return 0;
#endif
}
