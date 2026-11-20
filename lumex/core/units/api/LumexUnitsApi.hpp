/*
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

#ifndef LUMEX_CORE_UNITS_API_HPP
#define LUMEX_CORE_UNITS_API_HPP

/**
 * @file LumexUnitsApi.hpp
 * @brief The lookup of a unit symbol, the formatting of a symbol, the exact
 * ratio of two units and the conversion of a `double` value, over the catalog
 * of `data/LumexUnitsData.hpp`.
 * @details Everything is inline and works from C++11. A symbol is looked up
 * in one script (`script::international` or `script::russian`) or in either;
 * the text may be a bare symbol (`Pa`, `U+041F U+0430`), an alias (`ohm`,
 * `ua`) or a prefix and a symbol (`kPa`, `U+043C U+043A U+043C`). The rules of
 * the lookup:
 *
 * - A bare symbol wins over a generated pair: the Russian `U+0422 U+043B` is
 * the tesla, not the teralitre, `Gs` the gauss, not the gigasecond, `as` the
 * arcsecond, `ft` the foot, `nt` the nit, `U+0433 U+0440 U+0430 U+0434` the
 * gon. The shadowed pairs are listed in `LUMEX_UNITS_SHADOWED` of the data
 * header and are reached by their `unit_id` and `prefix_id`.
 * - A prefix attaches to one symbol of a unit that takes it, never to a
 *   prefixed symbol, so there are no compound prefixes (`U+00B5` followed by
 * `kg` is unknown, the kilogram is the pair `kilo` and `gram`).
 * - A text that mixes the scripts (`U+043C U+043A` followed by `g`) is
 * unknown, and so is the Cyrillic letter U+0421 or U+0441 after `U+00B0` in
 * place of the Latin C. One mixture is accepted on purpose: a Cyrillic prefix
 * with the Latin degree Celsius (`U+043C U+00B0 C`, "m-degree-C" with a
 * Cyrillic milli), because the Russian column of the standard writes the Latin
 * C.
 *
 * A value is converted by a double-double factor: the factor of a pair of
 * units is the exact value $$10^{k} \cdot \frac{p}{q} \cdot \pi^{n}$$,
 * computed in integers (up to 3584 bits) and kept as two doubles
 * `high + low` of about 106 bits (`converter_t`; make it once with
 * `make_converter` for many values), and the value is converted by one fused
 * multiply-add `fma (x, high, x * low)`, or, where there is no hardware
 * fused operation, by Dekker's two-product (three roundings, the first two
 * of quantities that are far below the last bit), or, where neither is right
 * (excess precision, a library `fma` that is not fused, `__FAST_MATH__`), by
 * the product in integers (`LUMEX_UNITS_ROUTE`). The result is the correctly
 * rounded value of the exact product, except when that product lies within
 * 2^-105 = 2.5e-32 (relative) of a tie between two doubles (2^-106 = 1.2e-32
 * on the exact route, which rounds the double-double factor and not the true
 * one); the number pi is taken with 126 bits, which adds below 1e-37 per
 * power. This holds for a factor above 2^-960 and a result in the normal range
 * (not below 2^-1022), with the denormals enabled (the start-up code of
 * `-ffast-math` and `-funsafe-math-optimizations` clears them, and the
 * two-product and fused routes then lose a few results of operands below
 * 2^-960) and the sign of a zero result kept (`-fno-signed-zeros` does not
 * keep it); 700 of a milli unit is exactly 0.7 of the unit, and a value is
 * never taken through an intermediate overflow or underflow.
 * Only units of one quantity kind convert (the hertz does not convert to the
 * becquerel, the gray does not convert to the sievert); the units with no
 * value in the SI (the logarithmic ones) never convert; the degree Celsius
 * converts as a point with `convert_point` and as a difference with
 * `convert_difference`. A unit or a prefix outside the catalog is refused
 * (`invalid_argument`, `invalid_prefix`, `invalid_unit`), not read.
 */

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "lumex/core/expected/Expected"
#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/units/data/LumexUnitsData.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
// The warnings that a strict consumer (clang-cl /W4 maps to -WCL4, or
// -Weverything) reports here: the switches over the enumerations have no
// default on purpose (-Wswitch-enum and -Wcovered-switch-default ask for the
// opposite), the aggregates are padded, the tables are read through indexes,
// and LaTeX in comments is not Doxygen.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wswitch-default"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#if __has_warning("-Wdeprecated-redundant-constexpr-static-def")
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#endif
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#endif

namespace lumex
{
namespace core
{
namespace units
{

/**
 * @brief The text type of the lookup: the `lumex_string_view` of
 * `lumex::string_view` in every standard (a string literal, a `char const *`,
 * a `std::string` and, from C++17, a `std::string_view` convert to it).
 */
using text_view_t = lumex::core::string_view::view::lumex_string_view;

#if LUMEX_HAS_CHAR8_T
/**
 * @brief The UTF-8 view of the `char8_t` overloads (C++20, or where the
 * compiler has `char8_t`). The lookup reads such a text byte by byte, so the
 * catalog's `char` tables are never read through a `char8_t` pointer.
 */
using u8_text_view_t = lumex::core::string_view::view::lumex_u8string_view;
#endif

/** @brief The result type of the functions that can fail. */
template <class Value, class Error>
using result_t = lumex::core::expected::result::expected<Value, Error>;

/** @brief Why a symbol is not found. */
enum class lookup_error : std::uint8_t
{
  /** The text is empty. */
  empty_text,
  /** No bare symbol, alias or prefix and symbol of the script is equal to
   * the text. */
  unknown_symbol
};

/** @brief Why a symbol cannot be formatted. */
enum class format_error : std::uint8_t
{
  /** The unit has no symbol in the script (the diopter has no international
   * one, the kilopond no Russian one). */
  no_symbol,
  /** The unit does not take the prefix. */
  prefix_not_allowed,
  /** The prefix is outside the 24 prefixes. */
  invalid_prefix,
  /** The symbol does not fit into a `symbol_buffer_t`. */
  too_long,
  /** The unit is outside the catalog. */
  invalid_unit,
  /** The symbol of the pair is also the symbol of another unit that wins over
   * it (see `LUMEX_UNITS_SHADOWED`): written, it would be read back as that
   * unit. */
  shadowed
};

/** @brief Why a ratio or a conversion is refused. */
enum class conversion_error : std::uint8_t
{
  /** The units measure different quantity kinds (hertz and becquerel, joule
   * and newton metre). */
  different_quantity_kind,
  /** A unit is affine (the degree Celsius): use `convert_point` or
   * `convert_difference`. */
  offset_unit,
  /** A unit has no value in an SI unit (the bel, the decibel, the neper, the
   * phon, the octave, the decade). */
  no_si_value,
  /** The exact ratio does not fit into the 64-bit integers of a scale. */
  overflow,
  /** `convert_point` takes temperature units only. */
  not_a_temperature,
  /** A unit or a prefix is outside the catalog. */
  invalid_argument
};

/** @brief A unit with an optional prefix (`prefix_id::none` for none). */
struct prefixed_unit_t
{
  unit_id unit;
  prefix_id prefix;
};

/** @brief Whether two prefixed units are the same pair. */
LUMEX_CONSTEXPR bool
operator== (prefixed_unit_t const &left,
            prefixed_unit_t const &right) LUMEX_NOEXCEPT
{
  return left.unit == right.unit && left.prefix == right.prefix;
}

/** @brief Whether two prefixed units differ. */
LUMEX_CONSTEXPR bool
operator!= (prefixed_unit_t const &left,
            prefixed_unit_t const &right) LUMEX_NOEXCEPT
{
  return !(left == right);
}

/** @brief The capacity of `symbol_buffer_t`, in bytes. */
LUMEX_CONSTEXPR std::size_t symbol_buffer_capacity = 32;

#if LUMEX_HAS_CHAR8_T
/**
 * @brief A symbol of `char8_t` in a fixed buffer, copied byte by byte from the
 * `char` tables of the catalog.
 */
struct u8_symbol_buffer_t
{
  char8_t text[symbol_buffer_capacity];
  std::size_t size;

  /** @brief The symbol as a view of the buffer. */
  LUMEX_ATTRIBUTE_NODISCARD ("The view of the symbol is lost")
  u8_text_view_t
  view () const LUMEX_NOEXCEPT
  {
    return u8_text_view_t (text, size);
  }
};

/** @brief The `char8_t` copy of a text of at most `symbol_buffer_capacity`
 * bytes (a longer one is cut). */
LUMEX_ATTRIBUTE_NODISCARD ("The copy of the text is the only output")
inline u8_symbol_buffer_t
to_u8_buffer (char const *text, std::size_t size) LUMEX_NOEXCEPT
{
  u8_symbol_buffer_t buffer = { {}, 0 };
  buffer.size = size < symbol_buffer_capacity ? size : symbol_buffer_capacity;
  for (std::size_t index = 0; index < buffer.size; ++index)
    buffer.text[index]
        = static_cast<char8_t> (static_cast<unsigned char> (text[index]));
  return buffer;
}
#endif

/**
 * @brief A symbol formatted into a fixed buffer: the text is UTF-8, `size`
 * bytes of it are set, and no null terminator is stored.
 */
struct symbol_buffer_t
{
  char text[symbol_buffer_capacity];
  std::size_t size;

