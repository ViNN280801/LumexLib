// Compile checks of the LUMEX_MATH_CONSTANTS_* macros, built by
// cmake.math_constants_compile_checks. Exactly one of
// LUMEX_MATH_CONSTANTS_GOOD_CASE or LUMEX_MATH_CONSTANTS_BAD_CASE=<n> is
// defined; the fixture compiles with warnings as errors. A constant is a
// double literal, so cases 1-5 are the uses a literal rejects; case 6 is a
// floating constant in an #if (the preprocessor has integers only); case 7 a
// suffix a floating literal does not have; cases 8 and 9 compare the double
// macro with a float and a long double literal of the same digits, which are
// different numbers.

#include "lumex/core/math/constants/LumexMathConstants.hpp"

#define LUMEX_CHECK_PASTE_IMPL(a, b) a##b
#define LUMEX_CHECK_PASTE(a, b) LUMEX_CHECK_PASTE_IMPL (a, b)

int
main ()
{
  int used = 0;

#if defined(LUMEX_MATH_CONSTANTS_GOOD_CASE)
  // The uses each bad case is the broken version of.
  double value = LUMEX_MATH_CONSTANTS_PI;
  double const &reference = LUMEX_MATH_CONSTANTS_PI;
  double const *pointer = &reference;
  float single = LUMEX_CHECK_PASTE (LUMEX_MATH_CONSTANTS_PI, F);
  long double wide = LUMEX_CHECK_PASTE (LUMEX_MATH_CONSTANTS_PI, L);
  int truncated = static_cast<int> (LUMEX_MATH_CONSTANTS_PI);
  long long speed = static_cast<long long> (LUMEX_MATH_CONSTANTS_LIGHT_SPEED);
  long long remainder = speed % 7;
  static_assert (LUMEX_MATH_CONSTANTS_PI == 3.141592653589793,
                 "the double macro is the double 3.141592653589793");
  used += value > 3.0 ? 1 : 0;
  used += *pointer > 3.0 ? 1 : 0;
  used += single > 3.0f ? 1 : 0;
  used += wide > 3.0L ? 1 : 0;
  used += truncated + static_cast<int> (remainder);
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 1
  // A constant is a prvalue: it has no address.
  double const *pointer = &LUMEX_MATH_CONSTANTS_PI;
  used += *pointer > 3.0 ? 1 : 0;
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 2
  // It does not bind to a non-const lvalue reference.
  double &reference = LUMEX_MATH_CONSTANTS_PI;
  used += reference > 3.0 ? 1 : 0;
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 3
  // It is not assignable.
  LUMEX_MATH_CONSTANTS_PI = 3.0;
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 4
  // It is a floating number, so there is no modulo.
  used += static_cast<int> (LUMEX_MATH_CONSTANTS_LIGHT_SPEED % 7);
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 5
  // Braces refuse the narrowing of a floating constant to an integer.
  int truncated{ LUMEX_MATH_CONSTANTS_PI };
  used += truncated;
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 6
  // The preprocessor evaluates integers only.
#if LUMEX_MATH_CONSTANTS_PI > 3
  used += 1;
#endif
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 7
  // Not a floating suffix: LL does not exist on a floating literal.
  used += LUMEX_CHECK_PASTE (LUMEX_MATH_CONSTANTS_PI, LL) > 3 ? 1 : 0;
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 8
  // The float literal of the same digits is a different number.
  static_assert (LUMEX_MATH_CONSTANTS_PI
                     == LUMEX_CHECK_PASTE (LUMEX_MATH_CONSTANTS_PI, F),
                 "float and double literals are different numbers");
#elif LUMEX_MATH_CONSTANTS_BAD_CASE == 9
  // The same for the long double literal.
  static_assert (static_cast<long double> (LUMEX_MATH_CONSTANTS_PI)
                     == LUMEX_CHECK_PASTE (LUMEX_MATH_CONSTANTS_PI, L),
                 "double and long double literals are different numbers");
#else
#error "LUMEX_MATH_CONSTANTS_BAD_CASE is not one of the cases"
#endif
  return used == -1;
}
