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
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
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

/**
 * @file LumexFormatStdQuirks.hpp
 * @brief The specifications for which a standard library's `std::format` is
 * known to differ from the standard, so the cross-checks against `std::format`
 * (LumexFormatStdCompare, LumexFormatDifferential) skip exactly those and
 * nothing else.
 * @details Every rule below was found by those tests on GCC 13.2
 * (libstdc++ 13) and Clang 23 (libc++ 23) and decided against the text of
 * [format.string.std] (C++20, with the DRs P1868R2 and P2909R4) and against
 * the other library; `LumexFormat` is right in each case and the library named
 * is not. The one exception is the deliberate decision below, where the
 * libraries disagree about what the standard allows and `LumexFormat` follows
 * libc++ and MSVC STL. The libstdc++ rules are active up to release 13: a
 * newer release that fixed the case is compared again, and one that still
 * differs shows up as a failing test to widen the gate. The libc++ rules are
 * active up to release 23 (`_LIBCPP_VERSION` below 240000) for the same
 * reason.
 *
 * libstdc++ 13 (`_GLIBCXX_RELEASE <= 13`):
 * - A multibyte fill (`{:ж^+}`) is rejected: the fill is a Unicode scalar
 *   value for a UTF-8 format string (P1868R2); libc++ accepts it.
 * - `#` with `g`, `G` or no type (`{:#g}`, `{:#.3g}`, `{:#}`): the zeros that
 *   `#` keeps are one short for a value below 1 ("0.10000" for 0.1, where C's
 *   `%#g` gives "0.100000"), the decimal point of a value in scientific style
 *   follows the exponent ("1e-05." for 1e-05, where the point belongs to the
 *   significand, "1.e-05"), and a value below 1e-3 with a precision throws
 *   `std::length_error` out of `std::format`.
 * - `char` with an integer presentation type (`{:d}` of `char (-1)` is "-1"):
 *   P2909R4 formats the code unit as unsigned ("255"); libc++ does.
 * - A text with multibyte characters and a width or precision: libstdc++ 13
 *   counts code units, P1868R2 counts estimated display columns and never cuts
 *   inside a code point; libc++ agrees with `LumexFormat`.
 * - `a` of a subnormal: C leaves the first digit unspecified for a subnormal
 *   ("1p-1074" and "0.0000000000001p-1022" both conform); each library prints
 *   its own and `LumexFormat` prints that of the `to_chars` it calls.
 * - `F` of infinity and NaN is "inf" and "nan": the uppercase presentation
 *   prints "INF" and "NAN" (C's `%F`; libc++ does).
 * - The alternate form of a fixed number of hundreds of digits (`{:#.0f}` of
 *   1.8e308) starts with a stray character: the point is inserted into a
 *   buffer that has been shifted wrongly.
 * - A pointer or `nullptr` with `0` (`{:013p}`) is rejected: P2510R3
 *   (formatting pointers, a DR of C++20) allows the option and pads with zeros
 *   after the "0x"; libc++ does.
 *
 * libc++ 23 (`_LIBCPP_VERSION`):
 * - `0` together with an explicit alignment is ignored on `bool` and `char`
 *   and with `c` on a `char`; the option is not valid there (sign, `#` and
 *   `0` need an arithmetic type other than charT and bool), so `LumexFormat`
 *   rejects it. With `c` on an integer see the decision below.
 * - `#` with `g` or `G` keeps one zero too few for a value below 1, as above.
 * - A `long double` is formatted through a `double` (the precision of the
 *   value is lost) and its hexadecimal form is normalised differently.
 * - `a` of a subnormal, as above.
 *
 * A deliberate decision, not a defect of the library named: `c` with a sign,
 * `#` or `0` on an integer argument (`{:+c}`, `{:#c}`, `{:07c}` of 65).
 * The current wording of [format.string.std] allows these options for the
 * arithmetic types other than charT and bool, and libstdc++ 13 (`{:+c}` is
 * "A") and {fmt} follow the letter of it. `LumexFormat`, libc++ and MSVC STL
 * reject them on purpose, pending a decision of LWG; this follows
 * https://github.com/llvm/llvm-project/issues/106136 and
 * https://github.com/microsoft/STL/issues/4918 and is revisited when LWG
 * decides. libc++ is not consistent here: it rejects the sign, `#` and a
 * plain `0` but accepts `0` together with an explicit alignment (`{:*>05c}`)
 * and ignores the `0`, as the standard says of `0` with an alignment. The
 * rules below skip those cases (libstdc++ for all three options, libc++ for
 * `0` with an alignment) and nothing else; with a `char` argument all three
 * libraries and `LumexFormat` reject the options and the cases are compared.
 */
