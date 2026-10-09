/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LUMEX_TESTS_CORE_MATH_CONSTANTS_REFERENCE_HPP
#define LUMEX_TESTS_CORE_MATH_CONSTANTS_REFERENCE_HPP

// Reference values of the LUMEX_MATH_CONSTANTS_* macros for the tests of
// lumex/core/math/constants, which share them (digits, values, relations).
//
// Source of the numbers: LumexMathConstantsReference.py in this directory.
// It computes every constant with the python `decimal` module at 140 digits
// from a formula that does not use the header (Machin and Gauss-Legendre for
// pi, the series of e, Euler-Maclaurin for Euler-Mascheroni, Apery's series,
// a fast Catalan series, the AGM for the lemniscate constant, Stirling's
// series for the Gamma values, Newton for the plastic number). The exact
// physical constants are the SI 2019 definitions and the standard gravity of
// the CGPM (1901); Brun's constant is the published 1.902160583104 (OEIS
// A065421), which cannot be recomputed here. Re-run the script to regenerate
// the table below; with the header's path it also reports a wrong digit.
//
// One row per macro, X (NAME, DIGITS, DECIMALS, FM, FE, DM, DE, LM, LE):
//   NAME      the macro without the prefix LUMEX_MATH_CONSTANTS_
//   DIGITS    the true value, 60 decimals (truncated; an exact value as is,
//             Planck and Avogadro as mantissa 'e' exponent)
//   DECIMALS  how many decimals the header must carry (50, Brun 12; the
//             exact values 0: they are compared as numbers)
//   FM, FE    the correctly rounded float, FM * 2^FE (mantissa < 2^24)
//   DM, DE    the correctly rounded double (mantissa < 2^53)
//   LM, LE    the correctly rounded x87 long double (mantissa < 2^64)

#include <cstddef>

#define LUMEX_TEST_MATH_CONSTANTS_COUNT 25

