// Probe for cmake.consumer_span_mismatch_guard: compiled once with the compile
// definitions of a compiled module of the library and once without them.
// EXPECT_GUARD is 0 for the first and 1 for the second.
#include "lumex/core/span/view/LumexSpanChecks.hpp"

#ifndef EXPECT_GUARD
#error "EXPECT_GUARD is not defined"
#endif

#if LUMEX_SPAN_MISMATCH_GUARD != EXPECT_GUARD
#error "the mismatch guard of the translation unit is not the expected one"
#endif

#if EXPECT_GUARD == 0 && !defined(LUMEX_SPAN_NO_MISMATCH_GUARD)
#error "a library translation unit lacks LUMEX_SPAN_NO_MISMATCH_GUARD"
#endif

#if EXPECT_GUARD == 1 && defined(LUMEX_SPAN_NO_MISMATCH_GUARD)
#error "a consumer translation unit has LUMEX_SPAN_NO_MISMATCH_GUARD"
#endif

int
lumex_span_mismatch_guard_probe ()
{
  return LUMEX_SPAN_MISMATCH_GUARD;
}
