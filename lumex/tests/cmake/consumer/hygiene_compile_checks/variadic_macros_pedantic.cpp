// Macros of the library that select an overload by counting arguments, under
// -Wall -Wextra -Wpedantic -Werror (cmake.hygiene_compile_checks). Before
// C++20 a macro whose `...` receives no argument is an extension: GCC reports
// it with -Wpedantic and Clang with -Wvariadic-macro-arguments-omitted. The
// pickers of LUMEX_DEFINE_REFLECTED_ENUM (an enumerator without a value, a
// list of one enumerator) and of LUMEX_MEASURE_TIME (the form with one
// argument) used to leave it empty.
//
// The enums cover one to sixteen enumerators, with and without values, at
// namespace scope and inside a class.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/core/time/timer/LumexTimer.hpp"
#include "lumex/core/utility/macros/LumexExceptionMacros.hpp"

LUMEX_DEFINE_REFLECTED_ENUM (single_t, std::uint8_t, (only))

LUMEX_DEFINE_REFLECTED_ENUM (single_valued_t, int, (only, 4))

LUMEX_DEFINE_REFLECTED_ENUM (pair_t, std::uint8_t, (first), (second, 7))

LUMEX_DEFINE_REFLECTED_ENUM (mixed_t, int, (a), (b, 5), (c), (d, 9), (e))

LUMEX_DEFINE_REFLECTED_ENUM (sixteen_t, int, (e1), (e2), (e3), (e4), (e5),
                             (e6), (e7), (e8), (e9), (e10, 10), (e11), (e12),
                             (e13), (e14), (e15), (e16))

#define LUMEX_HYGIENE_STRINGS(ENTRY) ENTRY (red, "r") ENTRY (green, "g")
LUMEX_DEFINE_REFLECTED_ENUM_TO_STRING (colour_t, std::uint8_t,
                                       LUMEX_HYGIENE_STRINGS, (red),
                                       (green, 3))

#define LUMEX_HYGIENE_ONE_STRING(ENTRY) ENTRY (lone, "l")
LUMEX_DEFINE_REFLECTED_ENUM_TO_STRING (lone_t, std::uint8_t,
                                       LUMEX_HYGIENE_ONE_STRING, (lone))

struct owner_t
{
  LUMEX_DEFINE_REFLECTED_ENUM (inner_t, int, (x), (y, 2))
};

LUMEX_DEFINE_REFLECTED_ENUM_STORAGE (owner_t, inner_t)

LUMEX_DEFINE_EXCEPTION (plain_error_t, std::runtime_error)
LUMEX_DEFINE_EXCEPTION_WITH_BODY (
    coded_error_t, std::runtime_error, int code () const { return 7; })

std::size_t
reflected_enum_sizes ()
{
  return single_tSize + single_valued_tSize + pair_tSize + mixed_tSize
         + sixteen_tSize + colour_tSize + lone_tSize + owner_t::inner_tSize;
}

char const *
reflected_enum_names ()
{
  return to_string (single_t::only);
}

int
measured_work ()
{
  int counter = 0;
  LUMEX_MEASURE_TIME (++counter);
  LUMEX_MEASURE_TIME (++counter, "second increment");
  return counter;
}
