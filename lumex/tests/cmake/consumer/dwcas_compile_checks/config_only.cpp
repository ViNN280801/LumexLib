// Built by cmake.dwcas_compile_checks with the predefined macros of the
// compiler replaced (-undef and chosen -D options): the configuration header
// must give the expected answers. The header includes nothing, so this
// translation unit needs no standard header either.
#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS != EXPECT_HAS
#error "LUMEX_ATOMIC_HAS_DWCAS is not the expected value"
#endif

#if LUMEX_DWCAS_BACKEND != EXPECT_BACKEND
#error "LUMEX_DWCAS_BACKEND is not the expected value"
#endif

#if LUMEX_DWCAS_BACKEND_NONE != 0 || LUMEX_DWCAS_BACKEND_ASM != 1             \
    || LUMEX_DWCAS_BACKEND_MSVC != 2 || LUMEX_DWCAS_BACKEND_BUILTIN != 3
#error "the backend constants changed"
#endif

int
main ()
{
  return 0;
}