  /** @brief The formatted symbol as a view of the buffer. */
  LUMEX_ATTRIBUTE_NODISCARD ("The view of the formatted symbol is lost")
  text_view_t
  view () const LUMEX_NOEXCEPT
  {
    return text_view_t (text, size);
  }

#if LUMEX_HAS_CHAR8_T
  /** @brief The formatted symbol copied into a `char8_t` buffer. */
  LUMEX_ATTRIBUTE_NODISCARD ("The copy of the formatted symbol is lost")
  u8_symbol_buffer_t
  view_u8 () const LUMEX_NOEXCEPT
  {
    return to_u8_buffer (text, size);
  }
#endif
};

/**
 * @brief The descriptor of a unit.
 * @param id A unit of the catalog.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The descriptor of the unit is lost")
LUMEX_CONSTEXPR unit_descriptor_t const &
descriptor (unit_id id) LUMEX_NOEXCEPT
{
  return unit_at (static_cast<std::size_t> (id));
}

/**
 * @brief The descriptor of a prefix.
 * @param id One of the 24 prefixes; `prefix_id::none` is not one.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The descriptor of the prefix is lost")
LUMEX_CONSTEXPR prefix_descriptor_t const &
descriptor (prefix_id id) LUMEX_NOEXCEPT
{
  return prefix_at (static_cast<std::size_t> (id));
}

/** @brief The unit with the given index, `index < unit_count`. */
LUMEX_ATTRIBUTE_NODISCARD ("The unit of the index is lost")
LUMEX_CONSTEXPR unit_id
unit_id_at (std::size_t index) LUMEX_NOEXCEPT
{
  return static_cast<unit_id> (index);
}

/** @brief Whether a quantity kind is a temperature (kelvin or degree
 * Celsius). */
LUMEX_ATTRIBUTE_NODISCARD ("The answer is lost")
LUMEX_CONSTEXPR bool
is_temperature_kind (quantity_kind kind) LUMEX_NOEXCEPT
{
  return kind == quantity_kind::thermodynamic_temperature
         || kind == quantity_kind::celsius_temperature;
}

/** @brief Whether a unit has a zero other than the SI one (the degree
 * Celsius). */
LUMEX_ATTRIBUTE_NODISCARD ("The answer is lost")
LUMEX_CONSTEXPR bool
is_affine (unit_id id) LUMEX_NOEXCEPT
{
  return static_cast<std::size_t> (id) < unit_count
         && descriptor (id).offset.numerator != 0;
}

/** @brief Whether a prefix is allowed for a unit. */
LUMEX_ATTRIBUTE_NODISCARD ("The answer is lost")
LUMEX_CONSTEXPR bool
takes_prefix (unit_id id, prefix_id prefix) LUMEX_NOEXCEPT
{
  return static_cast<std::size_t> (id) < unit_count
         && (descriptor (id).prefixes & prefix_bit (prefix)) != 0;
}

/**
 * @brief The canonical symbol of a unit in a script.
 * @return The symbol, empty when the standards give the unit none in that
 * script (or when the unit is outside the catalog).
 */
LUMEX_ATTRIBUTE_NODISCARD ("The canonical symbol is lost")
LUMEX_CONSTEXPR text_view_t
canonical_symbol (unit_id id, script in) LUMEX_NOEXCEPT
{
  return static_cast<std::size_t> (id) >= unit_count
             ? text_view_t ("", 0)
             : (in == script::international
                    ? text_view_t (descriptor (id).international.text,
                                   descriptor (id).international.size)
                    : text_view_t (descriptor (id).russian.text,
                                   descriptor (id).russian.size));
}

/** @brief The canonical symbol of a prefix in a script (empty for
 * `prefix_id::none` and for a value outside the 24 prefixes). */
LUMEX_ATTRIBUTE_NODISCARD ("The canonical symbol of the prefix is lost")
LUMEX_CONSTEXPR text_view_t
canonical_symbol (prefix_id id, script in) LUMEX_NOEXCEPT
{
  return static_cast<std::size_t> (id) >= prefix_count
             ? text_view_t ("", 0)
             : (in == script::international
                    ? text_view_t (descriptor (id).international.text,
                                   descriptor (id).international.size)
                    : text_view_t (descriptor (id).russian.text,
                                   descriptor (id).russian.size));
}

#if LUMEX_HAS_CHAR8_T
/** @brief `canonical_symbol (id, in)` copied into a `char8_t` buffer. */
LUMEX_ATTRIBUTE_NODISCARD ("The canonical symbol is lost")
inline u8_symbol_buffer_t
canonical_symbol_u8 (unit_id id, script in) LUMEX_NOEXCEPT
{
  text_view_t const symbol = canonical_symbol (id, in);
  return to_u8_buffer (symbol.data (), symbol.size ());
}

/** @brief `canonical_symbol (id, in)` of a prefix copied into a `char8_t`
 * buffer. */
LUMEX_ATTRIBUTE_NODISCARD ("The canonical symbol of the prefix is lost")
inline u8_symbol_buffer_t
canonical_symbol_u8 (prefix_id id, script in) LUMEX_NOEXCEPT
{
  text_view_t const symbol = canonical_symbol (id, in);
  return to_u8_buffer (symbol.data (), symbol.size ());
}
#endif

/** @brief The name of a domain (the part of ISO/IEC 80000). */
LUMEX_ATTRIBUTE_NODISCARD ("The name of the domain is lost")
inline char const *
domain_name (domain id) LUMEX_NOEXCEPT
{
  switch (id)
    {
    case domain::space_and_time:
      return "space_and_time";
    case domain::mechanics:
      return "mechanics";
    case domain::thermodynamics:
      return "thermodynamics";
    case domain::electromagnetism:
      return "electromagnetism";
    case domain::light_and_radiation:
      return "light_and_radiation";
    case domain::acoustics:
      return "acoustics";
    case domain::physical_chemistry:
      return "physical_chemistry";
    case domain::atomic_and_nuclear_physics:
      return "atomic_and_nuclear_physics";
    case domain::general:
      return "general";
    }
  return "";
}

/** @brief The name of a lookup error. */
LUMEX_ATTRIBUTE_NODISCARD ("The name of the error is lost")
inline char const *
error_name (lookup_error error) LUMEX_NOEXCEPT
{
  switch (error)
    {
    case lookup_error::empty_text:
      return "empty_text";
    case lookup_error::unknown_symbol:
      return "unknown_symbol";
    }
  return "";
}

/** @brief The name of a conversion error. */
LUMEX_ATTRIBUTE_NODISCARD ("The name of the error is lost")
inline char const *
error_name (conversion_error error) LUMEX_NOEXCEPT
{
  switch (error)
    {
    case conversion_error::different_quantity_kind:
      return "different_quantity_kind";
    case conversion_error::offset_unit:
      return "offset_unit";
    case conversion_error::no_si_value:
      return "no_si_value";
    case conversion_error::overflow:
      return "overflow";
    case conversion_error::not_a_temperature:
      return "not_a_temperature";
    case conversion_error::invalid_argument:
      return "invalid_argument";
    }
  return "";
}

/** @brief The name of a format error. */
LUMEX_ATTRIBUTE_NODISCARD ("The name of the error is lost")
inline char const *
error_name (format_error error) LUMEX_NOEXCEPT
{
  switch (error)
    {
    case format_error::no_symbol:
      return "no_symbol";
    case format_error::prefix_not_allowed:
      return "prefix_not_allowed";
    case format_error::invalid_prefix:
      return "invalid_prefix";
    case format_error::too_long:
      return "too_long";
    case format_error::invalid_unit:
      return "invalid_unit";
    case format_error::shadowed:
      return "shadowed";
    }
  return "";
}

namespace detail
{
// The bytes of a text and of a symbol are compared one by one, so a view of
// `char` and a view of `char8_t` read the same tables without a cast between
// pointer types.
template <class View>
inline bool
equal_symbol (View text, symbol_t const &symbol) LUMEX_NOEXCEPT
{
  if (symbol.size == 0 || text.size () != symbol.size)
    return false;
  for (std::size_t index = 0; index < symbol.size; ++index)
    {
      if (static_cast<unsigned char> (text[index])
          != static_cast<unsigned char> (symbol.text[index]))
        return false;
    }
  return true;
}

template <class View>
inline bool
starts_with_symbol (View text, symbol_t const &symbol) LUMEX_NOEXCEPT
{
  if (symbol.size == 0 || text.size () <= symbol.size)
    return false;
  for (std::size_t index = 0; index < symbol.size; ++index)
    {
      if (static_cast<unsigned char> (text[index])
          != static_cast<unsigned char> (symbol.text[index]))
        return false;
    }
  return true;
}

inline symbol_t const &
symbol_in (unit_descriptor_t const &unit, script in) LUMEX_NOEXCEPT
{
  return in == script::international ? unit.international : unit.russian;
}

inline symbol_t const &
symbol_in (prefix_descriptor_t const &prefix, script in) LUMEX_NOEXCEPT
{
  return in == script::international ? prefix.international : prefix.russian;
}

// A bare symbol or alias of a unit.
template <class View>
inline bool
find_bare (View text, script in, unit_id &found) LUMEX_NOEXCEPT
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      if (equal_symbol (text, symbol_in (unit_at (index), in)))
        {
          found = static_cast<unit_id> (index);
          return true;
        }
    }
  for (std::size_t index = 0; index < alias_count; ++index)
    {
      alias_descriptor_t const &alias = alias_at (index);
      if (alias.symbol_script == in && equal_symbol (text, alias.symbol))
        {
          found = alias.unit;
          return true;
        }
    }
  return false;
}

// A symbol or alias of a unit that takes the prefix, equal to the text.
template <class View>
inline bool
find_with_prefix (View text, script in, prefix_id prefix,
                  unit_id &found) LUMEX_NOEXCEPT
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_descriptor_t const &unit = unit_at (index);
      if ((unit.prefixes & prefix_bit (prefix)) == 0)
        continue;
      symbol_t const &canonical = symbol_in (unit, in);
      if (canonical.prefixable && equal_symbol (text, canonical))
        {
          found = unit.id;
          return true;
        }
    }
  for (std::size_t index = 0; index < alias_count; ++index)
    {
      alias_descriptor_t const &alias = alias_at (index);
      if (alias.symbol_script != in || !alias.symbol.prefixable
          || !equal_symbol (text, alias.symbol))
        continue;
      if ((unit_at (static_cast<std::size_t> (alias.unit)).prefixes
           & prefix_bit (prefix))
          != 0)
        {
          found = alias.unit;
          return true;
        }
    }
  return false;
}

