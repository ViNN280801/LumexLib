// LumexDebug.hpp sizes an array with LUMEX_CONSTEXPR; it stops compiling
// when __cplusplus reads 199711L (MSVC without /Zc:__cplusplus).
#include "lumex/core/utility/debug/LumexDebug.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

static_assert (__cplusplus >= 201703L,
               "__cplusplus does not report the real standard: "
               "lumex::utility must pass /Zc:__cplusplus to MSVC consumers");

int
main ()
{
  LUMEX_CONSTEXPR int kSize = 4;
  int values[kSize] = { 0, 1, 2, 3 };
  return values[kSize - 1] == 3 ? 0 : 1;
}
