// lumex/tests/core/math/constants/LumexMathConstantsDigits.cxx11.tests.cpp
//
// The decimal digits of every LUMEX_MATH_CONSTANTS_* macro against a
// reference computed outside the library (LumexMathConstantsReference.hpp:
// python `decimal`, 140 digits, formulas that do not use the header). The
// macros are plain decimal literals, so the text of the expanded macro is the
// whole of what the library promises: the test reads that text and compares
// it digit by digit, which also catches a wrong digit far beyond the 17 that
// a double keeps. It works from C++11, so the suite of every standard runs it.
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/math/constants/LumexMathConstants.hpp"

#include "LumexMathConstantsReference.hpp"

namespace
{
struct digits_row
{
  char const *name;
  std::string token;     // the text of the expanded macro
  std::string reference; // the true value, 60 decimals
  int decimals;          // how many decimals the header must carry
};

#define LUMEX_TEST_DIGITS_ROW(NAME, DIGITS, DECIMALS, FM, FE, DM, DE, LM, LE) \
  { #NAME, LUMEX_TEST_STRINGIZE (LUMEX_MATH_CONSTANTS_##NAME), DIGITS,        \
    DECIMALS },

std::vector<digits_row>
make_rows ()
{
  std::vector<digits_row> rows;
  digits_row const table[]
      = { LUMEX_TEST_MATH_CONSTANTS_REFERENCE (LUMEX_TEST_DIGITS_ROW) };
  rows.assign (table, table + sizeof (table) / sizeof (table[0]));
  return rows;
}

/// The mantissa of "6.02e23" ("6.02"): the text before the exponent.
std::string
mantissa_of (std::string const &text)
{
  return text.substr (0, text.find_first_of ("eE"));
}

/// The exponent of "6.02e+23" ("23"), empty without one; a plus sign and
/// leading zeros are not part of the number.
std::string
exponent_of (std::string const &text)
{
  std::size_t const e = text.find_first_of ("eE");
  if (e == std::string::npos)
    return std::string ();
  std::string result = text.substr (e + 1);
  if (!result.empty () && result[0] == '+')
    result.erase (0, 1);
  return result;
}

/// The number of decimals of "123.4560", or npos without a point.
std::size_t
decimals_of (std::string const &text)
{
  std::size_t const point = text.find ('.');
  return point == std::string::npos ? std::string::npos
                                    : text.size () - point - 1;
}

/// The text cut to `decimals` decimals (a cut, not a rounding).
std::string
truncate_to (std::string const &text, std::size_t decimals)
{
  return text.substr (0, text.find ('.') + 1 + decimals);
}

/// The same cut with the last digit rounded up when the next digit is 5 or
/// more ("3.1415|9" -> "3.1416"), carrying through nines.
std::string
round_to (std::string const &text, std::size_t decimals)
{
  std::string result = truncate_to (text, decimals);
  std::size_t const next = text.find ('.') + 1 + decimals;
  if (next >= text.size () || text[next] < '5')
    return result;
  for (std::size_t i = result.size (); i-- > 0;)
    {
      if (result[i] == '.')
        continue;
      if (result[i] != '9')
        {
          ++result[i];
          return result;
        }
      result[i] = '0';
    }
  return "1" + result;
}

/// "123.456" or "123.456e-7": digits with one point, then at most an exponent
/// of an optional sign and digits.
bool
is_plain_decimal (std::string const &token)
{
  std::string const mantissa = mantissa_of (token);
  std::string const exponent = exponent_of (token);
  std::size_t points = 0;
  for (std::size_t i = 0; i < mantissa.size (); ++i)
    {
      if (mantissa[i] == '.')
        ++points;
      else if (mantissa[i] < '0' || mantissa[i] > '9')
        return false;
    }
  if (points != 1 || mantissa[0] == '.'
      || mantissa[mantissa.size () - 1] == '.')
    return false;
  if (mantissa.size () == token.size ())
    return true;
  std::size_t first = !exponent.empty () && exponent[0] == '-' ? 1 : 0;
  if (exponent.size () <= first)
    return false;
  for (std::size_t i = first; i < exponent.size (); ++i)
    if (exponent[i] < '0' || exponent[i] > '9')
      return false;
  return true;
}
} // namespace

TEST (MathConstantsDigitsTest,
      GivenReferenceTable_WhenCounted_ThenEveryMacroHasARow)
{
  EXPECT_EQ (make_rows ().size (),
             static_cast<std::size_t> (LUMEX_TEST_MATH_CONSTANTS_COUNT));
}

TEST (MathConstantsDigitsTest,
      GivenEveryMacro_WhenExpanded_ThenItIsOnePlainDecimalLiteral)
{
  // No suffix, sign, parenthesis or operator (an exponent only where the
  // value needs one): a single token that cannot change meaning next to an
  // operator (-PI, 1 / PI, 2 * PI).
  for (digits_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      EXPECT_TRUE (is_plain_decimal (row.token)) << row.token;
    }
}

