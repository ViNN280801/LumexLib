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

/**
 * @file LumexOrdering.hpp
 * @brief `strong_ordering`, `weak_ordering` and `partial_ordering`, the
 * comparison categories of `<compare>` (C++20) for every standard from C++11,
 * with `is_eq`, `is_neq`, `is_lt`, `is_lteq`, `is_gt` and `is_gteq`.
 * @details The three classes have the values, the conversions
 * (`strong_ordering` to `weak_ordering` to `partial_ordering`) and the
 * comparisons of the standard ones: `==` and `!=` between two orderings (of
 * the same or of a weaker category), and `==`, `!=`, `<`, `<=`, `>` and `>=`
 * against the literal `0`, with `0` on either side. As in the standard, `<`
 * between two orderings does not compile: compare with `0` or with a named
 * value. `<=>` is a C++20 operator and is not provided. The classes are the
 * same in every standard and are never aliases of `std::strong_ordering` and
 * the others (as `span` is not an alias of `std::span`), so from C++20 both
 * families exist side by side and there is no conversion or comparison
 * between them.
 *
 * `strong_ordering_t`, `weak_ordering_t` and `partial_ordering_t` name the
 * ordering type "of this library at this standard": `std::*_ordering` where
 * the compiler and the standard library have `<compare>`
 * (`LUMEX_HAS_THREE_WAY_COMPARISON`), the class of this header otherwise. The
 * safe numeric comparator returns these, so code that spells the result type
 * once with the alias keeps compiling when the standard changes. The trait
 * `is_ordering` is true for the classes of both families.
 *
 * The named values are constants of the class, usable in constant expressions
 * (`constexpr auto o = strong_ordering::less;`) in every standard. Before
 * C++17 they are defined in the header with the link-once attribute of the
 * compiler (`LUMEX_ORDERING_LINK_ONCE`) in place of `inline`: a weak symbol on
 * GCC, `selectany` on MSVC, MinGW and Cygwin, an `inline` variable on Clang.
 * @since C++11
 */
#ifndef LUMEX_CORE_UTILITY_NUMERIC_ORDERING_HPP
#define LUMEX_CORE_UTILITY_NUMERIC_ORDERING_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#if __has_warning("-Wc++17-extensions")
#pragma clang diagnostic ignored "-Wc++17-extensions"
#endif
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#endif

#include <type_traits>
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @brief What stands in place of `inline` on the definitions of the named
 * values of the ordering classes.
 * @details The named values (`strong_ordering::less`, ...) are static data
 * members of a literal class type, declared in the class and defined after it
 * in the header, as the standard library does it with `inline constexpr`.
 * Before C++17 `inline` is not available for a variable, and a plain
 * definition in a header is defined again in every translation unit that
 * includes it. A class template cannot stand in: a member of a class template
 * is not usable in constant expressions unless it is declared `constexpr` in
 * the class, and there its type is incomplete. So the definitions carry the
 * link-once attribute of the compiler: `inline` from C++17, `selectany` on
 * MSVC (clang-cl too), MinGW and Cygwin, an `inline` variable (the C++17
 * feature as an extension, its warning ignored in this header) on Clang, a
 * weak symbol on GCC. A compiler none of these fits gets a plain definition,
 * which is correct only while one translation unit includes the header.
 */
#if __cplusplus >= 201703L
#define LUMEX_ORDERING_LINK_ONCE inline
#elif defined(_MSC_VER)
#define LUMEX_ORDERING_LINK_ONCE __declspec (selectany)
#elif defined(__clang__)
#define LUMEX_ORDERING_LINK_ONCE inline
#elif defined(__MINGW32__) || defined(__CYGWIN__)
#define LUMEX_ORDERING_LINK_ONCE __attribute__ ((selectany))
#elif defined(__GNUC__)
#define LUMEX_ORDERING_LINK_ONCE __attribute__ ((weak))
#else
#define LUMEX_ORDERING_LINK_ONCE
#endif