// The rest of the text after a prefix symbol, when the text starts with it.
template <class View>
inline bool
split_prefix (View text, symbol_t const &symbol, View &rest) LUMEX_NOEXCEPT
{
  if (!starts_with_symbol (text, symbol))
    return false;
  rest = View (text.data () + symbol.size, text.size () - symbol.size);
  return true;
}

template <class View>
inline bool
find_pair (View text, script in, prefixed_unit_t &found) LUMEX_NOEXCEPT
{
  for (std::size_t index = 0; index < prefix_count; ++index)
    {
      prefix_descriptor_t const &prefix = prefix_at (index);
      View rest;
      unit_id unit = unit_id ();
      if (split_prefix (text, symbol_in (prefix, in), rest)
          && find_with_prefix (rest, in, prefix.id, unit))
        {
          found.unit = unit;
          found.prefix = prefix.id;
          return true;
        }
      for (std::size_t other = 0; other < prefix_alias_count; ++other)
        {
          prefix_alias_descriptor_t const &alias = prefix_alias_at (other);
          if (alias.prefix == prefix.id && alias.symbol_script == in
              && split_prefix (text, alias.symbol, rest)
              && find_with_prefix (rest, in, prefix.id, unit))
            {
              found.unit = unit;
              found.prefix = prefix.id;
              return true;
            }
        }
    }
  return false;
}

template <class View>
inline result_t<prefixed_unit_t, lookup_error>
lookup_in (View text, script in) LUMEX_NOEXCEPT
{
  if (text.size () == 0)
    return lumex::core::expected::result::failure (lookup_error::empty_text);
  prefixed_unit_t found = { unit_id (), prefix_id::none };
  if (find_bare (text, in, found.unit))
    return found;
  if (find_pair (text, in, found))
    return found;
  return lumex::core::expected::result::failure (lookup_error::unknown_symbol);
}

// A product of two positive 64-bit integers, false when it overflows.
inline bool
checked_multiply (std::int64_t left, std::int64_t right,
                  std::int64_t &product) LUMEX_NOEXCEPT
{
  if (left > (std::numeric_limits<std::int64_t>::max) () / right)
    return false;
  product = left * right;
  return true;
}

inline bool
kinds_compatible (quantity_kind left, quantity_kind right) LUMEX_NOEXCEPT
{
  return left == right
         || (is_temperature_kind (left) && is_temperature_kind (right));
}

// A unit and a prefix of the catalog (`prefix_id::none` is allowed).
inline bool
valid_pair (prefixed_unit_t const &pair) LUMEX_NOEXCEPT
{
  return static_cast<std::size_t> (pair.unit) < unit_count
         && (pair.prefix == prefix_id::none
             || static_cast<std::size_t> (pair.prefix) < prefix_count);
}

// The number of times that `base` divides `value` (and the value without
// them).
inline int
strip_factor (std::int64_t &value, std::int64_t base) LUMEX_NOEXCEPT
{
  int count = 0;
  while (value % base == 0)
    {
      value /= base;
      ++count;
    }
  return count;
}

// value *= base ^ times, false on overflow.
inline bool
multiply_by_power (std::int64_t &value, std::int64_t base,
                   int times) LUMEX_NOEXCEPT
{
  for (int step = 0; step < times; ++step)
    {
      if (!checked_multiply (value, base, value))
        return false;
    }
  return true;
}

// The canonical form of 10^exponent10 * numerator / denominator * pi^pi_power
// (numerator and denominator positive and coprime): the reduced fraction of
// the whole value without the whole factors 10 that its numerator (a positive
// exponent) or its denominator (a negative one) has. The exponents of 2 and 5
// of the value decide: with E2 and E5 the exponents of 2 and 5 in the value, k
// is the smaller of them when both are positive, the larger when both are
// negative and 0 otherwise. False on overflow.
inline bool
canonical_scale (std::int32_t exponent10, std::int64_t numerator,
                 std::int64_t denominator, int pi_power,
                 scale_t &out) LUMEX_NOEXCEPT
{
  int const e2 = static_cast<int> (exponent10) + strip_factor (numerator, 2)
                 - strip_factor (denominator, 2);
  int const e5 = static_cast<int> (exponent10) + strip_factor (numerator, 5)
                 - strip_factor (denominator, 5);
  int const smaller = e2 < e5 ? e2 : e5;
  int const larger = e2 < e5 ? e5 : e2;
  int const k = smaller > 0 ? smaller : (larger < 0 ? larger : 0);
  int const a = e2 - k;
  int const b = e5 - k;
  if (!multiply_by_power (a > 0 ? numerator : denominator, 2, a > 0 ? a : -a)
      || !multiply_by_power (b > 0 ? numerator : denominator, 5,
                             b > 0 ? b : -b))
    return false;
  out.exponent10 = k;
  out.numerator = numerator;
  out.denominator = denominator;
  out.pi_power = static_cast<std::int8_t> (pi_power);
  return true;
}

} // namespace detail

// The route of the product value * (high + low), one per configuration (the
// table is in the README). A user may define `LUMEX_UNITS_ROUTE` to one of the
// three values before the first include; the route is part of the name of an
// inline namespace (see `route_fused` below), so units built with different
// routes (a route chosen by the flags of the floating-point model, or forced)
// keep their own copies of the functions that depend on the route and never
// meet in the ODR. All units of a program that include the header must still
// be built for one instruction set, as with any inline code: a copy of an
// inline function that the linker keeps for another unit can use instructions
// (FMA, VEX) that the CPU of the program lacks.
//
//  - `LUMEX_UNITS_ROUTE_FUSED`: `fma (value, high, value * low)` with the
//    hardware fused operation (`FP_FAST_FMA`, `__FP_FAST_FMA`, `__FMA__`,
//    MSVC `/arch:AVX2`, ARM64);
//  - `LUMEX_UNITS_ROUTE_TWO_PRODUCT`: Dekker's two-product, for a machine with
//    exact binary64 arithmetic and no hardware fused operation;
//  - `LUMEX_UNITS_ROUTE_EXACT`: the product in integers, rounded once, where
//    neither of the two is right: excess precision of the arithmetic (x87,
//    `FLT_EVAL_METHOD != 0`) breaks the two-product, a library `std::fma` is
//    not always fused (MinGW-w64 without `-mfma`), and `__FAST_MATH__`,
//    finite-math-only (`__FINITE_MATH_ONLY__` of 1) and MSVC `/fp:fast`
//    (`_M_FP_FAST`) allow the compiler to rewrite both. It takes hundreds of
//    nanoseconds. The macros select the route, not the flag `-ffast-math` as
//    such: `-ffast-math -fno-finite-math-only` defines neither of the two
//    and takes the fused or the two-product route, which is right there
//    because the cores protect themselves (below).
//
// Neither compiler can be asked whether `-fassociative-math` is on (GCC
// defines `__ASSOCIATIVE_MATH__` only from version 12, clang never; only
// `-ffast-math` defines `__FAST_MATH__` in both), so the flag is not read for
// the route: the code that must not be reassociated is protected where it
// stands. The cores of the fused and the two-product routes, the split and
// the scaling switch the reassociation off for themselves: clang with
// `#pragma clang fp reassociate(off)` (clang 14 and later, where the pragma
// exists and the route is measured; a clang before 14 takes the exact route),
// GCC with `__attribute__ ((optimize ("no-associative-math")))` (GCC 9 to 13
// measured; applied where the flag can be on: before GCC 12, which has no
// macro to test, and from GCC 12 only where `__ASSOCIATIVE_MATH__` is
// defined, because GCC 11 to 13 refuse to inline `multiply_add_scaled` into
// `apply` under the attribute and a default build does not need it; 0 wrong
// of 395712 random results with
// `-funsafe-math-optimizations` at `-O3 -march=x86-64-v3`, on the default and
// on the forced two-product route). Clang 15 under
// `-funsafe-math-optimizations` has `FLT_EVAL_METHOD` of -1 and takes the
// exact route by that condition (clang 14, 16 and 18 keep `FLT_EVAL_METHOD`
// of 0), and
// `/fp:fast` of MSVC defines `_M_FP_FAST`, which takes it too. The call of
// `std::fma` is not covered by the pragma (clang marks it `reassoc` all the
// same): a fused core without the instruction, which is a route that the user
// forced, is rewritten under `-fassociative-math`, and a forced route is the
// user's own responsibility.
#define LUMEX_UNITS_ROUTE_FUSED 1
#define LUMEX_UNITS_ROUTE_TWO_PRODUCT 2
#define LUMEX_UNITS_ROUTE_EXACT 3
// internal, undefined at the end of this header
#if defined(__clang__) && __clang_major__ >= 14
#define LUMEX_UNITS_STRICT_FP _Pragma ("clang fp reassociate(off)")
#else
#define LUMEX_UNITS_STRICT_FP
#endif
#if defined(__GNUC__) && !defined(__clang__)                                  \
    && (__GNUC__ < 12 || defined(__ASSOCIATIVE_MATH__))
#define LUMEX_UNITS_GCC_STRICT                                                \
  __attribute__ ((optimize ("no-associative-math")))
#else
#define LUMEX_UNITS_GCC_STRICT
#endif
#if !defined(LUMEX_UNITS_ROUTE)
#if !defined(FLT_EVAL_METHOD) || FLT_EVAL_METHOD != 0                         \
    || defined(__FAST_MATH__) || defined(_M_FP_FAST)                          \
    || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ == 1)           \
    || (defined(__clang__) && __clang_major__ < 14)
