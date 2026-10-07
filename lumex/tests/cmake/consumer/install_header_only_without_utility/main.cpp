// cmake.install_header_only_without_utility: the five header-only
// modules compile and work from an install prefix that has no core/utility
// module. Their umbrellas include core/utility headers, which LumexLib
// installs whatever LUMEX_BUILD_UTILITY says. Exit code 0 on success, 1 when
// a call returns an unexpected value.

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/generators/LumexGenerators"
#include "lumex/core/math/LumexMath"
#include "lumex/core/optional/LumexOptional"
#include "lumex/core/string/LumexString"

namespace
{
int failures = 0;

void
check (bool condition, char const *what)
{
  if (!condition)
    {
      std::printf ("failed: %s\n", what);
      ++failures;
    }
}
} // namespace

int
main ()
{
  std::vector<int> const numbers = { 1, 2, 3 };
  check (lumex::core::string::utility::stringify ("n=", 2) == "n=2",
         "string: stringify");
  check (lumex::core::string::text::join (numbers, ",") == "1,2,3",
         "string: join");

  check (lumex::core::math::ops::avg (numbers) == 2.0, "math: avg");
  check (LUMEX_MATH_CONSTANTS_PI > 3.14 && LUMEX_MATH_CONSTANTS_PI < 3.15,
         "math: constants");

  lumex::core::generators::number_generator::number_generator<int> dice (1, 6);
  int const roll = dice ();
  check (roll >= 1 && roll <= 6, "generators: NumberGenerator");

  lumex::core::optional::opt::optional<int> value;
  check (!value.has_value (), "optional: empty");
  value.emplace (7);
  check (value.value_or (0) == 7, "optional: emplace");

  lumex::core::atomic::smart_ptr::atomic_shared_ptr<int> cell (
      std::make_shared<int> (1));
  cell.store (std::make_shared<int> (2));
  check (*cell.load () == 2, "atomic: atomic_shared_ptr");

  std::printf ("header-only modules from the install prefix: %d failure(s)\n",
               failures);
  return failures == 0 ? 0 : 1;
}
