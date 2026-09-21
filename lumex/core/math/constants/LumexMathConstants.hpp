/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LUMEX_CORE_MATH_CONSTANTS_HPP
#define LUMEX_CORE_MATH_CONSTANTS_HPP

/// @link https://en.wikipedia.org/wiki/List_of_mathematical_constants
/// @link https://calculla.com/math_constants
/// @link http://www.numberworld.org/digits/

// 50 digits precision
#define LUMEX_MATH_CONSTANTS_PI                                               \
  3.14159265358979323846264338327950288419716939937510
#define LUMEX_MATH_CONSTANTS_EULER_NUMBER                                     \
  2.71828182845904523536028747135266249775724709369995
#define LUMEX_MATH_CONSTANTS_GOLDEN_RATIO                                     \
  1.61803398874989484820458683436563811772030917980576
#define LUMEX_MATH_CONSTANTS_SILVER_RATIO                                     \
  2.41421356237309504880168872420969807856967187537694
#define LUMEX_MATH_CONSTANTS_EULER_MASCHERONI                                 \
  0.57721566490153286060651209008240243104215933593992
#define LUMEX_MATH_CONSTANTS_SQRT_2                                           \
  1.41421356237309504880168872420969807856967187537694
#define LUMEX_MATH_CONSTANTS_SQRT_3                                           \
  1.73205080756887729352744634150587236694280525381038
#define LUMEX_MATH_CONSTANTS_SQRT_5                                           \
  2.23606797749978969640917366873127623544061835960414
#define LUMEX_MATH_CONSTANTS_LN_2                                             \
  0.69314718055994530941723212145817656807550013436025
#define LUMEX_MATH_CONSTANTS_LN_10                                            \
  2.30258509299404568401799145468436420760110148862877
#define LUMEX_MATH_CONSTANTS_APERY                                            \
  1.20205690315959428539973816151144999076498629234049
#define LUMEX_MATH_CONSTANTS_CATALAN                                          \
  0.91596559417721901505460351493238411077414937428167
#define LUMEX_MATH_CONSTANTS_LEMMISCATE                                       \
  5.24411510858423962092967917978223882736550990286324
#define LUMEX_MATH_CONSTANTS_GAMMA_1_4                                        \
  3.62560990822190831193068515586767200299516768288006
#define LUMEX_MATH_CONSTANTS_GAMMA_1_3                                        \
  2.67893853470774763365569294097467764412868937795730
#define LUMEX_MATH_CONSTANTS_BRUNS_CONSTANT_TWIN_PRIMES                       \
  1.902160583104 // 12 digits from calculla, no 50 digit source easily
                 // available
#define LUMEX_MATH_CONSTANTS_PLASTIC_NUMBER                                   \
  1.32471795724474602596090885447809734073440794569502
#define LUMEX_MATH_CONSTANTS_RECIPROCAL_PI                                    \
  0.31830988618379067153776752674502872406891929148091
#define LUMEX_MATH_CONSTANTS_SQRT_PI                                          \
  1.77245385090551602729816748334114518279754945612239
#define LUMEX_MATH_CONSTANTS_INVERSE_SQRT_PI                                  \
  0.56418958354775628694807945156077258584405905105952
#define LUMEX_MATH_CONSTANTS_COPERNICUS_CONSTANT                              \
  0.01745329251994329576922222222222222222222222222222 // (pi/180)
#define LUMEX_MATH_CONSTANTS_GRAVITY_ACCELERATION                             \
  9.80665000000000000000000000000000000000000000000000 // Standard Gravity on
                                                       // Earth (m/s^2)
#define LUMEX_MATH_CONSTANTS_LIGHT_SPEED                                      \
  299792458.000000000000000000000000000000000000000000 // Speed of Light in
                                                       // Vacuum (m/s)
#define LUMEX_MATH_CONSTANTS_PLANCK_CONSTANT                                  \
  6.62607015000000000000000000000000000000000000000000 // Planck Constant (J.s)
#define LUMEX_MATH_CONSTANTS_AVOGADRO_CONSTANT                                \
  6.02214076000000000000000000000000000000000000000000 // Avogadro Constant
                                                       // (mol^-1)

#endif // !LUMEX_CORE_MATH_CONSTANTS_HPP
