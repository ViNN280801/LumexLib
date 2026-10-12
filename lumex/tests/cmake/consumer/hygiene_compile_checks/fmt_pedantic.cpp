// The fmt headers under -Wall -Wextra -Wpedantic -Werror, built by
// cmake.hygiene_compile_checks once per header (LUMEX_HYGIENE_FMT_CORE,
// _CHRONO, _RANGES or _UMBRELLA selects the include): GCC reports "ISO C++
// does not support '__int128'" at every spelling of the type, so the header
// names it once through an `__extension__` alias. The 128-bit integers are
// formatted so that every template that touches them is instantiated.
// LUMEX_HYGIENE_FMT_RAW_INT128 is the control: a plain `__int128` that GCC
// must reject under the same flags (Clang accepts it silently).

#include <chrono>
#include <string>
#include <vector>

#if defined(LUMEX_HYGIENE_FMT_CORE)
#include "lumex/core/fmt/LumexFormat.hpp"
#elif defined(LUMEX_HYGIENE_FMT_CHRONO)
#include "lumex/core/fmt/LumexFormatChrono.hpp"
#elif defined(LUMEX_HYGIENE_FMT_RANGES)
#include "lumex/core/fmt/LumexFormatRanges.hpp"
#elif defined(LUMEX_HYGIENE_FMT_UMBRELLA)
#include "lumex/core/fmt/LumexFormat"
#elif defined(LUMEX_HYGIENE_FMT_RAW_INT128)
// Not a header of the library: the plain spelling of the type, which GCC
// rejects under -Wpedantic. It proves that the checks above can fail.
__int128 raw_int128 = 0;
#else
#error "select a header: LUMEX_HYGIENE_FMT_CORE, _CHRONO, _RANGES or _UMBRELLA"
#endif

#if !defined(LUMEX_HYGIENE_FMT_RAW_INT128)
namespace fmt = lumex::core::fmt;

std::string
format_integers ()
{
  std::string text = fmt::format ("{} {:x} {:>8}", 1, 255u, -3L);
#if defined(__SIZEOF_INT128__)
  // The user's own spelling of the type needs the extension marker too.
  __extension__ typedef __int128 signed_128;
  __extension__ typedef unsigned __int128 unsigned_128;
  signed_128 const negative = -static_cast<signed_128> (170141183460469231LL);
  unsigned_128 const big = static_cast<unsigned_128> (1) << 100;
  text += fmt::format (" {} {} {:#x} {:+}", negative, big, big, negative);
#endif
  return text;
}

#if defined(LUMEX_HYGIENE_FMT_CHRONO) || defined(LUMEX_HYGIENE_FMT_UMBRELLA)
std::string
format_durations ()
{
  return fmt::format ("{} {}", std::chrono::seconds (3),
                      std::chrono::milliseconds (250));
}
#endif

#if defined(LUMEX_HYGIENE_FMT_RANGES) || defined(LUMEX_HYGIENE_FMT_UMBRELLA)
std::string
format_ranges ()
{
  std::vector<int> const values = { 1, 2, 3 };
  return fmt::format ("{}", values);
}
#endif
#endif // !LUMEX_HYGIENE_FMT_RAW_INT128