#ifndef LUMEX_TESTS_CORE_FMT_FORMAT_STD_QUIRKS_HPP
#define LUMEX_TESTS_CORE_FMT_FORMAT_STD_QUIRKS_HPP

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE <= 13
#define LUMEX_FORMAT_TEST_LIBSTDCXX_13 1
#else
#define LUMEX_FORMAT_TEST_LIBSTDCXX_13 0
#endif

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 240000
#define LUMEX_FORMAT_TEST_LIBCXX 1
#else
#define LUMEX_FORMAT_TEST_LIBCXX 0
#endif

namespace lumex_format_tests
{
/** @brief What kind of argument a specification is applied to. */
enum class value_kind
{
  integer,
  character,
  boolean,
  text,
  floating,
  long_floating,
  pointer,
  other
};

/** @brief The options of a standard-format-spec that the rules look at. */
struct spec_info
{
  bool multibyte_fill = false;
  bool explicit_align = false;
  bool sign = false;
  bool alternate = false;
  bool zero = false;
  bool has_width = false;
  bool has_precision = false;
  char type = '\0';
};

/**
 * @brief Reads the options of `spec` (`{:...}`, or `{}`) as the grammar of
 * [format.string.std] lays them out; no validation.
 */
inline spec_info
parse_spec (std::string const &spec)
{
  spec_info info;
  if (spec.size () < 2 || spec[0] != '{')
    return info;
  std::size_t i = spec.find (':');
  std::size_t const end = spec.rfind ('}');
  if (i == std::string::npos || end == std::string::npos || i > end)
    return info;
  ++i;
  auto is_align = [] (char c) { return c == '<' || c == '>' || c == '^'; };
  // fill-and-align: [[fill]align]
  std::size_t fill_size = 0;
  if (i < end)
    {
      unsigned char const lead = static_cast<unsigned char> (spec[i]);
      std::size_t const length = lead >= 0xF0   ? 4
                                 : lead >= 0xE0 ? 3
                                 : lead >= 0xC0 ? 2
                                                : 1;
      if (i + length < end && is_align (spec[i + length]))
        fill_size = length;
      else if (is_align (spec[i]))
        fill_size = 0;
      else
        fill_size = std::string::npos;
      if (fill_size != std::string::npos)
        {
          info.explicit_align = true;
          info.multibyte_fill = fill_size > 1;
          i += fill_size + 1;
        }
    }
  if (i < end && (spec[i] == '+' || spec[i] == '-' || spec[i] == ' '))
    {
      info.sign = true;
      ++i;
    }
  if (i < end && spec[i] == '#')
    {
      info.alternate = true;
      ++i;
    }
  if (i < end && spec[i] == '0')
    {
      info.zero = true;
      ++i;
    }
  while (i < end && spec[i] >= '0' && spec[i] <= '9')
    {
      info.has_width = true;
      ++i;
    }
  if (i < end && spec[i] == '.')
    {
      info.has_precision = true;
      ++i;
      while (i < end && spec[i] >= '0' && spec[i] <= '9')
        ++i;
    }
  if (i < end && spec[i] == 'L')
    ++i;
  if (i < end)
    info.type = spec[i];
  return info;
}

inline bool
integer_type_letter (char type)
{
  return type == 'b' || type == 'B' || type == 'd' || type == 'o'
         || type == 'x' || type == 'X';
}

/** @brief True when `text` has a byte of a multibyte character. */
inline bool
has_non_ascii (std::string_view text)
{
  for (char const c : text)
    if (static_cast<unsigned char> (c) >= 0x80)
      return true;
  return false;
}

inline bool
has_non_ascii (std::string const &text)
{
  return has_non_ascii (std::string_view (text));
}

inline bool
has_non_ascii (char const *text)
{
  return text != nullptr && has_non_ascii (std::string_view (text));
}

template <typename T>
bool
has_non_ascii (T const &)
{
  return false;
}

/** @brief The kind of argument `T` is, for `std_is_known_to_differ`. */
template <typename T>
constexpr value_kind
kind_of ()
{
  using plain = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_same_v<plain, bool>)
    return value_kind::boolean;
  else if constexpr (std::is_same_v<plain, char>)
    return value_kind::character;
  else if constexpr (std::is_same_v<plain, long double>)
    return value_kind::long_floating;
  else if constexpr (std::is_floating_point_v<plain>)
    return value_kind::floating;
  else if constexpr (std::is_integral_v<plain>)
    return value_kind::integer;
  else if constexpr (std::is_null_pointer_v<plain>)
    return value_kind::pointer;
  else if constexpr (std::is_convertible_v<plain, std::string_view>)
    return value_kind::text;
  else if constexpr (std::is_pointer_v<plain>)
    return value_kind::pointer;
  else
    return value_kind::other;
}

