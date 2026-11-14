// Every overload of the UTF-8 validation, and the routines behind them that
// this build has, called in a translation unit that a strict consumer compiles
// (see CMakeLists.txt). Exit code 0 when every check held.
#include <cstddef>
#include <cstdint>
#include <string>

#if defined(LUMEX_FIXTURE_UNICODE_UMBRELLA)
#include "lumex/core/unicode/LumexUnicode"
#else
#include "lumex/core/unicode/utf/LumexUtf8Validate.hpp"
#endif

namespace utf = lumex::core::unicode::utf;

namespace
{
bool
check_bytes (std::uint8_t const *data, std::size_t size, std::size_t prefix)
{
  return utf::utf8_valid_prefix_length (data, size) == prefix
         && utf::is_valid_utf8 (data, size) == (prefix == size);
}

bool
check_chars (char const *data, std::size_t size, std::size_t prefix)
{
  return utf::utf8_valid_prefix_length (data, size) == prefix
         && utf::is_valid_utf8 (data, size) == (prefix == size);
}

#if LUMEX_HAS_CHAR8_T
bool
check_units (char8_t const *data, std::size_t size, std::size_t prefix)
{
  return utf::utf8_valid_prefix_length (data, size) == prefix
         && utf::is_valid_utf8 (data, size) == (prefix == size);
}
#endif
} // namespace

int
main ()
{
  // 70 bytes of ASCII and two-byte characters, then a stray continuation
  // byte, then more: long enough for a block of 32 bytes and a group of 64
  std::string text (40, 'a');
  for (int i = 0; i < 15; ++i)
    text += "\xC3\xA9";
  std::string bad = text;
  bad += '\x80';
  bad += "tail";

  bool ok = true;
  ok = check_chars (text.data (), text.size (), text.size ()) && ok;
  ok = check_chars (bad.data (), bad.size (), text.size ()) && ok;
  ok = check_bytes (reinterpret_cast<std::uint8_t const *> (bad.data ()),
                    bad.size (), text.size ())
       && ok;
  ok = check_chars (nullptr, 0, 0) && ok;
#if LUMEX_HAS_CHAR8_T
  ok = check_units (reinterpret_cast<char8_t const *> (bad.data ()),
                    bad.size (), text.size ())
       && ok;
#endif

#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  // the routines of every variant, called directly, on a CPU that has them
  std::uint8_t const *const bytes
      = reinterpret_cast<std::uint8_t const *> (bad.data ());
  if (utf::detail::utf8_cpu_has_ssse3 ())
    ok = (utf::detail::utf8_valid_prefix_length_ssse3 (bytes, bad.size ())
          == text.size ())
         && ok;
  if (utf::detail::utf8_cpu_has_avx2 ())
    ok = (utf::detail::utf8_valid_prefix_length_avx2 (bytes, bad.size ())
          == text.size ())
         && ok;
#endif
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
  ok = (utf::detail::utf8_valid_prefix_length_neon (
            reinterpret_cast<std::uint8_t const *> (bad.data ()), bad.size ())
        == text.size ())
       && ok;
#endif

  return ok ? 0 : 1;
}
