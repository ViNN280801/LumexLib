#define LUMEX_IMPLEMENTATION
#include <cstdlib>
#include <iostream>

#include "LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

LUMEX_PUBLIC_API
void
lumex_assert_handler (char const *assertion, char const *file,
                      int line) LUMEX_NOEXCEPT
{
  std::cerr << "Assertion failed: " << assertion << ", file " << file
            << ", line " << line << "\n";
  std::abort ();
}