/** @brief True for a subnormal floating-point value. */
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_subnormal_value (T value)
{
  return std::fpclassify (value) == FP_SUBNORMAL;
}

template <typename T>
typename std::enable_if<!std::is_floating_point<T>::value, bool>::type
is_subnormal_value (T)
{
  return false;
}

/** @brief True for a finite value whose fixed text is hundreds of digits. */
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_huge_value (T value)
{
  return std::isfinite (value) && std::fabs (value) >= static_cast<T> (1e200L);
}

template <typename T>
typename std::enable_if<!std::is_floating_point<T>::value, bool>::type
is_huge_value (T)
{
  return false;
}

/** @brief True for infinity and NaN. */
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_non_finite_value (T value)
{
  return !std::isfinite (value);
}

template <typename T>
typename std::enable_if<!std::is_floating_point<T>::value, bool>::type
is_non_finite_value (T)
{
  return false;
}

/**
 * @brief True when the standard library of this build is known to differ
 * from the standard (and from `LumexFormat`) for `spec` applied to a value
 * of `kind` (see the file description).
 * @param non_ascii_text The value is a text with multibyte characters.
 * @param subnormal The value is a subnormal floating-point number.
 * @param non_finite The value is an infinity or a NaN.
 * @param huge The value is finite and above 1e200.
 */
inline bool
std_is_known_to_differ (spec_info const &spec, value_kind kind,
                        bool non_ascii_text, bool subnormal, bool non_finite,
                        bool huge)
{
  bool const floating
      = kind == value_kind::floating || kind == value_kind::long_floating;
  bool const hex_float = floating && (spec.type == 'a' || spec.type == 'A');
#if LUMEX_FORMAT_TEST_LIBSTDCXX_13
  if (spec.multibyte_fill)
    return true;
  // Deliberate decision (see the file description): libstdc++ accepts the
  // options with `c` on an integer, LumexFormat rejects them.
  if (kind == value_kind::integer && spec.type == 'c'
      && (spec.sign || spec.alternate || spec.zero))
    return true;
  if (floating && spec.alternate
      && (spec.type == 'g' || spec.type == 'G' || spec.type == '\0'))
    return true;
  if (kind == value_kind::character && integer_type_letter (spec.type))
    return true;
  if (kind == value_kind::text && non_ascii_text
      && (spec.has_width || spec.has_precision))
    return true;
  if (hex_float && subnormal)
    return true;
  if (floating && non_finite && spec.type == 'F')
    return true;
  if (kind == value_kind::pointer && spec.zero)
    return true;
  if (floating && huge && spec.alternate
      && (spec.type == 'f' || spec.type == 'F'))
    return true;
#endif
#if LUMEX_FORMAT_TEST_LIBCXX
  // A defect for `bool` and `char`, where the options are invalid.
  if (spec.zero && spec.explicit_align
      && (kind == value_kind::boolean || kind == value_kind::character)
      && !integer_type_letter (spec.type))
    return true;
  // Deliberate decision (see the file description): libc++ ignores the `0`
  // next to an alignment with `c` on an integer, LumexFormat rejects it.
  if (spec.zero && spec.explicit_align && spec.type == 'c'
      && kind == value_kind::integer)
    return true;
  if (floating && spec.alternate && (spec.type == 'g' || spec.type == 'G'))
    return true;
  if (kind == value_kind::long_floating)
    return true;
  if (hex_float && subnormal)
    return true;
#endif
  static_cast<void> (hex_float);
  static_cast<void> (non_ascii_text);
  static_cast<void> (subnormal);
  static_cast<void> (non_finite);
  static_cast<void> (huge);
  return false;
}
} // namespace lumex_format_tests

#endif // !LUMEX_TESTS_CORE_FMT_FORMAT_STD_QUIRKS_HPP
