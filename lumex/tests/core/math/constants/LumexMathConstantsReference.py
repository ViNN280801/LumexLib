#!/usr/bin/env python3
"""Reference values of the LUMEX_MATH_CONSTANTS_* macros.

Prints the LUMEX_TEST_MATH_CONSTANTS_REFERENCE table of
LumexMathConstantsReference.hpp: for every macro the 60-decimal true value and
the correctly rounded (nearest, ties to even) float, double and x87 long
double as mantissa * 2^exponent. Every value is computed here with the python
`decimal` module at 140 digits from a formula that does not use the header
(mpmath is not needed; python 3.7 is enough):

  pi        Machin (16 atan 1/5 - 4 atan 1/239), cross-checked by Gauss-Legendre
  e         sum of 1/n!
  sqrt, ln  Decimal.sqrt, Decimal.ln (correctly rounded)
  gamma     Euler-Maclaurin summation of the harmonic numbers (N = 1000)
  zeta(3)   Apery: 5/2 sum (-1)^(n-1) / (n^3 C(2n, n))
  Catalan   pi/8 ln(2 + sqrt 3) + 3/8 sum 1 / ((2n + 1)^2 C(2n, n))
  2 varpi   2 pi / AGM(1, sqrt 2)  (the lemniscate constant, as in the header)
  Gamma(x)  Stirling series at x + 200 and the recurrence back; cross-checked
            by varpi = Gamma(1/4)^2 / (2 sqrt(2 pi)) and by
            Gamma(1/3) Gamma(2/3) = 2 pi / sqrt 3
  plastic   Newton on x^3 = x + 1

The exact physical constants (SI 2019, standard gravity of the CGPM 1901)
are given as decimals; Brun's constant is the published 1.902160583104
(OEIS A065421), which cannot be recomputed here.

The table is pasted into LumexMathConstantsReference.hpp and reformatted by
clang-format there (cmake.wiring_math_constants reads the rows back).

Usage:
  python3 LumexMathConstantsReference.py            # print the table
  python3 LumexMathConstantsReference.py <header>   # also compare the tokens
                                                    # of LumexMathConstants.hpp
"""

import re
import sys
from decimal import Decimal as D
from decimal import getcontext
from fractions import Fraction as F

getcontext().prec = 140


def atan_inverse(n):
    """atan(1/n) by the Taylor series."""
    x = D(1) / n
    term = x
    total = term
    n2 = n * n
    k = 1
    while abs(term) > D(10) ** -135:
        term = -term / n2
        k += 2
        total += term / k
    return total


def pi_machin():
    return 16 * atan_inverse(5) - 4 * atan_inverse(239)


def pi_gauss_legendre():
    a, b, t, p = D(1), 1 / D(2).sqrt(), D(1) / 4, D(1)
    for _ in range(10):
        an = (a + b) / 2
        b = (a * b).sqrt()
        t -= p * (a - an) ** 2
        a = an
        p *= 2
    return (a + b) ** 2 / (4 * t)


def e_series():
    total, term, n = D(1), D(1), 1
    while term > D(10) ** -135:
        term /= n
        total += term
        n += 1
    return total


def bernoulli(count):
    """B_0 .. B_count as Fractions (Akiyama-Tanigawa; only even ones used)."""
    work = [F(0)] * (count + 1)
    result = []
    for i in range(count + 1):
        work[i] = F(1, i + 1)
        for j in range(i, 0, -1):
            work[j - 1] = j * (work[j - 1] - work[j])
        result.append(work[0])
    return result


def central_binomial(n):
    result = 1
    for i in range(1, n + 1):
        result = result * (n + i) // i
    return result


def frac_to_decimal(fraction):
    return D(fraction.numerator) / D(fraction.denominator)


PI = pi_machin()
assert abs(PI - pi_gauss_legendre()) < D(10) ** -125
E = e_series()
SQRT2, SQRT3, SQRT5 = D(2).sqrt(), D(3).sqrt(), D(5).sqrt()
BERNOULLI = bernoulli(80)


def euler_gamma():
    n = 1000
    harmonic = sum(F(1, k) for k in range(1, n + 1))
    total = frac_to_decimal(harmonic) - D(n).ln() - D(1) / (2 * n)
    for k in range(1, 30):
        total += (frac_to_decimal(BERNOULLI[2 * k]) / (2 * k)
                  / D(n) ** (2 * k))
    return total


def apery():
    total = D(0)
    for n in range(1, 400):
        total += D((-1) ** (n - 1)) / (D(n) ** 3 * central_binomial(n))
    return total * 5 / 2


def catalan():
    total = D(0)
    for n in range(0, 400):
        total += D(1) / (D(2 * n + 1) ** 2 * central_binomial(n))
    return PI / 8 * (2 + SQRT3).ln() + 3 * total / 8


def agm(a, b):
    for _ in range(20):
        a, b = (a + b) / 2, (a * b).sqrt()
    return a


def log_gamma_stirling(z):
    total = (z - D('0.5')) * z.ln() - z + (2 * PI).ln() / 2
    for k in range(1, 40):
        total += (frac_to_decimal(BERNOULLI[2 * k])
                  / (2 * k * (2 * k - 1) * z ** (2 * k - 1)))
    return total


def gamma_fraction(x):
    """Gamma(x) for 0 < x < 1: Stirling at x + 200, recurrence back."""
    shift = 200
    value = log_gamma_stirling(x + shift).exp()
    product = D(1)
    for k in range(shift):
        product *= x + k
    return value / product