#define LUMEX_UNITS_ROUTE LUMEX_UNITS_ROUTE_EXACT
#elif defined(FP_FAST_FMA) || defined(__FP_FAST_FMA) || defined(__FMA__)      \
    || (defined(_MSC_VER) && !defined(__clang__) && defined(__AVX2__))        \
    || defined(_M_ARM64) || defined(_M_ARM64EC)
#define LUMEX_UNITS_ROUTE LUMEX_UNITS_ROUTE_FUSED
#else
#define LUMEX_UNITS_ROUTE LUMEX_UNITS_ROUTE_TWO_PRODUCT
#endif
#endif

// The arithmetic of a double rounds a product twice (an x87 unit keeps 64 bits
// and rounds to 53 afterwards, `FLT_EVAL_METHOD` of 2 or an unknown one): the
// exact route does not multiply two doubles there. Public and read-only, like
// `LUMEX_UNITS_ROUTE`: not to be defined by the user (the tests read it).
#if !defined(FLT_EVAL_METHOD) || FLT_EVAL_METHOD != 0
#define LUMEX_UNITS_EXCESS_PRECISION 1
#else
#define LUMEX_UNITS_EXCESS_PRECISION 0
#endif

// The helpers that depend on the route (the integers of the exact factor, the
// three products) are in the inline namespace of the route as well, so units
// of different routes never share a copy of one of them.
#if LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_FUSED
inline namespace route_fused
{
#elif LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_TWO_PRODUCT
inline namespace route_two_product
{
#else
inline namespace route_exact
{
#endif
namespace route_detail
{
// The longest scale that is computed exactly (|pi_power| above it is refused
// unless the value is certainly out of the range of a double, see
// `scale_pi_power_limit`) and the length of the integers that hold it. The
// value of a scale is $$10^{k} p / q \pi^{n}$$ and pi enters as the integer
// $$P = \lfloor \pi 2^{126} \rfloor$$ of 128 bits, so the numerator or the
// denominator has at most 63 + 3.33 k + 128 n bits. A scale whose binary
// logarithm lies in [-1100, 1100] (the only one that is computed) with
// |n| <= 16 has k at most 360 in absolute value, so an integer has at most
// 63 + 1200 + 2048 = 3311 bits, the alignment of the division adds 58 bits and
// the residual of the double-double factor 6 more: 3375 bits. 112 limbs of 32
// bits are 3584 bits. Every operation that would pass the capacity sets
// `overflow` and leaves the value alone, so nothing is ever written past the
// array, and the caller refuses the result.
enum : int
{
  big_capacity = 112
};

struct big_t
{
  std::uint32_t limb[big_capacity];
  int size;
  bool overflow;
};

inline void
big_set (big_t &value, std::uint64_t number) LUMEX_NOEXCEPT
{
  value.size = 0;
  value.overflow = false;
  while (number != 0)
    {
      value.limb[value.size++] = static_cast<std::uint32_t> (number);
      number >>= 32;
    }
}

inline void
big_multiply_small (big_t &value, std::uint32_t factor) LUMEX_NOEXCEPT
{
  if (value.overflow)
    return;
  std::uint64_t carry = 0;
  for (int index = 0; index < value.size; ++index)
    {
      std::uint64_t const part
          = static_cast<std::uint64_t> (value.limb[index]) * factor + carry;
      value.limb[index] = static_cast<std::uint32_t> (part);
      carry = part >> 32;
    }
  if (carry != 0)
    {
      if (value.size >= big_capacity)
        value.overflow = true;
      else
        value.limb[value.size++] = static_cast<std::uint32_t> (carry);
    }
}

inline void
big_multiply_power_of_ten (big_t &value, int exponent) LUMEX_NOEXCEPT
{
  while (exponent >= 9 && !value.overflow)
    {
      big_multiply_small (value, 1000000000u);
      exponent -= 9;
    }
  std::uint32_t rest = 1;
  for (int step = 0; step < exponent; ++step)
    rest *= 10u;
  big_multiply_small (value, rest);
}

// The 128 bits of floor (pi * 2^126), least significant limb first.
inline void
big_multiply_pi (big_t &value) LUMEX_NOEXCEPT
{
  if (value.overflow)
    return;
  if (value.size + 4 > big_capacity)
    {
      value.overflow = true;
      return;
    }
  static std::uint32_t const pi_limbs[4]
      = { 0x80dc1cd1u, 0xc4c6628bu, 0x2168c234u, 0xc90fdaa2u };
  big_t result;
  result.overflow = false;
  result.size = value.size + 4;
  for (int index = 0; index < result.size; ++index)
    result.limb[index] = 0;
  for (int i = 0; i < value.size; ++i)
    {
      std::uint64_t carry = 0;
      for (int j = 0; j < 4; ++j)
        {
          std::uint64_t const part
              = static_cast<std::uint64_t> (value.limb[i]) * pi_limbs[j]
                + result.limb[i + j] + carry;
          result.limb[i + j] = static_cast<std::uint32_t> (part);
          carry = part >> 32;
        }
      result.limb[i + 4] = static_cast<std::uint32_t> (carry);
    }
  while (result.size > 0 && result.limb[result.size - 1] == 0)
    --result.size;
  value = result;
}

inline int
big_bit_length (big_t const &value) LUMEX_NOEXCEPT
{
  if (value.size == 0)
    return 0;
  int bits = (value.size - 1) * 32;
  std::uint32_t top = value.limb[value.size - 1];
  while (top != 0)
    {
      ++bits;
      top >>= 1;
    }
  return bits;
}

inline void
big_shift_left (big_t &value, int bits) LUMEX_NOEXCEPT
{
  if (value.overflow || value.size == 0 || bits == 0)
    return;
  int const whole = bits / 32;
  int const part = bits % 32;
  if (value.size + whole + 1 > big_capacity)
    {
      value.overflow = true;
      return;
    }
  big_t result;
  result.overflow = false;
  result.size = value.size + whole + 1;
  for (int index = 0; index < result.size; ++index)
    result.limb[index] = 0;
  for (int index = 0; index < value.size; ++index)
    {
      std::uint64_t const moved
          = static_cast<std::uint64_t> (value.limb[index]) << part;
      result.limb[index + whole] |= static_cast<std::uint32_t> (moved);
      result.limb[index + whole + 1]
          |= static_cast<std::uint32_t> (moved >> 32);
    }
  while (result.size > 0 && result.limb[result.size - 1] == 0)
    --result.size;
  value = result;
}

// value >>= bits, for 0 < bits < 64.
inline void
big_shift_right (big_t &value, int bits) LUMEX_NOEXCEPT
{
  if (value.overflow || value.size == 0 || bits == 0)
    return;
  int const whole = bits / 32;
  int const part = bits % 32;
  int const size = value.size;
  for (int index = 0; index + whole < size; ++index)
    {
      std::uint64_t moved = value.limb[index + whole];
      if (index + whole + 1 < size)
        moved |= static_cast<std::uint64_t> (value.limb[index + whole + 1])
                 << 32;
      value.limb[index] = static_cast<std::uint32_t> (moved >> part);
    }
  value.size = size > whole ? size - whole : 0;
  while (value.size > 0 && value.limb[value.size - 1] == 0)
    --value.size;
}

// value = value * 2 + bit, in place.
inline void
big_double_and_add (big_t &value, unsigned bit) LUMEX_NOEXCEPT
{
  if (value.overflow)
    return;
  std::uint32_t carry = bit;
  for (int index = 0; index < value.size; ++index)
    {
      std::uint32_t const next = value.limb[index] >> 31;
      value.limb[index] = (value.limb[index] << 1) | carry;
      carry = next;
    }
  if (carry != 0)
    {
      if (value.size >= big_capacity)
        value.overflow = true;
      else
        value.limb[value.size++] = carry;
    }
}

inline int
big_compare (big_t const &left, big_t const &right) LUMEX_NOEXCEPT
{
  if (left.size != right.size)
    return left.size < right.size ? -1 : 1;
  for (int index = left.size - 1; index >= 0; --index)
    {
      if (left.limb[index] != right.limb[index])
        return left.limb[index] < right.limb[index] ? -1 : 1;
    }
  return 0;
}

// left -= right, for left >= right.
inline void
big_subtract (big_t &left, big_t const &right) LUMEX_NOEXCEPT
{
  std::int64_t borrow = 0;
  for (int index = 0; index < left.size; ++index)
    {
      std::int64_t part
          = static_cast<std::int64_t> (left.limb[index]) - borrow
            - (index < right.size
                   ? static_cast<std::int64_t> (right.limb[index])
                   : 0);
      borrow = part < 0 ? 1 : 0;
      if (part < 0)
        part += static_cast<std::int64_t> (1) << 32;
      left.limb[index] = static_cast<std::uint32_t> (part);
    }
  while (left.size > 0 && left.limb[left.size - 1] == 0)
    --left.size;
}

// left += right.
inline void
big_add (big_t &left, big_t const &right) LUMEX_NOEXCEPT
{
  if (left.overflow || right.overflow)
    {
      left.overflow = true;
      return;
    }
  int const size = left.size > right.size ? left.size : right.size;
  std::uint64_t carry = 0;
  for (int index = 0; index < size; ++index)
    {
      std::uint64_t const part
          = carry + (index < left.size ? left.limb[index] : 0u)
            + (index < right.size ? right.limb[index] : 0u);
      left.limb[index] = static_cast<std::uint32_t> (part);
      carry = part >> 32;
    }
  left.size = size;
  if (carry != 0)
    {
      if (size >= big_capacity)
        left.overflow = true;
      else
        left.limb[left.size++] = static_cast<std::uint32_t> (carry);
    }
}

// The result of the division of two integers, rounded to a double.
struct division_t
{
  big_t remainder;        // of the aligned division
  big_t divisor;          // the aligned divisor
  std::uint64_t quotient; // the first 57 or 58 bits of the exact quotient
  std::uint64_t mantissa; // the rounded quotient (the 53 bits of a double)
  int exponent; // the exact value is (quotient + remainder / divisor) *
                // 2^exponent
  int extra;    // the bits of the quotient dropped by the rounding
  int length;   // the bits of the quotient
  double value; // the double nearest to the exact value
  bool done;    // false when the integers did not fit
};

// numerator / denominator * 2^binary: the double nearest to it (ties to
// even, also in the subnormal range, rounded once) and what is left of the
// division. The quotient has 57 or 58 bits and the remainder is kept apart
// as a flag and in `remainder`. The remainder (not the divisor) is shifted
// by one bit per quotient bit.
inline void
big_divide (big_t numerator, big_t denominator, int binary,
            division_t &out) LUMEX_NOEXCEPT
{
  out.done = false;
  out.value = 0.0;
  out.quotient = 0;
  out.mantissa = 0;
  out.extra = 0;
  out.length = 0;
  out.exponent = 0;
  out.remainder.size = 0;
  out.remainder.overflow = false;
  out.divisor.size = 0;
  out.divisor.overflow = false;
  if (numerator.overflow || denominator.overflow || denominator.size == 0)
    return;
  if (numerator.size == 0)
    {
      out.done = true;
      return;
    }
  int const shift
      = 57 - (big_bit_length (numerator) - big_bit_length (denominator));
  if (shift >= 0)
    big_shift_left (numerator, shift);
  else
    big_shift_left (denominator, -shift);
  if (numerator.overflow || denominator.overflow)
    return;
  // The low 59 bits of the numerator are brought down one at a time into the
  // remainder; the numerator is below denominator * 2^58, so the part above
  // them is below the denominator.
  std::uint64_t low = numerator.limb[0];
  if (numerator.size > 1)
    low |= static_cast<std::uint64_t> (numerator.limb[1]) << 32;
  low &= (static_cast<std::uint64_t> (1) << 59) - 1;
  big_shift_right (numerator, 59);
  std::uint64_t quotient = 0;
  for (int bit = 58; bit >= 0; --bit)
    {
      big_double_and_add (numerator,
                          static_cast<unsigned> ((low >> bit) & 1u));
      if (numerator.overflow)
        return;
      if (big_compare (numerator, denominator) >= 0)
        {
          big_subtract (numerator, denominator);
          quotient |= static_cast<std::uint64_t> (1) << bit;
        }
    }
  bool const sticky = numerator.size != 0;
  int length = 0;
  for (std::uint64_t rest = quotient; rest != 0; rest >>= 1)
    ++length;
  int const exponent = binary - shift;
  int extra = length - 53;
  // The subnormal range keeps fewer bits: the unit of the last place is
  // 2^-1074 there, and the quotient is rounded once to it.
  if (exponent + extra < -1074)
    extra = -1074 - exponent;
  out.exponent = exponent;
  out.length = length;
  out.quotient = quotient;
  out.extra = extra;
  out.remainder = numerator;
  out.divisor = denominator;
  out.done = true;
  if (extra > length)
    return; // below half of the smallest subnormal: zero
  std::uint64_t const low_part
      = quotient & ((static_cast<std::uint64_t> (1) << extra) - 1);
  std::uint64_t const half = static_cast<std::uint64_t> (1) << (extra - 1);
  std::uint64_t mantissa = quotient >> extra;
  if (low_part > half || (low_part == half && (sticky || (mantissa & 1) != 0)))
    ++mantissa;
  out.mantissa = mantissa;
  out.value = std::ldexp (static_cast<double> (mantissa), exponent + extra);
}

// The double nearest to numerator / denominator * 2^binary.
inline double
big_ratio_to_double (big_t const &numerator, big_t const &denominator,
                     int binary) LUMEX_NOEXCEPT
{
  division_t division;
  big_divide (numerator, denominator, binary, division);
  return division.value;
}

// The two doubles with high + low = the exact value to about 106 bits:
// high is the nearest double and low the nearest double to the exact
// residual. A factor out of the normal range keeps low = 0.
inline bool
big_ratio_to_pair (big_t const &numerator, big_t const &denominator,
                   int binary, double &high, double &low) LUMEX_NOEXCEPT
{
  division_t division;
  big_divide (numerator, denominator, binary, division);
  high = division.value;
  low = 0.0;
  if (!division.done)
    return false;
  // A subnormal or an infinite factor has no residual worth keeping.
  if (!(division.value >= std::numeric_limits<double>::min ())
      || division.value > std::numeric_limits<double>::max ()
      || division.extra != division.length - 53)
    return true;
  // exact - high = (quotient - mantissa * 2^extra + remainder / divisor)
  // * 2^exponent, with the integer part d in [-2^extra / 2, 2^extra / 2].
  std::int64_t const difference
      = static_cast<std::int64_t> (division.quotient)
        - static_cast<std::int64_t> (division.mantissa << division.extra);
  big_t residual = division.divisor;
  if (difference == 0)
    {
      if (division.remainder.size == 0)
        return true;
      residual = division.remainder;
    }
  else if (difference > 0)
    {
      big_multiply_small (residual, static_cast<std::uint32_t> (difference));
      big_add (residual, division.remainder);
    }
  else
    {
      big_multiply_small (residual, static_cast<std::uint32_t> (-difference));
      if (residual.overflow)
        return false;
      big_subtract (residual, division.remainder);
    }
  if (residual.overflow)
    return false;
  double const magnitude
      = big_ratio_to_double (residual, division.divisor, division.exponent);
  low = difference < 0 ? -magnitude : magnitude;
  return true;
}

// The product value * high is kept as it is when the factor has no low part,
// the value is zero (the sign of zero) or not finite.
inline bool
is_plain_product (double value, double low) LUMEX_NOEXCEPT
{
  return low == 0.0 || value == 0.0
         || !(std::fabs (value) <= std::numeric_limits<double>::max ());
}

// The core of the fused route: the product value * high is exact inside
// `std::fma`. The arguments are scaled (see `multiply_add_scaled`). Clang
// with `-fassociative-math` marks the call `reassoc` whatever the pragma says
// and, with no hardware fused operation (a forced route), rewrites
// `fma (value, high, value * low)` as `value * (high + low)`: 35124 of ~395k
// results differ there. With the instruction (`-mfma`, every build that takes
// this route) none does.
inline double LUMEX_UNITS_GCC_STRICT
multiply_add_fused_core (double value, double high, double low) LUMEX_NOEXCEPT
{
  LUMEX_UNITS_STRICT_FP
  return std::fma (value, high, value * low);
}

// The split of a double into two halves of 26 bits: `upper` is the value
// rounded to its first 26 bits (to nearest, in the bits of the number) and
// `lower` the rest, exactly. Veltkamp's split (`2^27 + 1` times the value) is
// not used on purpose: it needs `2^27 + 1` times the value rounded before the
// subtractions that follow, which a compiler that contracts `a * b - c` into
// one fused operation, or an arithmetic with excess precision, does not
// give. No arithmetic of a double is in the choice of `upper`, so nothing can
// be contracted or evaluated in excess precision there; `lower` is a
// difference of two doubles that is exact. The value is finite and not
// subnormal (the arguments are scaled, see `multiply_add_scaled`).
inline void LUMEX_UNITS_GCC_STRICT
split_double (double value, double &upper, double &lower) LUMEX_NOEXCEPT
{
  LUMEX_UNITS_STRICT_FP
  // the bits of the number are read as a 64-bit integer: a double of another
  // format has no meaning here
  static_assert (std::numeric_limits<double>::is_iec559
                     && std::numeric_limits<double>::digits == 53
                     && sizeof (double) == 8 && sizeof (std::uint64_t) == 8,
                 "the split of a double needs IEEE 754 binary64");
  std::uint64_t bits = 0;
  std::memcpy (&bits, &value, sizeof (bits));
  // The sign is the top bit and the rest is ordered as the magnitude: half of
  // the 27 bits that are dropped is added (a carry goes into the exponent,
  // which rounds a mantissa of all ones up to the next power of two).
  bits += static_cast<std::uint64_t> (1) << 26;
  bits &= ~((static_cast<std::uint64_t> (1) << 27) - 1);
  std::memcpy (&upper, &bits, sizeof (upper));
  lower = value - upper;
}

// The core of the two-product route: Dekker's two-product, high * value is
// product + error exactly, then RN (product + RN (error + RN (value * low))).
// The arguments are scaled (see `multiply_add_scaled`), so that the split does
// not overflow and the error term is not subnormal.
//
// The product `value * high` must be the rounded product, the one that the
// error term is relative to, and the halves must be halves. A compiler that
// contracts `a * b + c` (GCC with FMA instructions enabled does by default,
// and clang with `-ffp-contract=fast`) fuses the Veltkamp split and turns the
// last sum into `fma (value, high, ...)` with the exact product, so that the
// error is counted twice: GCC 13 at `-O2 -march=x86-64-v3` misrounded 1715 of
// 395712 random results of the exponents up to +-600. Two defences, each of
// which removed the misrounding alone in the builds that were measured: the
// split is made in the bits of the number (see `split_double`), and the
// rounded product is stored through a `volatile` object, because no
// multiplication is fused with an addition across a store. Every product of
// two halves is exact (26 bits times 26 bits), so fusing one of them with the
// sum next to it changes nothing; fusing `value * low` with the sum only
// rounds it once instead of twice, inside the bound of the route.
inline double LUMEX_UNITS_GCC_STRICT
multiply_add_two_product_core (double value, double high,
                               double low) LUMEX_NOEXCEPT
{
  LUMEX_UNITS_STRICT_FP
  volatile double rounded_product = value * high;
  double const product = rounded_product;
  double value_upper = 0.0;
  double value_lower = 0.0;
  double high_upper = 0.0;
  double high_lower = 0.0;
  split_double (value, value_upper, value_lower);
  split_double (high, high_upper, high_lower);
  double const error = ((value_upper * high_upper - product)
                        + value_upper * high_lower + value_lower * high_upper)
                       + value_lower * high_lower;
  return product + (error + value * low);
}

// Runs a core on arguments whose product is in [2^-900.24, 2^995.98] (the
// limits below are decimal literals: 1.0e-271 is 2^-900.24 and 6.6e299 is
// 2^995.98, so 2^995.98 plus the 27 bits of the split factor stays below
// 2^1024): the arguments and the result are scaled by exact powers of two.
// The magnitude of the product decides, not that of the arguments: above the
// upper limit the products of the split halves overflow (a result within
// 2^-26 of the largest double), below the lower one the error term and
// value * low fall into the subnormal range (a result below 2^-969). A product
// that overflows in a double can still round to the largest double (the low
// part is negative): it is run scaled down by 2^-64, and the result is the
// infinity only when the scaled-back result is. A value below 2^-968 is scaled
// up by 2^120 as well: the lower half of its split is subnormal there, and an
// arithmetic that flushes the subnormals (`-ffast-math` start-up code, DAZ and
// FTZ) would lose it. A subnormal value or result stays outside the
// guarantee.
inline double LUMEX_UNITS_GCC_STRICT
multiply_add_scaled (double value, double high, double low,
                     double (*core) (double, double, double)) LUMEX_NOEXCEPT
{
  LUMEX_UNITS_STRICT_FP
  if (is_plain_product (value, low))
    return value * high;
  double const raw_product = value * high;
  double const product = std::fabs (raw_product);
  bool const overflowed = !(product <= (std::numeric_limits<double>::max) ());
  // 2^995.98 and 2^-900.24
  double const upper_limit = 6.6e299;
  double const lower_limit = 1.0e-271;
  // 2^-967.97, above 2^-968: below 2^-968 the lower half of a split is
  // subnormal
  double const value_limit = 4.1e-292;
  // the common case: nothing to scale, and no call of `ldexp`
  if (!overflowed && std::fabs (value) <= upper_limit
      && std::fabs (high) <= upper_limit && product <= upper_limit
      && (product >= lower_limit || product == 0.0)
      && std::fabs (value) >= value_limit)
    return core (value, high, low);
  int value_shift = 0;
  int high_shift = 0;
  if (std::fabs (value) > upper_limit)
    value_shift = -64;
  if (std::fabs (high) > upper_limit)
    high_shift = -64;
  if (std::fabs (value) < value_limit)
    value_shift = 120;
  int shift = value_shift + high_shift;
  if (overflowed)
    {
      value_shift -= 64;
      shift -= 64;
    }
  else if (product != 0.0)
    {
      double const scaled = std::ldexp (product, shift);
      if (scaled > upper_limit)
        {
          value_shift -= 64;
          shift -= 64;
        }
      else if (scaled < lower_limit)
        {
          value_shift += 120;
          shift += 120;
        }
    }
  if (shift == 0)
    return core (value, high, low);
  if (value_shift != 0)
    value = std::ldexp (value, value_shift);
  if (high_shift != 0)
    {
      high = std::ldexp (high, high_shift);
      low = std::ldexp (low, high_shift);
    }
  // an overflow of the scaled product too: the result is the infinity
  if (overflowed
      && !(std::fabs (value * high) <= (std::numeric_limits<double>::max) ()))
    return raw_product;
  return std::ldexp (core (value, high, low), -shift);
}

// The product of two integers of at most 64 bits.
inline void
big_multiply_u64 (big_t &out, std::uint64_t left,
                  std::uint64_t right) LUMEX_NOEXCEPT
{
  big_t second;
  big_set (out, left);
  big_multiply_small (out, static_cast<std::uint32_t> (right >> 32));
  big_shift_left (out, 32);
  big_set (second, left);
  big_multiply_small (second, static_cast<std::uint32_t> (right));
  big_add (out, second);
}

// The mantissa of a finite double as an integer of 53 bits, and the exponent
// of its last bit.
inline std::uint64_t
integer_mantissa (double value, int &exponent) LUMEX_NOEXCEPT
{
  int binary = 0;
  double const fraction = std::frexp (std::fabs (value), &binary);
  exponent = binary - 53;
  return static_cast<std::uint64_t> (std::ldexp (fraction, 53));
}

// value * high in integers, rounded once: the product of two finite doubles
// without a rounding of the arithmetic of the machine in it. The double of an
// x87 unit (`FLT_EVAL_METHOD` of 2) rounds a product twice, to 64 bits and to
// 53; this one never multiplies two doubles.
inline double
multiply_exact_product (double left, double right) LUMEX_NOEXCEPT
{
  if (left == 0.0 || right == 0.0
      || !(std::fabs (left) <= (std::numeric_limits<double>::max) ())
      || !(std::fabs (right) <= (std::numeric_limits<double>::max) ()))
    return left * right; // a zero, an infinity or a NaN: no rounding in it
  int left_exponent = 0;
  int right_exponent = 0;
  std::uint64_t const left_mantissa = integer_mantissa (left, left_exponent);
  std::uint64_t const right_mantissa
      = integer_mantissa (right, right_exponent);
  big_t product;
  big_t one;
  big_multiply_u64 (product, left_mantissa, right_mantissa);
  big_set (one, 1);
  double const magnitude
      = big_ratio_to_double (product, one, left_exponent + right_exponent);
  return (left < 0.0) == (right < 0.0) ? magnitude : -magnitude;
}

// A product of two doubles that is rounded once on every machine: the product
// of the machine where it has no excess precision, the product in integers
// where it has.
inline double
multiply_once (double left, double right) LUMEX_NOEXCEPT
{
#if LUMEX_UNITS_EXCESS_PRECISION
  return multiply_exact_product (left, right);
#else
  return left * right;
#endif
}

// value * (high + low) in integers, rounded once: the exact route. No product
// of two doubles is formed on this route where the arithmetic of the machine
// has excess precision (see `multiply_once`).
inline double
multiply_add_exact (double value, double high, double low) LUMEX_NOEXCEPT
{
  if (value == 0.0
      || !(std::fabs (value) <= (std::numeric_limits<double>::max) ())
      || !(std::fabs (high) <= (std::numeric_limits<double>::max) ()))
    return value * high; // no rounding in it
  if (low == 0.0)
    return multiply_once (value, high);
  if (high == 0.0)
    return multiply_once (value, low);
  int value_exponent = 0;
  int high_exponent = 0;
  int low_exponent = 0;
  std::uint64_t const value_mantissa
      = integer_mantissa (value, value_exponent);
  std::uint64_t const high_mantissa = integer_mantissa (high, high_exponent);
  std::uint64_t const low_mantissa = integer_mantissa (low, low_exponent);
  int const gap = high_exponent - low_exponent;
  if (gap < 0 || gap > 3000)
    return multiply_once (value,
                          high); // the low part is far below the last bit
  big_t sum;
  big_t correction;
  big_multiply_u64 (sum, value_mantissa, high_mantissa);
  big_shift_left (sum, gap);
  big_multiply_u64 (correction, value_mantissa, low_mantissa);
  if ((low < 0.0) == (high < 0.0))
    big_add (sum, correction);
  else
    big_subtract (sum, correction);
  if (sum.overflow)
    return multiply_once (value, high);
  big_t one;
  big_set (one, 1);
  double const magnitude
      = big_ratio_to_double (sum, one, value_exponent + low_exponent);
  return (value < 0.0) == (high < 0.0) ? magnitude : -magnitude;
}

// The three routes as functions of the arguments (the tests run each of
// them; `converter_t::apply` takes the one of `LUMEX_UNITS_ROUTE`).
inline double
multiply_add_fma (double value, double high, double low) LUMEX_NOEXCEPT
{
  return multiply_add_scaled (value, high, low, &multiply_add_fused_core);
}

inline double
multiply_add_dekker (double value, double high, double low) LUMEX_NOEXCEPT
{
  return multiply_add_scaled (value, high, low,
                              &multiply_add_two_product_core);
}
} // namespace route_detail
} // namespace route

/**
 * @brief Looks a symbol up in one script.
 * @details The text is a bare symbol or alias of a unit, or a prefix and a
 * symbol that takes it, in the given script. A bare symbol wins over a prefix
 * and a symbol, and a test requires every other generated symbol to be unique,
 * so the reading of a text is never a matter of order.
 * @param text The text, UTF-8, without spaces around it.
 * @param in The script of the symbol.
 * @return The unit and its prefix (`prefix_id::none` for a bare symbol), or
 * the reason the text is unknown.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The found unit and prefix are the only output")
inline result_t<prefixed_unit_t, lookup_error>
lookup (text_view_t text, script in) LUMEX_NOEXCEPT
{
  return detail::lookup_in (text, in);
}

/**
 * @brief Looks a symbol up in either script: the international one first,
 * then the Russian one.
 * @details A text that is a symbol of both scripts (`U+00B0` followed by `C`,
 * `%`, `1`) gives the same unit in both, which a test checks.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The found unit and prefix are the only output")
inline result_t<prefixed_unit_t, lookup_error>
lookup (text_view_t text) LUMEX_NOEXCEPT
{
  result_t<prefixed_unit_t, lookup_error> const first
      = detail::lookup_in (text, script::international);
  if (first.has_value () || text.size () == 0)
    return first;
  return detail::lookup_in (text, script::russian);
}

#if LUMEX_HAS_CHAR8_T
/** @brief `lookup (text, in)` for UTF-8 text of `char8_t`. */
LUMEX_ATTRIBUTE_NODISCARD ("The found unit and prefix are the only output")
inline result_t<prefixed_unit_t, lookup_error>
lookup (u8_text_view_t text, script in) LUMEX_NOEXCEPT
{
  return detail::lookup_in (text, in);
}

/** @brief `lookup (text)` for UTF-8 text of `char8_t`. */
LUMEX_ATTRIBUTE_NODISCARD ("The found unit and prefix are the only output")
inline result_t<prefixed_unit_t, lookup_error>
lookup (u8_text_view_t text) LUMEX_NOEXCEPT
{
  result_t<prefixed_unit_t, lookup_error> const first
      = detail::lookup_in (text, script::international);
  if (first.has_value () || text.size () == 0)
    return first;
  return detail::lookup_in (text, script::russian);
}
#endif

/**
 * @brief Formats the canonical symbol of a prefix and a unit.
 * @details The symbol of the unit is its canonical symbol of the script, or,
 * for a prefixed unit whose canonical symbol takes no prefix (the arcsecond
 * sign), its first alias that does; a unit without a canonical symbol in
 * the script has none to write, whatever its aliases. The prefix is the
 * canonical one (U+00B5 MICRO SIGN, never `u`). A pair whose symbol is also
 * the symbol of another unit that wins over it (`LUMEX_UNITS_SHADOWED`) is
 * refused with `format_error::shadowed`, so what is written is always read
 * back as the same unit and prefix.
 * @param unit The unit and the prefix (`prefix_id::none` for none).
 * @param in The script of the symbol.
 * @return The symbol in a buffer, or why the pair has none.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The formatted symbol is the only output")
inline result_t<symbol_buffer_t, format_error>
format_symbol (prefixed_unit_t unit, script in) LUMEX_NOEXCEPT
{
  using lumex::core::expected::result::failure;
  if (static_cast<std::size_t> (unit.unit) >= unit_count)
    return failure (format_error::invalid_unit);
  unit_descriptor_t const &descriptor_of_unit
      = unit_at (static_cast<std::size_t> (unit.unit));
  symbol_t unit_symbol = detail::symbol_in (descriptor_of_unit, in);
  symbol_t prefix_symbol = make_symbol ("", 0, false);
  if (unit.prefix != prefix_id::none)
    {
      if (static_cast<std::size_t> (unit.prefix) >= prefix_count)
        return failure (format_error::invalid_prefix);
      if ((descriptor_of_unit.prefixes & prefix_bit (unit.prefix)) == 0)
        return failure (format_error::prefix_not_allowed);
      prefix_symbol = detail::symbol_in (
          prefix_at (static_cast<std::size_t> (unit.prefix)), in);
      // A unit that has no canonical symbol in the script is never written
      // (its aliases are input only), with a prefix or without one.
      if (unit_symbol.size == 0)
        return failure (format_error::no_symbol);
      if (!unit_symbol.prefixable)
        {
          // The canonical symbol takes no prefix: an alias that does.
          unit_symbol = make_symbol ("", 0, false);
          for (std::size_t index = 0; index < alias_count; ++index)
            {
              alias_descriptor_t const &alias = alias_at (index);
              if (alias.unit == unit.unit && alias.symbol_script == in
                  && alias.symbol.prefixable)
                {
                  unit_symbol = alias.symbol;
                  break;
                }
            }
        }
    }
  if (unit_symbol.size == 0)
    return failure (unit.prefix == prefix_id::none
                        ? format_error::no_symbol
                        : format_error::prefix_not_allowed);
  if (prefix_symbol.size + unit_symbol.size > symbol_buffer_capacity)
    return failure (format_error::too_long);
  symbol_buffer_t buffer = { {}, 0 };
  std::memcpy (buffer.text, prefix_symbol.text, prefix_symbol.size);
  std::memcpy (buffer.text + prefix_symbol.size, unit_symbol.text,
               unit_symbol.size);
  buffer.size = prefix_symbol.size + unit_symbol.size;
  if (unit.prefix != prefix_id::none)
    {
      result_t<prefixed_unit_t, lookup_error> const back
          = detail::lookup_in (buffer.view (), in);
      if (!back.has_value () || back.value () != unit)
        return failure (format_error::shadowed);
    }
  return buffer;
}

/**
 * @brief The value of a prefixed unit in the coherent SI unit of its
 * dimension: the scale of the unit with the exponent of the prefix added.
 * @details A unit without a value (`factor_kind::none`) has the zero scale,
 * and so has a unit or a prefix outside the catalog.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The scale of the unit is the only output")
inline scale_t
scale_of (prefixed_unit_t unit) LUMEX_NOEXCEPT
{
  if (!detail::valid_pair (unit))
    return make_scale (0, 0, 1, 0);
  scale_t result = unit_at (static_cast<std::size_t> (unit.unit)).scale;
  if (unit.prefix != prefix_id::none)
    result.exponent10
        += prefix_at (static_cast<std::size_t> (unit.prefix)).exponent;
  return result;
}

/**
 * @brief The factor that turns a value in `from` into a value in `to`, exactly
 * and in the canonical form of a scale.
 * @details The units must measure one quantity kind (or be temperature units:
 * the ratio of the sizes of the kelvin and the degree Celsius is 1). The
 * numerator and the denominator are reduced by their common divisors before
 * they are multiplied, so the exact ratio fails only when its reduced form
 * does not fit into 63 bits. The result is the canonical form (the reduced
 * fraction of the value without the whole factors 10 of its numerator or
 * denominator), so two ratios of one value are equal as aggregates.
 * @return The ratio, or `conversion_error::different_quantity_kind`,
 * `no_si_value` (a unit without a value), `invalid_argument` (a unit or a
 * prefix outside the catalog) or `overflow`.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The ratio of the units is the only output")
inline result_t<scale_t, conversion_error>
ratio (prefixed_unit_t from, prefixed_unit_t to) LUMEX_NOEXCEPT
{
  using lumex::core::expected::result::failure;
  if (!detail::valid_pair (from) || !detail::valid_pair (to))
    return failure (conversion_error::invalid_argument);
  unit_descriptor_t const &source
      = unit_at (static_cast<std::size_t> (from.unit));
  unit_descriptor_t const &target
      = unit_at (static_cast<std::size_t> (to.unit));
  if (source.factor == factor_kind::none || target.factor == factor_kind::none)
    return failure (conversion_error::no_si_value);
  if (!detail::kinds_compatible (source.kind, target.kind))
    return failure (conversion_error::different_quantity_kind);
  scale_t const first = scale_of (from);
  scale_t const second = scale_of (to);
  std::int64_t const common_numerator
      = greatest_common_divisor (first.numerator, second.numerator);
  std::int64_t const common_denominator
      = greatest_common_divisor (first.denominator, second.denominator);
  std::int64_t numerator = 0;
  std::int64_t denominator = 0;
  if (!detail::checked_multiply (first.numerator / common_numerator,
                                 second.denominator / common_denominator,
                                 numerator)
      || !detail::checked_multiply (first.denominator / common_denominator,
                                    second.numerator / common_numerator,
                                    denominator))
    return failure (conversion_error::overflow);
  scale_t result = { 0, 0, 1, 0 };
  if (!detail::canonical_scale (first.exponent10 - second.exponent10,
                                numerator, denominator,
                                first.pi_power - second.pi_power, result))
    return failure (conversion_error::overflow);
  return result;
}

/**
 * @brief The largest `|pi_power|` of a scale whose factor is computed. A scale
 * with a larger power has a factor of the double range only when its exponent
 * of ten undoes it; the factor of such a scale is a NaN, unless it is
 * certainly out of the range (infinity or zero). The catalog needs 2.
 */
constexpr int scale_pi_power_limit = 16;

// Everything that calls `apply` is in an inline namespace named after the
// route: units of different routes (a route chosen by the flags of the
// floating-point model, or forced) have different `converter_t` (and
// `make_converter`, `convert`, ...) as far as the linker is concerned, so one
// program never keeps one copy of an inline function for two routes.
#if LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_FUSED
inline namespace route_fused
{
#elif LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_TWO_PRODUCT
inline namespace route_two_product
{
#else
inline namespace route_exact
{
#endif

/**
 * @brief A factor of a conversion as two doubles, `high + low`: the exact
 * factor to about 106 bits.
 * @details `high` is the double nearest to the exact factor, `low` the double
 * nearest to the exact difference of the two (zero for an exact factor, and
 * for a factor outside the normal range). A value is converted by `apply`.
 * Make one with `make_converter`; the factor is computed there, once, and a
 * converter that is kept converts a value in a few nanoseconds.
 */
struct converter_t
{
  double high;
  double low;

  /**
   * @brief The value times the factor.
   * @details The route is `LUMEX_UNITS_ROUTE`. Fused: `fma (value, high,
   * value * low)`. Two-product: Dekker's two-product gives `value * high`
   * exactly as two doubles and the result is `RN (product + RN (error + RN
   * (value * low)))`. Exact: the product in integers, rounded once. The
   * arguments of the first two are scaled by powers of two so that the
   * magnitude of the product is in [2^-900, 2^995]. The result is the
   * correctly rounded value of the exact product unless that product lies
   * within a bound of a tie between two doubles: 2^-105 = 2.5e-32
   * (relative) for the fused and the two-product routes, and 2^-106 = 1.2e-32
   * for the exact route. The exact route rounds `value * (high + low)` once,
   * not the true factor, and the double-double `high + low` differs from the
   * true factor by up to 2^-106 (relative), so it has a limit too. The number
   * pi is taken with 126 bits, which adds below 1e-37 per power of pi on
   * every route. The guarantee holds for a factor above 2^-960 (so that `low`
   * is not subnormal) and a result in the normal range; a result below 2^-1022
   * is rounded twice by the first two routes. A zero, an infinity or a NaN
   * value gives `value * high`; the result overflows to the infinity of its
   * sign only when the correctly rounded value does (a product that rounds
   * to the largest double is not an infinity). On a machine with excess
   * precision (x87) the exact route multiplies no two doubles, so a product
   * is rounded once there as well.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The converted value is the only output")
  double
  apply (double value) const LUMEX_NOEXCEPT
  {
#if LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_FUSED
    return route_detail::multiply_add_fma (value, high, low);
#elif LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_TWO_PRODUCT
    return route_detail::multiply_add_dekker (value, high, low);
#else
    return route_detail::multiply_add_exact (value, high, low);
#endif
  }

  /** @brief `apply (value)`. */
  LUMEX_ATTRIBUTE_NODISCARD ("The converted value is the only output")
  double
  operator() (double value) const LUMEX_NOEXCEPT
  {
    return apply (value);
  }
};

/**
 * @brief The double-double factor of a scale.
 * @details $$10^{k} \cdot \frac{p}{q} \cdot \pi^{n}$$ is computed exactly in
 * integers of up to 3584 bits (the number pi as the 128 bits of
 * $$\lfloor \pi \cdot 2^{126} \rfloor$$): `high` is its nearest double (ties
 * to even, the subnormal range too) and `low` the nearest double of the exact
 * difference. The size of the integers is decided first from the binary
 * logarithm of the value, computed in a double (its error is far below the
 * margin of 76 binary orders): a value above 2^1100 is infinity, one below
 * 2^-1100 is zero, whatever its exponents, and no integer larger than the
 * buffer is ever formed. A zero or negative numerator or denominator (no
 * value) gives 0. A `|pi_power|` above `scale_pi_power_limit` of a value that
 * is not certainly out of the range gives a NaN `high`.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The factor of the scale is the only output")
inline converter_t
make_converter (scale_t scale) LUMEX_NOEXCEPT
{
  converter_t result = { 0.0, 0.0 };
  if (scale.numerator <= 0 || scale.denominator <= 0)
    return result;
  // log2 (10) and log2 (pi) to 17 digits.
  double const estimate
      = std::log2 (static_cast<double> (scale.numerator))
        - std::log2 (static_cast<double> (scale.denominator))
        + static_cast<double> (scale.exponent10) * 3.3219280948873623
        + static_cast<double> (scale.pi_power) * 1.6514961294723187;
  if (estimate > 1100.0)
    {
      result.high = std::numeric_limits<double>::infinity ();
      return result;
    }
  if (estimate < -1100.0)
    return result;
  if (scale.pi_power > scale_pi_power_limit
      || scale.pi_power < -scale_pi_power_limit)
    {
      result.high = std::numeric_limits<double>::quiet_NaN ();
      return result;
    }
  route_detail::big_t numerator;
  route_detail::big_t denominator;
  route_detail::big_set (numerator,
                         static_cast<std::uint64_t> (scale.numerator));
  route_detail::big_set (denominator,
                         static_cast<std::uint64_t> (scale.denominator));
  if (scale.exponent10 > 0)
    route_detail::big_multiply_power_of_ten (numerator, scale.exponent10);
  else if (scale.exponent10 < 0)
    route_detail::big_multiply_power_of_ten (denominator, -scale.exponent10);
  int binary = 0;
  for (int power = scale.pi_power; power > 0; --power)
    {
      route_detail::big_multiply_pi (numerator);
      binary -= 126;
    }
  for (int power = scale.pi_power; power < 0; ++power)
    {
      route_detail::big_multiply_pi (denominator);
      binary += 126;
    }
  if (!route_detail::big_ratio_to_pair (numerator, denominator, binary,
                                        result.high, result.low))
    {
      result.high = std::numeric_limits<double>::quiet_NaN ();
      result.low = 0.0;
    }
  return result;
}

/**
 * @brief The value of a scale as the `double` nearest to it: the `high` part
 * of `make_converter (scale)`.
 * @details Correctly rounded for a factor without pi; with pi it is correctly
 * rounded unless the exact value lies within 1e-37 (relative) of a tie (below
 * 1e-37 per power of pi). See
 * `make_converter` for the range and the NaN.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The factor of the scale is the only output")
inline double
scale_factor (scale_t scale) LUMEX_NOEXCEPT
{
  return make_converter (scale).high;
}

/**
 * @brief Applies a scale to a value: the value times the factor of the scale
 * as a double-double, rounded once (`converter_t::apply`).
 */
LUMEX_ATTRIBUTE_NODISCARD ("The scaled value is the only output")
inline double
apply_scale (scale_t scale, double value) LUMEX_NOEXCEPT
{
  return make_converter (scale).apply (value);
}

namespace route_detail
{
// The converter of the exact ratio of two units (units of one kind, or
// temperature units), the affine ones included: internal, the public
// `make_converter` refuses an affine unit.
inline result_t<converter_t, conversion_error>
make_ratio_converter (prefixed_unit_t from, prefixed_unit_t to) LUMEX_NOEXCEPT
{
  using lumex::core::expected::result::failure;
  result_t<scale_t, conversion_error> const factor = ratio (from, to);
  if (!factor.has_value ())
    return failure (factor.error ());
  converter_t const converter = make_converter (factor.value ());
  if (std::isnan (converter.high))
    return failure (conversion_error::overflow);
  return converter;
}
} // namespace route_detail

/**
 * @brief Makes the converter of two linear units of one quantity kind, once:
 * the checks (the kinds, the affine units, the arguments) and the exact
 * factor are done here, and `apply` of the result converts a value.
 * @details An affine unit (the degree Celsius) is refused: its value is a
 * point and needs `convert_point`, or a difference and needs
 * `convert_difference`.
 * @param from The unit of the values.
 * @param to The unit of the results.
 * @return The converter, or why the pair is refused (`overflow` also for a
 * factor that cannot be computed).
 */
LUMEX_ATTRIBUTE_NODISCARD ("The converter is the only output")
inline result_t<converter_t, conversion_error>
make_converter (prefixed_unit_t from, prefixed_unit_t to) LUMEX_NOEXCEPT
{
  using lumex::core::expected::result::failure;
  result_t<converter_t, conversion_error> const converter
      = route_detail::make_ratio_converter (from, to);
  if (!converter.has_value ())
    return converter;
  if (is_affine (from.unit) || is_affine (to.unit))
    return failure (conversion_error::offset_unit);
  return converter;
}

/**
 * @brief Converts a value of a linear unit into another unit of the same
 * quantity kind.
 * @details `make_converter (from, to)` and `apply`: the value times the
 * factor of the exact `ratio`, as a double-double rounded once, so the result
 * is correctly rounded (see `converter_t::apply`), and 700 of a milli unit is
 * exactly 0.7 of the unit. For many values of one pair, make the converter
 * once. An affine unit (the degree Celsius) is refused.
 * @param value The value in `from`.
 * @param from The unit of the value.
 * @param to The unit of the result.
 * @return The value in `to`, or why the conversion is refused.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The converted value is the only output")
inline result_t<double, conversion_error>
convert (double value, prefixed_unit_t from, prefixed_unit_t to) LUMEX_NOEXCEPT
{
  using lumex::core::expected::result::failure;
  result_t<converter_t, conversion_error> const converter
      = make_converter (from, to);
  if (!converter.has_value ())
    return failure (converter.error ());
  return converter.value ().apply (value);
}

/**
 * @brief Converts a difference of temperatures (or any other difference of
 * linear values) between two units: the degree Celsius and the kelvin have
 * one size, so the number does not change.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The converted difference is the only output")
inline result_t<double, conversion_error>
convert_difference (double value, prefixed_unit_t from,
                    prefixed_unit_t to) LUMEX_NOEXCEPT
{
  using lumex::core::expected::result::failure;
  result_t<converter_t, conversion_error> const converter
      = route_detail::make_ratio_converter (from, to);
  if (!converter.has_value ())
    return failure (converter.error ());
  return converter.value ().apply (value);
}

/**
 * @brief Converts a temperature (a point on a scale) between kelvin, degree
 * Celsius and the prefixed forms of both.
 * @details The value is converted by the ratio of the sizes of the units, and
 * the exact offset 273.15 K of the degree Celsius is added or subtracted in
 * the unit of the result: $$y = x \cdot \frac{s_{\text{from}}}{s_{\text{to}}}
 * + \frac{o_{\text{from}} - o_{\text{to}}}{s_{\text{to}}}$$ with the offset
 * $$o$$ of each unit in kelvin.
 */
LUMEX_ATTRIBUTE_NODISCARD ("The converted temperature is the only output")
inline result_t<double, conversion_error>
convert_point (double value, prefixed_unit_t from,
               prefixed_unit_t to) LUMEX_NOEXCEPT
{
  using lumex::core::expected::result::failure;
  if (!detail::valid_pair (from) || !detail::valid_pair (to))
    return failure (conversion_error::invalid_argument);
  if (!is_temperature_kind (descriptor (from.unit).kind)
      || !is_temperature_kind (descriptor (to.unit).kind))
    return failure (conversion_error::not_a_temperature);
  result_t<converter_t, conversion_error> const converter
      = route_detail::make_ratio_converter (from, to);
  if (!converter.has_value ())
    return failure (converter.error ());
  double const first = converter.value ().apply (value);
  double const offset_difference
      = apply_scale (descriptor (from.unit).offset, 1.0)
        - apply_scale (descriptor (to.unit).offset, 1.0);
  if (offset_difference == 0.0)
    return first;
  // The offset in the size of the unit of the result: the ratio of the
  // kelvin to it.
  prefixed_unit_t const kelvin = { unit_id::kelvin, prefix_id::none };
  result_t<converter_t, conversion_error> const to_size
      = route_detail::make_ratio_converter (kelvin, to);
  if (!to_size.has_value ())
    return failure (to_size.error ());
  return first + to_size.value ().apply (offset_difference);
}
} // namespace route
} // namespace units
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#undef LUMEX_UNITS_STRICT_FP
#undef LUMEX_UNITS_GCC_STRICT

#endif // LUMEX_CORE_UNITS_API_HPP