namespace lumex
{
namespace core
{
namespace utility
{
namespace numeric
{

class strong_ordering;
class weak_ordering;
class partial_ordering;

namespace Detail
{
/**
 * @brief The value of an ordering: the sign of the comparison, and `unordered`
 * for the one value a `partial_ordering` has besides the three.
 */
enum class ordering_value : signed char
{
  less = -1,
  equivalent = 0,
  greater = 1,
  unordered = 2
};

/**
 * @brief The type of the second parameter of a comparison with `0`.
 * @details It is constructible only from a pointer to itself, so the only
 * arguments that fit are the literal `0` (a null pointer constant) and
 * `nullptr`, as with the unspecified parameter type of the standard
 * orderings. A variable, even one of value 0, does not fit.
 */
struct literal_zero_t
{
  LUMEX_CONSTEXPR_CTOR
  literal_zero_t (literal_zero_t *) LUMEX_NOEXCEPT {}
};

/**
 * @brief The value and the comparisons shared by the three ordering classes.
 * @tparam Derived The ordering class (it derives from this class).
 * @details The comparisons are hidden friends found through the base class of
 * the ordering, written once for `Derived` instead of three times. The value
 * of a `partial_ordering` may be `unordered`, and then every comparison with
 * `0` is false except `!=`, which is true, as in the standard.
 */
template <typename Derived> class ordering_base
{
protected:
  signed char m_value; ///< An `ordering_value`.

  LUMEX_CONSTEXPR_CTOR explicit ordering_base (ordering_value value)
      LUMEX_NOEXCEPT : m_value (static_cast<signed char> (value))
  {
  }

  /// `lhs == rhs`: the same value.
  friend LUMEX_CONSTEXPR bool
  operator== (Derived lhs, Derived rhs) LUMEX_NOEXCEPT
  {
    return lhs.m_value == rhs.m_value;
  }

  /// `lhs != rhs`: not the same value.
  friend LUMEX_CONSTEXPR bool
  operator!= (Derived lhs, Derived rhs) LUMEX_NOEXCEPT
  {
    return lhs.m_value != rhs.m_value;
  }

  /// `value == 0`: equivalent.
  friend LUMEX_CONSTEXPR bool
  operator== (Derived value, literal_zero_t) LUMEX_NOEXCEPT
  {
    return value.m_value == 0;
  }

  /// `0 == value`: equivalent.
  friend LUMEX_CONSTEXPR bool
  operator== (literal_zero_t, Derived value) LUMEX_NOEXCEPT
  {
    return value.m_value == 0;
  }

  /// `value != 0`: not equivalent (`unordered` is not equivalent either).
  friend LUMEX_CONSTEXPR bool
  operator!= (Derived value, literal_zero_t) LUMEX_NOEXCEPT
  {
    return value.m_value != 0;
  }

  /// `0 != value`: not equivalent.
  friend LUMEX_CONSTEXPR bool
  operator!= (literal_zero_t, Derived value) LUMEX_NOEXCEPT
  {
    return value.m_value != 0;
  }

  /// `value < 0`: less.
  friend LUMEX_CONSTEXPR bool
  operator< (Derived value, literal_zero_t) LUMEX_NOEXCEPT
  {
    return value.m_value == -1;
  }

  /// `0 < value`: greater.
  friend LUMEX_CONSTEXPR bool
  operator< (literal_zero_t, Derived value) LUMEX_NOEXCEPT
  {
    return value.m_value == 1;
  }

  /// `value <= 0`: less or equivalent (false for `unordered`).
  friend LUMEX_CONSTEXPR bool
  operator<= (Derived value, literal_zero_t) LUMEX_NOEXCEPT
  {
    return value.m_value == -1 || value.m_value == 0;
  }

  /// `0 <= value`: greater or equivalent (false for `unordered`).
  friend LUMEX_CONSTEXPR bool
  operator<= (literal_zero_t, Derived value) LUMEX_NOEXCEPT
  {
    return value.m_value == 0 || value.m_value == 1;
  }

  /// `value > 0`: greater.
  friend LUMEX_CONSTEXPR bool
  operator> (Derived value, literal_zero_t) LUMEX_NOEXCEPT
  {
    return value.m_value == 1;
  }

  /// `0 > value`: less.
  friend LUMEX_CONSTEXPR bool
  operator> (literal_zero_t, Derived value) LUMEX_NOEXCEPT
  {
    return value.m_value == -1;
  }

  /// `value >= 0`: greater or equivalent (false for `unordered`).
  friend LUMEX_CONSTEXPR bool
  operator>= (Derived value, literal_zero_t) LUMEX_NOEXCEPT
  {
    return value.m_value == 0 || value.m_value == 1;
  }

  /// `0 >= value`: less or equivalent (false for `unordered`).
  friend LUMEX_CONSTEXPR bool
  operator>= (literal_zero_t, Derived value) LUMEX_NOEXCEPT
  {
    return value.m_value == -1 || value.m_value == 0;
  }
};
} // namespace Detail

/**
 * @brief The result of a three-way comparison that may leave two values
 * unordered (NaN against anything): less, equivalent, greater or unordered.
 * @details The category of the floating-point `<=>`. Same values and
 * comparisons as `std::partial_ordering`; `unordered` is neither less, equal
 * nor greater, so `unordered == 0`, `unordered < 0`, `unordered > 0`,
 * `unordered <= 0` and `unordered >= 0` are all false and `unordered != 0` is
 * true. There is no default constructor; take one of the named values.
 */
class partial_ordering : public Detail::ordering_base<partial_ordering>
{
  friend class weak_ordering;
  friend class strong_ordering;