TEST (MathConstantsDigitsTest,
      GivenEveryMacro_WhenExpanded_ThenItCarriesTheDocumentedDigits)
{
  // "50 digits precision" of the header: 50 decimals for the computed
  // constants, the 12 published digits of Brun's constant, and the exact
  // physical constants with however many zeros.
  for (digits_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      std::string const mantissa = mantissa_of (row.token);
      ASSERT_NE (decimals_of (mantissa), std::string::npos) << row.token;
      EXPECT_GE (decimals_of (mantissa),
                 static_cast<std::size_t> (row.decimals))
          << row.token;
    }
}

TEST (MathConstantsDigitsTest,
      GivenEveryMacro_WhenComparedWithTheTrueValue_ThenEveryDigitIsRight)
{
  // The header may cut the true value or round it at its last digit; any
  // other digit is wrong. A value with more decimals than the 60 of the
  // table is padded with zeros (the exact physical constants). The exponent
  // of a constant that has one (Planck, Avogadro) must be equal.
  for (digits_row const &row : make_rows ())
    {
      SCOPED_TRACE (row.name);
      std::string const token = mantissa_of (row.token);
      std::string const reference = mantissa_of (row.reference);
      std::size_t const decimals = decimals_of (token);
      ASSERT_NE (decimals, std::string::npos) << row.token;
      std::string const padded = reference + std::string (decimals + 1, '0');
      EXPECT_TRUE (token == truncate_to (padded, decimals)
                   || token == round_to (padded, decimals))
          << "header   " << row.token << "\ntruncated "
          << truncate_to (padded, decimals) << "\nrounded   "
          << round_to (padded, decimals);
      EXPECT_EQ (exponent_of (row.token), exponent_of (row.reference));
    }
}

TEST (MathConstantsDigitsTest, GivenAWrongDigit_WhenCompared_ThenItIsReported)
{
  // The comparison itself must be able to fail: one digit changed far from
  // the front, a digit dropped, a digit added and a rounding that is not one.
  std::string const pi
      = "3.14159265358979323846264338327950288419716939937510";
  std::string const reference
      = "3.141592653589793238462643383279502884197169399375105820974944";
  ASSERT_EQ (pi, truncate_to (reference, 50));

  std::string changed = pi;
  changed[40] = changed[40] == '7' ? '8' : '7';
  EXPECT_NE (changed, truncate_to (reference, 50));
  EXPECT_NE (changed, round_to (reference, 50));
  EXPECT_NE (pi.substr (0, pi.size () - 1), truncate_to (reference, 50));
  EXPECT_NE (pi + "6", truncate_to (reference, 51));

  // 3.14159|5 rounds up, 3.14159|4 does not, 9.9|9 carries into a new digit.
  EXPECT_EQ (round_to ("3.141595", 5), "3.14160");
  EXPECT_EQ (round_to ("3.141594", 5), "3.14159");
  EXPECT_EQ (round_to ("9.99", 1), "10.0");
  EXPECT_EQ (truncate_to ("9.99", 1), "9.9");
}

TEST (MathConstantsDigitsTest,
      GivenPhysicalConstants_WhenRead_ThenTheyCarryTheirExponent)
{
  // SI 2019 definitions: the Planck constant is 6.62607015e-34 J s and the
  // Avogadro constant 6.02214076e23 1/mol, the macros hold the whole number
  // (before 2.0.0.0 they held only the mantissa).
  std::string const planck
      = LUMEX_TEST_STRINGIZE (LUMEX_MATH_CONSTANTS_PLANCK_CONSTANT);
  std::string const avogadro
      = LUMEX_TEST_STRINGIZE (LUMEX_MATH_CONSTANTS_AVOGADRO_CONSTANT);
  EXPECT_EQ (planck, "6.62607015e-34");
  EXPECT_EQ (avogadro, "6.02214076e23");
  EXPECT_DOUBLE_EQ (LUMEX_MATH_CONSTANTS_PLANCK_CONSTANT, 6.62607015e-34);
  EXPECT_DOUBLE_EQ (LUMEX_MATH_CONSTANTS_AVOGADRO_CONSTANT, 6.02214076e23);
}

TEST (MathConstantsDigitsTest, GivenAnExponent_WhenParsed_ThenItIsSplitOff)
{
  // The text helpers behind the checks above, and the shapes they refuse.
  EXPECT_EQ (mantissa_of ("6.02e+23"), "6.02");
  EXPECT_EQ (exponent_of ("6.02e+23"), "23");
  EXPECT_EQ (exponent_of ("6.6e-34"), "-34");
  EXPECT_EQ (exponent_of ("6.6"), "");
  EXPECT_TRUE (is_plain_decimal ("6.62607015e-34"));
  EXPECT_TRUE (is_plain_decimal ("6.02214076e23"));
  EXPECT_FALSE (is_plain_decimal ("6.02e"));
  EXPECT_FALSE (is_plain_decimal ("6.02e-"));
  EXPECT_FALSE (is_plain_decimal ("6.02e2x"));
  EXPECT_FALSE (is_plain_decimal ("6.02F"));
  EXPECT_FALSE (is_plain_decimal ("(6.02)"));
  EXPECT_FALSE (is_plain_decimal ("602"));
}
