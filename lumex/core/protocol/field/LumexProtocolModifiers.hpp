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

/**
 * @file LumexProtocolModifiers.hpp
 * @brief The modifiers of a numeric field: `scale<Ratio>` (fixed point), the
 * units of `unit::...` and `LUMEX_PROTOCOL_UNIT` for another unit.
 * @details A numeric field takes its modifiers after its own parameters:
 * `u16_be_<scale<std::ratio<1, 100> >, unit::microliter>`. The engineering
 * value of the field is the raw value times the ratio; the unit is the
 * symbol the decoded log line and the export print after the value. Each
 * modifier may appear once per field.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_MODIFIERS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_MODIFIERS_HPP

#include <cstddef>
#include <ratio>
#include <type_traits>

#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
/// @brief The tag a unit names as `lumex_protocol_unit`.
struct unit_tag
{
};

/**
 * @brief Field modifier: the engineering value is the raw value times
 * `Ratio` (a `std::ratio`).
 * @tparam Ratio For example `std::ratio<1, 100>` (hundredths).
 */
template <typename Ratio> struct scale
{
  /// Marks the type as a field modifier.
  using lumex_protocol_modifier = modifier_tag;
  /// The factor, reduced (`std::ratio<2, 200>` is `std::ratio<1, 100>`).
  using ratio = typename Ratio::type;
};

/**
 * @brief Defines unit `name_` with the ASCII symbol `symbol_`, a modifier of
 * numeric fields: `LUMEX_PROTOCOL_UNIT (counts_per_second, "cps");`
 */
#define LUMEX_PROTOCOL_UNIT(name_, symbol_)                                   \
  struct name_                                                                \
  {                                                                           \
    using lumex_protocol_modifier                                             \
        = ::lumex::core::protocol::field::modifier_tag;                       \
    using lumex_protocol_unit = ::lumex::core::protocol::field::unit_tag;     \
    static LUMEX_CONSTEXPR char const *                                       \
    symbol () LUMEX_NOEXCEPT                                                  \
    {                                                                         \
      return symbol_;                                                         \
    }                                                                         \
  }

/**
 * @brief The units of the library, written `unit::microliter` in a table.
 * @details ASCII symbols, so that a log line and an export stay plain text:
 * `u` for micro, `degC` for degrees Celsius. Another unit is defined with
 * `LUMEX_PROTOCOL_UNIT`.
 */
struct unit
{
  /// Percent, `%`.
  LUMEX_PROTOCOL_UNIT (percent, "%");
  /// Volt, `V`.
  LUMEX_PROTOCOL_UNIT (volt, "V");
  /// Millivolt, `mV`.
  LUMEX_PROTOCOL_UNIT (millivolt, "mV");
  /// Microvolt, `uV`.
  LUMEX_PROTOCOL_UNIT (microvolt, "uV");
  /// Ampere, `A`.
  LUMEX_PROTOCOL_UNIT (ampere, "A");
  /// Milliampere, `mA`.
  LUMEX_PROTOCOL_UNIT (milliampere, "mA");
  /// Second, `s`.
  LUMEX_PROTOCOL_UNIT (second, "s");
  /// Millisecond, `ms`.
  LUMEX_PROTOCOL_UNIT (millisecond, "ms");
  /// Minute, `min`.
  LUMEX_PROTOCOL_UNIT (minute, "min");
  /// Hertz, `Hz`.
  LUMEX_PROTOCOL_UNIT (hertz, "Hz");
  /// Kilohertz, `kHz`.
  LUMEX_PROTOCOL_UNIT (kilohertz, "kHz");
  /// Degree Celsius, `degC`.
  LUMEX_PROTOCOL_UNIT (degree_celsius, "degC");
  /// Kelvin, `K`.
  LUMEX_PROTOCOL_UNIT (kelvin, "K");
  /// Millimeter, `mm`.
  LUMEX_PROTOCOL_UNIT (millimeter, "mm");
  /// Micrometer, `um`.
  LUMEX_PROTOCOL_UNIT (micrometer, "um");
  /// Nanometer, `nm`.
  LUMEX_PROTOCOL_UNIT (nanometer, "nm");
  /// Milliliter, `mL`.
  LUMEX_PROTOCOL_UNIT (milliliter, "mL");
  /// Microliter, `uL`.
  LUMEX_PROTOCOL_UNIT (microliter, "uL");
  /// Milliliter per minute, `mL/min`.
  LUMEX_PROTOCOL_UNIT (milliliter_per_minute, "mL/min");
  /// Microliter per minute, `uL/min`.
  LUMEX_PROTOCOL_UNIT (microliter_per_minute, "uL/min");
  /// Bar, `bar`.
  LUMEX_PROTOCOL_UNIT (bar, "bar");
  /// Kilopascal, `kPa`.
  LUMEX_PROTOCOL_UNIT (kilopascal, "kPa");
  /// Megapascal, `MPa`.
  LUMEX_PROTOCOL_UNIT (megapascal, "MPa");
  /// Pound per square inch, `psi`.
  LUMEX_PROTOCOL_UNIT (psi, "psi");
  /// Revolutions per minute, `rpm`.
  LUMEX_PROTOCOL_UNIT (rpm, "rpm");
  /// Absorbance unit, `AU`.
  LUMEX_PROTOCOL_UNIT (absorbance, "AU");
  /// Milli absorbance unit, `mAU`.
  LUMEX_PROTOCOL_UNIT (milli_absorbance, "mAU");
  /// Refractive index unit, `RIU`.
  LUMEX_PROTOCOL_UNIT (riu, "RIU");
  /// Micro refractive index unit, `uRIU`.
  LUMEX_PROTOCOL_UNIT (micro_riu, "uRIU");
  /// A count of events, `count`.
  LUMEX_PROTOCOL_UNIT (count, "count");
  /// A motor step, `step`.
  LUMEX_PROTOCOL_UNIT (step, "step");
};