  LUMEX_CONSTEXPR_CTOR explicit partial_ordering (Detail::ordering_value value)
      LUMEX_NOEXCEPT : Detail::ordering_base<partial_ordering> (value)
  {
  }

public:
  static partial_ordering const less; ///< The left operand is less.
  static partial_ordering const
      equivalent; ///< The operands are equivalent (equal).
  static partial_ordering const greater;   ///< The left operand is greater.
  static partial_ordering const unordered; ///< The operands cannot be ordered.
};

/**
 * @brief The result of a three-way comparison of a total preorder: less,
 * equivalent or greater, where equivalent values may differ.
 * @details Same values and comparisons as `std::weak_ordering`; converts to
 * `partial_ordering`. There is no default constructor.
 */
class weak_ordering : public Detail::ordering_base<weak_ordering>
{
  friend class strong_ordering;

  LUMEX_CONSTEXPR_CTOR explicit weak_ordering (Detail::ordering_value value)
      LUMEX_NOEXCEPT : Detail::ordering_base<weak_ordering> (value)
  {
  }

public:
  static weak_ordering const less; ///< The left operand is less.
  static weak_ordering const
      equivalent; ///< The operands are equivalent (not necessarily equal).
  static weak_ordering const greater; ///< The left operand is greater.

  /// The same value as a `partial_ordering`.
  LUMEX_CONSTEXPR
  operator partial_ordering () const LUMEX_NOEXCEPT
  {
    return partial_ordering (static_cast<Detail::ordering_value> (m_value));
  }
};

/**
 * @brief The result of a three-way comparison of a total order: less, equal or
 * greater, where equal values are indistinguishable.
 * @details Same values and comparisons as `std::strong_ordering`; converts to
 * `weak_ordering` and `partial_ordering`. `equal` and `equivalent` are the
 * same value. There is no default constructor.
 */
class strong_ordering : public Detail::ordering_base<strong_ordering>
{
  LUMEX_CONSTEXPR_CTOR explicit strong_ordering (Detail::ordering_value value)
      LUMEX_NOEXCEPT : Detail::ordering_base<strong_ordering> (value)
  {
  }

public:
  static strong_ordering const less;       ///< The left operand is less.
  static strong_ordering const equal;      ///< The operands are equal.
  static strong_ordering const equivalent; ///< The same value as `equal`.
  static strong_ordering const greater;    ///< The left operand is greater.

  /// The same value as a `weak_ordering`.
  LUMEX_CONSTEXPR
  operator weak_ordering () const LUMEX_NOEXCEPT
  {
    return weak_ordering (static_cast<Detail::ordering_value> (m_value));
  }