#define LUMEX_TEST_MATH_CONSTANTS_REFERENCE(X)                                \
  X (PI, "3.141592653589793238462643383279502884197169399375105820974944",    \
     50, 13176795ULL, -22, 884279719003555ULL, -48, 14488038916154245685ULL,  \
     -62)                                                                     \
  X (EULER_NUMBER,                                                            \
     "2.718281828459045235360287471352662497757247093699959574966967", 50,    \
     2850325ULL, -20, 6121026514868073ULL, -51, 12535862302449814171ULL, -62) \
  X (GOLDEN_RATIO,                                                            \
     "1.618033988749894848204586834365638117720309179805762862135448", 50,    \
     13573053ULL, -23, 910872158600853ULL, -49, 14923729446516375051ULL, -63) \
  X (SILVER_RATIO,                                                            \
     "2.414213562373095048801688724209698078569671875376948073176679", 50,    \
     5062973ULL, -21, 2718162824974067ULL, -50, 5566797465546889505ULL, -61)  \
  X (EULER_MASCHERONI,                                                        \
     "0.577215664901532860606512090082402431042159335939923598805767", 50,    \
     1210509ULL, -21, 5199096506725913ULL, -53, 10647749645774669733ULL, -64) \
  X (SQRT_2,                                                                  \
     "1.414213562373095048801688724209698078569671875376948073176679", 50,    \
     11863283ULL, -23, 6369051672525773ULL, -52, 3260954456333195553ULL, -61) \
  X (SQRT_3,                                                                  \
     "1.732050807568877293527446341505872366942805253810380628055806", 50,    \
     14529495ULL, -23, 3900231685776981ULL, -51, 7987674492471257551ULL, -62) \
  X (SQRT_5,                                                                  \
     "2.236067977499789696409173668731276235440618359611525724270897", 50,    \
     9378749ULL, -22, 629397181890197ULL, -48, 10312043428088987147ULL, -62)  \
  X (LN_2, "0.693147180559945309417232121458176568075500134360255254120680",  \
     50, 1453635ULL, -21, 6243314768165359ULL, -53, 3196577161300663915ULL,   \
     -62)                                                                     \
  X (LN_10, "2.302585092994045684017991454684364207601101488628772976033327", \
     50, 4828871ULL, -21, 2592480341699211ULL, -50, 10618799479599967255ULL,  \
     -62)                                                                     \
  X (APERY, "1.202056903159594285399738161511449990764986292340498881792271", \
     50, 39389ULL, -15, 5413583021147681ULL, -52, 5543509013655225569ULL,     \
     -62)                                                                     \
  X (CATALAN,                                                                 \
     "0.915965594177219015054603514932384110774149374281672134266498", 50,    \
     15367353ULL, -24, 8250284617241437ULL, -53, 16896582896110463045ULL,     \
     -64)                                                                     \
  X (LEMNISCATE,                                                              \
     "5.244115108584239620929679179782238827365509902863246325633643", 50,    \
     5498853ULL, -20, 369021794514187ULL, -46, 3023026540660220125ULL, -59)   \
  X (GAMMA_1_4,                                                               \
     "3.625609908221908311930685155867672002995167682880065467433377", 50,    \
     7603455ULL, -21, 4082073857914741ULL, -50, 16720174522018779623ULL, -62) \
  X (GAMMA_1_3,                                                               \
     "2.678938534707747633655692940974677644128689377957301100950428", 50,    \
     11236283ULL, -22, 6032433293329137ULL, -51, 3088605846184518351ULL, -60) \
  X (BRUNS_CONSTANT_TWIN_PRIMES,                                              \
     "1.902160583104000000000000000000000000000000000000000000000000", 12,    \
     15956479ULL, -23, 2141642423316505ULL, -50, 17544334731808808529ULL,     \
     -63)                                                                     \
  X (PLASTIC_NUMBER,                                                          \
     "1.324717957244746025960908854478097340734404056901733364534015", 50,    \
     2778135ULL, -21, 5965999298618443ULL, -52, 6109183281785285483ULL, -62)  \
  X (RECIPROCAL_PI,                                                           \
     "0.318309886183790671537767526745028724068919291480912897495334", 50,    \
     10680707ULL, -25, 5734161139222659ULL, -54, 5871781006564002453ULL, -64) \
  X (SQRT_PI,                                                                 \
     "1.772453850905516027298167483341145182797549456122387128213807", 50,    \
     14868421ULL, -23, 7982422502469483ULL, -52, 16348001285057500477ULL,     \
     -63)                                                                     \
  X (INVERSE_SQRT_PI,                                                         \
     "0.564189583547756286948079451560772585844050629328998856844085", 50,    \
     9465531ULL, -24, 5081767996463981ULL, -53, 10407460856758233229ULL, -64) \
  X (COPERNICUS_CONSTANT,                                                     \
     "0.017453292519943295769236907684886127134428718885417254560971", 50,    \
     9370165ULL, -29, 5030569068109113ULL, -58, 5151302725743731799ULL, -68)  \
  X (GRAVITY_ACCELERATION, "9.80665", 0, 5141509ULL, -19,                     \
     5520653160719109ULL, -49, 11306297673152735897ULL, -60)                  \
  X (LIGHT_SPEED, "299792458.0", 0, 4684257ULL, 6, 149896229ULL, 1,           \
     149896229ULL, 1)                                                         \
  X (PLANCK_CONSTANT, "6.62607015e-34", 0, 14430303ULL, -134,                 \
     7747209898635537ULL, -163, 1983285734050697523ULL, -171)                 \
  X (AVOGADRO_CONSTANT, "6.02214076e23", 0, 8357399ULL, 56,                   \
     8973689019680023ULL, 26, 4594528778076171875ULL, 17)

// The text of a macro after its expansion: "3.14159...", the exact token.
#define LUMEX_TEST_STRINGIZE_IMPL(x) #x
#define LUMEX_TEST_STRINGIZE(x) LUMEX_TEST_STRINGIZE_IMPL (x)

// The literal of a macro with a type suffix: the macro is a double literal,
// so a float or long double with all its digits needs the suffix pasted on
// the expanded token (3.14...F, 3.14...L).
#define LUMEX_TEST_PASTE_IMPL(a, b) a##b
#define LUMEX_TEST_PASTE(a, b) LUMEX_TEST_PASTE_IMPL (a, b)
#define LUMEX_TEST_FLOAT_LITERAL(x) LUMEX_TEST_PASTE (x, F)
#define LUMEX_TEST_LONG_DOUBLE_LITERAL(x) LUMEX_TEST_PASTE (x, L)

namespace lumex_math_constants_test
{
/// 2^e, exactly, in a C++11 constant expression.
template <typename T>
constexpr T
pow2 (int e)
{
  return e == 0  ? static_cast<T> (1)
         : e > 0 ? static_cast<T> (2) * pow2<T> (e - 1)
                 : pow2<T> (e + 1) / static_cast<T> (2);
}

/// mantissa * 2^exponent, exact when the mantissa fits the type.
template <typename T>
constexpr T
make (unsigned long long mantissa, int exponent)
{
  return static_cast<T> (mantissa) * pow2<T> (exponent);
}
} // namespace lumex_math_constants_test

#endif // !LUMEX_TESTS_CORE_MATH_CONSTANTS_REFERENCE_HPP