namespace detail
{
template <typename T, typename = void> struct is_unit : std::false_type
{
};

template <typename T>
struct is_unit<T, typename voider<typename T::lumex_protocol_unit>::type>
    : std::is_same<typename T::lumex_protocol_unit, unit_tag>
{
};

template <typename T> struct is_unit_modifier : is_unit<T>
{
};

template <typename T> struct is_scale : std::false_type
{
};

template <typename Ratio> struct is_scale<scale<Ratio>> : std::true_type
{
};

template <bool... B> struct modifier_bools
{
};

template <bool... B>
struct all_true
    : std::is_same<modifier_bools<true, B...>, modifier_bools<B..., true>>
{
};

template <template <typename> class Is, typename... T> struct count_if;

template <template <typename> class Is>
struct count_if<Is> : std::integral_constant<std::size_t, 0>
{
};

template <template <typename> class Is, typename Head, typename... Tail>
struct count_if<Is, Head, Tail...>
    : std::integral_constant<std::size_t, (Is<Head>::value ? 1u : 0u)
                                              + count_if<Is, Tail...>::value>
{
};

// The ratio of the scale<> among the modifiers, std::ratio<1> without one.
template <typename... Modifiers> struct scale_of
{
  using type = std::ratio<1>;
};

template <typename Head, typename... Tail>
struct scale_of<Head, Tail...> : scale_of<Tail...>
{
};

template <typename Ratio, typename... Tail>
struct scale_of<scale<Ratio>, Tail...>
{
  using type = typename scale<Ratio>::ratio;
};

// The symbol of the unit among the modifiers, "" without one.
template <typename... Modifiers> struct unit_of
{
  static LUMEX_CONSTEXPR char const *
  symbol () LUMEX_NOEXCEPT
  {
    return "";
  }
};

template <typename Head, bool IsUnit, typename... Tail> struct unit_pick;

template <typename Head, typename... Tail>
struct unit_pick<Head, true, Tail...>
{
  static LUMEX_CONSTEXPR char const *
  symbol () LUMEX_NOEXCEPT
  {
    return Head::symbol ();
  }
};

template <typename Head, typename... Tail>
struct unit_pick<Head, false, Tail...> : unit_of<Tail...>
{
};

template <typename Head, typename... Tail>
struct unit_of<Head, Tail...> : unit_pick<Head, is_unit<Head>::value, Tail...>
{
};

/**
 * Checks the modifiers of a numeric field (a base class, so that the
 * messages appear where the field is used). Scaled says whether the field
 * takes scale<>.
 */
template <bool Scaled, typename... Modifiers> struct check_modifiers
{
  static_assert (all_true<is_modifier<Modifiers>::value...>::value,
                 "LUMEX_PROTOCOL: every argument after the parameters of a "
                 "numeric field must be a modifier: scale<std::ratio<1, 100> "
                 "> or a unit such as unit::microliter");
  static_assert (count_if<is_scale, Modifiers...>::value <= 1,
                 "LUMEX_PROTOCOL: give scale<> at most once");
  static_assert (count_if<is_unit_modifier, Modifiers...>::value <= 1,
                 "LUMEX_PROTOCOL: give at most one unit");
  static_assert (Scaled || count_if<is_scale, Modifiers...>::value == 0,
                 "LUMEX_PROTOCOL: this field takes no scale<> (a floating "
                 "field holds the engineering value already)");
};
} // namespace detail
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_MODIFIERS_HPP