  /// The same value as a `partial_ordering`.
  LUMEX_CONSTEXPR
  operator partial_ordering () const LUMEX_NOEXCEPT
  {
    return partial_ordering (static_cast<Detail::ordering_value> (m_value));
  }
};

LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR partial_ordering const
    partial_ordering::less (Detail::ordering_value::less);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR partial_ordering const
    partial_ordering::equivalent (Detail::ordering_value::equivalent);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR partial_ordering const
    partial_ordering::greater (Detail::ordering_value::greater);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR partial_ordering const
    partial_ordering::unordered (Detail::ordering_value::unordered);

LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR weak_ordering const
    weak_ordering::less (Detail::ordering_value::less);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR weak_ordering const
    weak_ordering::equivalent (Detail::ordering_value::equivalent);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR weak_ordering const
    weak_ordering::greater (Detail::ordering_value::greater);

LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR strong_ordering const
    strong_ordering::less (Detail::ordering_value::less);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR strong_ordering const
    strong_ordering::equal (Detail::ordering_value::equivalent);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR strong_ordering const
    strong_ordering::equivalent (Detail::ordering_value::equivalent);
LUMEX_ORDERING_LINK_ONCE LUMEX_CONSTEXPR strong_ordering const
    strong_ordering::greater (Detail::ordering_value::greater);

/**
 * @brief True for `value == 0`: equivalent (equal).
 * @param[in] value The ordering; a `strong_ordering` or a `weak_ordering`
 * converts.
 * @return true if `value` is `equivalent`.
 */
LUMEX_CONSTEXPR bool
is_eq (partial_ordering value) LUMEX_NOEXCEPT
{
  return value == partial_ordering::equivalent;
}

/**
 * @brief True for `value != 0`: not equivalent, `unordered` included.
 * @param[in] value The ordering.
 * @return true if `value` is not `equivalent`.
 */
LUMEX_CONSTEXPR bool
is_neq (partial_ordering value) LUMEX_NOEXCEPT
{
  return value != partial_ordering::equivalent;
}

/**
 * @brief True for `value < 0`: less.
 * @param[in] value The ordering.
 * @return true if `value` is `less`.
 */
LUMEX_CONSTEXPR bool
is_lt (partial_ordering value) LUMEX_NOEXCEPT
{
  return value == partial_ordering::less;
}

/**
 * @brief True for `value <= 0`: less or equivalent.
 * @param[in] value The ordering.
 * @return true if `value` is `less` or `equivalent`; false for `unordered`.
 */
LUMEX_CONSTEXPR bool
is_lteq (partial_ordering value) LUMEX_NOEXCEPT
{
  return value == partial_ordering::less
         || value == partial_ordering::equivalent;
}

/**
 * @brief True for `value > 0`: greater.
 * @param[in] value The ordering.
 * @return true if `value` is `greater`.
 */
LUMEX_CONSTEXPR bool
is_gt (partial_ordering value) LUMEX_NOEXCEPT
{
  return value == partial_ordering::greater;
}

/**
 * @brief True for `value >= 0`: greater or equivalent.
 * @param[in] value The ordering.
 * @return true if `value` is `greater` or `equivalent`; false for
 * `unordered`.
 */
LUMEX_CONSTEXPR bool
is_gteq (partial_ordering value) LUMEX_NOEXCEPT
{
  return value == partial_ordering::greater
         || value == partial_ordering::equivalent;
}

#if LUMEX_HAS_THREE_WAY_COMPARISON
/// The strong ordering of this library at this standard:
/// `std::strong_ordering`.
using strong_ordering_t = std::strong_ordering;
/// The weak ordering of this library at this standard: `std::weak_ordering`.
using weak_ordering_t = std::weak_ordering;
/// The partial ordering of this library at this standard:
/// `std::partial_ordering`.
using partial_ordering_t = std::partial_ordering;
#else
/// The strong ordering of this library at this standard: `strong_ordering` of
/// this header (no `<compare>`).
using strong_ordering_t = strong_ordering;
/// The weak ordering of this library at this standard: `weak_ordering` of this
/// header (no `<compare>`).
using weak_ordering_t = weak_ordering;
/// The partial ordering of this library at this standard: `partial_ordering`
/// of this header (no `<compare>`).
using partial_ordering_t = partial_ordering;
#endif

namespace Detail
{
/// True if `T`, which has no cv-qualifier and is no reference, is an ordering.
template <typename T>
struct is_plain_ordering
    : std::integral_constant<
          bool, std::is_same<T, strong_ordering>::value
                    || std::is_same<T, weak_ordering>::value
                    || std::is_same<T, partial_ordering>::value
#if LUMEX_HAS_THREE_WAY_COMPARISON
                    || std::is_same<T, std::strong_ordering>::value
                    || std::is_same<T, std::weak_ordering>::value
                    || std::is_same<T, std::partial_ordering>::value
#endif
          >
{
};
} // namespace Detail

/**
 * @brief True for the three ordering classes of this header and, where
 * `<compare>` is available, for the three of the standard (cv-qualifiers and a
 * reference are ignored).
 * @tparam T The type to test.
 */
template <typename T>
struct is_ordering : Detail::is_plain_ordering<typename std::remove_cv<
                         typename std::remove_reference<T>::type>::type>
{
};

} // namespace numeric
} // namespace utility
} // namespace core
} // namespace lumex

#undef LUMEX_ORDERING_LINK_ONCE

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_NUMERIC_ORDERING_HPP
