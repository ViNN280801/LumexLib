// lumex/tests/core/math/constants/LumexMathConstantsValues.cxx11.tests.cpp
//
// The value of every LUMEX_MATH_CONSTANTS_* macro as a float, a double and a
// long double against the correctly rounded reference of
// LumexMathConstantsReference.hpp (mantissa * 2^exponent, computed outside the
// library), and its use in constant expressions. The macros are double
// literals: the double is the nearest double to the true value, a float
// variable gets the nearest float, and a long double with all of its digits
// needs the suffix pasted on the expanded token
// (LUMEX_TEST_LONG_DOUBLE_LITERAL builds 3.14...L). The header defines macros
// and no variable, so there is nothing to ODR-use: a constant is a prvalue
// that binds to a const reference and has no address. It works from C++11, so
// the suite of every standard runs this file (the static_asserts below are the
// constant-expression check at each standard).
#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/math/constants/LumexMathConstants.hpp"

#include "LumexMathConstantsReference.hpp"

using lumex_math_constants_test::make;

// --- Constant expressions, at the standard of the suite -------------------

#define LUMEX_TEST_STATIC_VALUES(NAME, DIGITS, DECIMALS, FM, FE, DM, DE, LM,  \
                                 LE)                                          \
  static_assert (                                                             \
      std::is_same<decltype (LUMEX_MATH_CONSTANTS_##NAME), double>::value,    \
      #NAME " is a double literal");                                          \
  static_assert (                                                             \
      std::is_same<decltype ((LUMEX_MATH_CONSTANTS_##NAME)), double>::value,  \
      #NAME " is a prvalue, not a reference");                                \
  static_assert (LUMEX_MATH_CONSTANTS_##NAME == make<double> (DM, DE),        \
                 #NAME " is not the nearest double");                         \
  static_assert (static_cast<float> (LUMEX_MATH_CONSTANTS_##NAME)             \
                     == make<float> (FM, FE),                                 \
                 #NAME " as a float is not the nearest float");               \
  static_assert (LUMEX_TEST_FLOAT_LITERAL (LUMEX_MATH_CONSTANTS_##NAME)       \
                     == make<float> (FM, FE),                                 \
                 #NAME "F is not the nearest float");                         \
  static_assert (                                                             \
      std::numeric_limits<long double>::digits != 64                          \
          || LUMEX_TEST_LONG_DOUBLE_LITERAL (LUMEX_MATH_CONSTANTS_##NAME)     \
                 == make<long double> (LM, LE),                               \
      #NAME "L is not the nearest x87 long double");

LUMEX_TEST_MATH_CONSTANTS_REFERENCE (LUMEX_TEST_STATIC_VALUES)

namespace
{
constexpr double
circumference (double radius)
{
  return 2.0 * LUMEX_MATH_CONSTANTS_PI * radius;
}

constexpr double kCircumferenceOfOne = circumference (1.0);
static_assert (kCircumferenceOfOne == 2.0 * LUMEX_MATH_CONSTANTS_PI,
               "a macro works in a constexpr function");

constexpr double kGoldenSquare
    = LUMEX_MATH_CONSTANTS_GOLDEN_RATIO * LUMEX_MATH_CONSTANTS_GOLDEN_RATIO;
static_assert (kGoldenSquare > 2.618 && kGoldenSquare < 2.6181,
               "a constexpr variable is built from macros");

// An integral constant expression built from a macro: an array bound and a
// template argument (a cast of a floating constant is a constant expression).
using array_of_pi_hundredths
    = std::array<char,
                 static_cast<std::size_t> (LUMEX_MATH_CONSTANTS_PI * 100)>;
static_assert (sizeof (array_of_pi_hundredths) == 314, "an array bound");
static_assert (
    std::integral_constant<int, static_cast<int> (LUMEX_MATH_CONSTANTS_SQRT_2
                                                  * 1000)>::value
        == 1414,
    "a template argument");

// A literal that is negated, divided or compared in a constant expression.
static_assert (-LUMEX_MATH_CONSTANTS_PI < 0.0, "unary minus");
static_assert (1.0 / LUMEX_MATH_CONSTANTS_RECIPROCAL_PI
                   > LUMEX_MATH_CONSTANTS_PI - 1e-15,
               "a quotient");
static_assert (static_cast<long long> (LUMEX_MATH_CONSTANTS_LIGHT_SPEED)
                   == 299792458LL,
               "the speed of light is an integral number of m/s");

struct value_row
{
  char const *name;
  double macro;
  float macro_as_float;
  float float_literal;
  long double macro_as_long_double;
  long double long_double_literal;
  double reference_double;
  float reference_float;
  long double reference_long_double;
};

#define LUMEX_TEST_VALUE_ROW(NAME, DIGITS, DECIMALS, FM, FE, DM, DE, LM, LE)  \
  { #NAME,                                                                    \
    LUMEX_MATH_CONSTANTS_##NAME,                                              \
    static_cast<float> (LUMEX_MATH_CONSTANTS_##NAME),                         \
    LUMEX_TEST_FLOAT_LITERAL (LUMEX_MATH_CONSTANTS_##NAME),                   \
    static_cast<long double> (LUMEX_MATH_CONSTANTS_##NAME),                   \
    LUMEX_TEST_LONG_DOUBLE_LITERAL (LUMEX_MATH_CONSTANTS_##NAME),             \
    make<double> (DM, DE),                                                    \
    make<float> (FM, FE),                                                     \
    make<long double> (LM, LE) },

std::vector<value_row>
make_rows ()
{
  value_row const table[]
      = { LUMEX_TEST_MATH_CONSTANTS_REFERENCE (LUMEX_TEST_VALUE_ROW) };
  return std::vector<value_row> (table,
                                 table + sizeof (table) / sizeof (table[0]));
}

bool
long_double_is_x87 ()
{
  return std::numeric_limits<long double>::digits == 64;
}
} // namespace

TEST (MathConstantsValuesTest,
      GivenReferenceTable_WhenCounted_ThenEveryMacroHasARow)
{
  EXPECT_EQ (make_rows ().size (),
             static_cast<std::size_t> (LUMEX_TEST_MATH_CONSTANTS_COUNT));
}

TEST (MathConstantsValuesTest,
      GivenDouble_WhenCompared_ThenItIsTheNearestDouble)
{
  for (value_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      EXPECT_EQ (row.macro, row.reference_double);
      EXPECT_TRUE (row.macro > 0.0);
      EXPECT_TRUE (row.macro <= (std::numeric_limits<double>::max) ());
    }
}

TEST (MathConstantsValuesTest, GivenFloat_WhenCompared_ThenItIsTheNearestFloat)
{
  for (value_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      // A float variable initialised from the double macro, and the float
      // literal with all the digits: both are the nearest float.
      EXPECT_EQ (row.macro_as_float, row.reference_float);
      EXPECT_EQ (row.float_literal, row.reference_float);
    }
}

TEST (MathConstantsValuesTest,
      GivenLongDouble_WhenCompared_ThenTheSuffixKeepsAllTheDigits)
{
  if (!long_double_is_x87 ())
    GTEST_SKIP () << "long double has "
                  << std::numeric_limits<long double>::digits
                  << " mantissa bits, the reference is the 64 bit x87";
  for (value_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      EXPECT_EQ (row.long_double_literal, row.reference_long_double);
    }
}

TEST (MathConstantsValuesTest,
      GivenLongDouble_WhenTakenFromThePlainMacro_ThenItHasDoublePrecision)
{
  // The macro is a double literal: a long double variable initialised from it
  // is that double, widened. Only the pasted suffix gives more digits, and
  // only where the double is not already exact (the speed of light is).
  for (value_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      EXPECT_EQ (row.macro_as_long_double,
                 static_cast<long double> (row.reference_double));
      if (long_double_is_x87 ())
        {
          bool const double_is_exact
              = row.reference_long_double
                == static_cast<long double> (row.reference_double);
          EXPECT_EQ (row.macro_as_long_double == row.long_double_literal,
                     double_is_exact);
        }
    }
}

TEST (MathConstantsValuesTest,
      GivenFiniteLiteralOfEachType_WhenCompared_ThenTheTypesAgree)
{
  // The three precisions describe one number: float and long double stay
  // within one float / one double step of the double.
  for (value_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      double const as_float = static_cast<double> (row.float_literal);
      EXPECT_LE (
          as_float > row.macro ? as_float - row.macro : row.macro - as_float,
          row.macro
              * static_cast<double> (std::numeric_limits<float>::epsilon ()));
      long double const widened = static_cast<long double> (row.macro);
      long double const difference = widened > row.long_double_literal
                                         ? widened - row.long_double_literal
                                         : row.long_double_literal - widened;
      EXPECT_LE (difference, row.long_double_literal
                                 * static_cast<long double> (
                                     std::numeric_limits<double>::epsilon ()));
    }
}

TEST (MathConstantsValuesTest,
      GivenAConstant_WhenBoundToAReference_ThenItIsACopyOfTheValue)
{
  // A prvalue: it binds to a const reference (a temporary) and to a value
  // parameter; two bindings are two objects.
  double const &first = LUMEX_MATH_CONSTANTS_PI;
  double const &second = LUMEX_MATH_CONSTANTS_PI;
  EXPECT_EQ (first, second);
  EXPECT_NE (&first, &second);
  EXPECT_EQ (first, LUMEX_MATH_CONSTANTS_PI);
}

TEST (MathConstantsValuesTest,
      GivenConstexprVariables_WhenRead_ThenTheyMatchTheMacros)
{
  EXPECT_EQ (kCircumferenceOfOne, 2.0 * LUMEX_MATH_CONSTANTS_PI);
  EXPECT_NEAR (kGoldenSquare, LUMEX_MATH_CONSTANTS_GOLDEN_RATIO + 1.0, 1e-15);
  EXPECT_EQ (sizeof (array_of_pi_hundredths), 314u);
}

TEST (MathConstantsValuesTest,
      GivenWrongReference_WhenCompared_ThenTheComparisonFails)
{
  // The comparisons above must be able to fail: one unit in the last place
  // of the double and of the float is not the same number.
  value_row const row = make_rows ().front ();
  double const next_double
      = row.reference_double * (1.0 + std::numeric_limits<double>::epsilon ());
  EXPECT_NE (row.macro, next_double);
  float const next_float
      = row.reference_float * (1.0f + std::numeric_limits<float>::epsilon ());
  EXPECT_NE (row.float_literal, next_float);
  EXPECT_NE (make<double> (884279719003555ULL, -48),
             make<double> (884279719003556ULL, -48));
}
