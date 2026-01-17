#define LUMEX_IMPLEMENTATION
#include "LumexAssert.hpp"

#include <cstdlib>
#include <iostream>

LUMEX_PUBLIC_API
void
lumex_assert_handler(char const *assertion, char const *file, int line) noexcept
{
  std::cerr << "Assertion failed: " << assertion << ", file " << file << ", line " << line << "\n";
  std::abort();
}
