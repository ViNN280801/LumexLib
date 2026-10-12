// Compile check of the value of LUMEX_SPAN_CHECKS, built by
// cmake.span_compile_checks with and without NDEBUG and with the macro
// defined by the program. LUMEX_SPAN_EXPECT is the value the build must give.

#include "lumex/core/span/LumexSpan"

#if !defined(LUMEX_SPAN_EXPECT)
#error "LUMEX_SPAN_EXPECT is not defined"
#endif

static_assert (LUMEX_SPAN_CHECKS == LUMEX_SPAN_EXPECT,
               "LUMEX_SPAN_CHECKS has an unexpected value");

int
main ()
{
  return 0;
}
