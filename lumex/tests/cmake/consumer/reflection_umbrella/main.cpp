#include <cstdint>
#include <cstring>

#include "lumex/core/reflection/LumexReflection"

LUMEX_DEFINE_REFLECTED_ENUM (SmokeColor, std::uint8_t, (red), (green))

int
main ()
{
#if defined(LUMEX_WITH_FIELD_REFLECTION)
  // The option must not reach consumers that did not opt in.
  return 2;
#else
  return std::strcmp (toString (SmokeColor::green), "green") == 0 ? 0 : 1;
#endif
}
