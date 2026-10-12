// Header-order check of cmake.hygiene_compile_checks. Exactly one of
// LUMEX_HYGIENE_TIME_FIRST or LUMEX_HYGIENE_GENERATORS_FIRST is defined.
//
// lumex::core::time is a namespace, so a header of a sibling module that
// spells a C library function `time ()` unqualified finds the namespace
// instead of ::time as soon as LumexTime came first. The number generator
// seeds from std::time when the random device has no entropy; the check
// instantiates both of its constructors in each include order.

#if defined(LUMEX_HYGIENE_TIME_FIRST)
#include "lumex/core/generators/LumexGenerators"
#include "lumex/core/time/LumexTime"
#elif defined(LUMEX_HYGIENE_GENERATORS_FIRST)
#include "lumex/core/generators/LumexGenerators"
#include "lumex/core/time/LumexTime"
#else
#error "define LUMEX_HYGIENE_TIME_FIRST or LUMEX_HYGIENE_GENERATORS_FIRST"
#endif

int
draw_after_time_and_generators ()
{
  using lumex::core::generators::number_generator::number_generator;
  number_generator<int> default_bounds;
  number_generator<int> given_bounds (1, 6);
  return default_bounds () + given_bounds ();
}