def plastic():
    x = D('1.3247')
    for _ in range(40):
        x -= (x ** 3 - x - 1) / (3 * x * x - 1)
    return x


VARPI = PI / agm(D(1), SQRT2)
GAMMA_1_4 = gamma_fraction(D(1) / 4)
GAMMA_1_3 = gamma_fraction(D(1) / 3)
assert abs(GAMMA_1_4 ** 2 / (2 * (2 * PI).sqrt()) - VARPI) < D(10) ** -100
assert abs(GAMMA_1_3 * gamma_fraction(D(2) / 3) - 2 * PI / SQRT3) \
    < D(10) ** -100

# name, value, digits the header must carry after the point (0: exact value)
CONSTANTS = [
    ('PI', PI, 50),
    ('EULER_NUMBER', E, 50),
    ('GOLDEN_RATIO', (1 + SQRT5) / 2, 50),
    ('SILVER_RATIO', 1 + SQRT2, 50),
    ('EULER_MASCHERONI', euler_gamma(), 50),
    ('SQRT_2', SQRT2, 50),
    ('SQRT_3', SQRT3, 50),
    ('SQRT_5', SQRT5, 50),
    ('LN_2', D(2).ln(), 50),
    ('LN_10', D(10).ln(), 50),
    ('APERY', apery(), 50),
    ('CATALAN', catalan(), 50),
    ('LEMNISCATE', 2 * VARPI, 50),
    ('GAMMA_1_4', GAMMA_1_4, 50),
    ('GAMMA_1_3', GAMMA_1_3, 50),
    ('BRUNS_CONSTANT_TWIN_PRIMES', D('1.902160583104'), 12),
    ('PLASTIC_NUMBER', plastic(), 50),
    ('RECIPROCAL_PI', 1 / PI, 50),
    ('SQRT_PI', PI.sqrt(), 50),
    ('INVERSE_SQRT_PI', 1 / PI.sqrt(), 50),
    ('COPERNICUS_CONSTANT', PI / 180, 50),
    ('GRAVITY_ACCELERATION', D('9.80665'), 0),
    ('LIGHT_SPEED', D('299792458'), 0),
    ('PLANCK_CONSTANT', D('6.62607015'), 0),
    ('AVOGADRO_CONSTANT', D('6.02214076'), 0),
]


def round_to_bits(value, bits):
    """(mantissa, exponent) with mantissa * 2**exponent the correctly rounded
    (nearest, ties to even) binary value of `value` on `bits` bits."""
    exact = F(value)
    e = exact.numerator.bit_length() - exact.denominator.bit_length() - bits
    while True:
        q = exact / F(2) ** e if e >= 0 else exact * F(2) ** -e
        if q >= 2 ** bits:
            e += 1
        elif q < 2 ** (bits - 1):
            e -= 1
        else:
            break
    mantissa = q.numerator // q.denominator
    rest = q - mantissa
    if rest > F(1, 2) or (rest == F(1, 2) and mantissa % 2 == 1):
        mantissa += 1
    if mantissa == 2 ** bits:
        mantissa //= 2
        e += 1
    while mantissa % 2 == 0 and mantissa:
        mantissa //= 2
        e += 1
    return mantissa, e


def truncated(value, decimals):
    return value.quantize(D(10) ** -decimals, rounding='ROUND_DOWN')


def header_tokens(path):
    text = open(path).read()
    tokens = {}
    for m in re.finditer(
            r'#define LUMEX_MATH_CONSTANTS_(\w+)\s*\\?\s*\n?\s*([0-9.]+)',
            text):
        tokens[m.group(1)] = m.group(2)
    return tokens


def main(argv):
    tokens = header_tokens(argv[1]) if len(argv) > 1 else {}
    problems = 0
    lines = []
    for name, value, decimals in CONSTANTS:
        float_ref = round_to_bits(value, 24)
        double_ref = round_to_bits(value, 53)
        long_ref = round_to_bits(value, 64)
        # a float variable initialised from the double macro: rounded twice
        via_double = round_to_bits(F(double_ref[0]) * F(2) ** double_ref[1], 24)
        if via_double != float_ref:
            sys.stderr.write('%s: float(double) differs from float\n' % name)
        reference = (format(truncated(value, 60), 'f') if decimals
                     else format(value, 'f'))
        if '.' not in reference:
            reference += '.0'
        token = tokens.get(name)
        if token is not None:
            shown = len(token.split('.')[1])
            token_value = D(token)
            if token_value not in (truncated(value, shown),
                                   value.quantize(D(10) ** -shown)):
                sys.stderr.write('%s: header %s is wrong (true %s)\n'
                                 % (name, token, truncated(value, shown)))
                problems += 1
            for bits, expected in ((24, float_ref), (53, double_ref),
                                   (64, long_ref)):
                if round_to_bits(token_value, bits) != expected:
                    sys.stderr.write('%s: header rounds differently at %d'
                                     ' bits\n' % (name, bits))
                    problems += 1
        lines.append('  X (%s, "%s", %d, %dULL, %d, %dULL, %d, %dULL, %d)'
                     % (name, reference, decimals, float_ref[0], float_ref[1],
                        double_ref[0], double_ref[1], long_ref[0],
                        long_ref[1]))
    print('#define LUMEX_TEST_MATH_CONSTANTS_REFERENCE(X) \\')
    print(' \\\n'.join(lines))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
