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
 * the others (as `span` is not an alias of `std::span`).
 *
 * Where the standard library has `<compare>` (C++20,
 * `LUMEX_HAS_THREE_WAY_COMPARISON`) each class converts implicitly to the
 * `std::` ordering of its category and of every weaker one
 * (`strong_ordering` to `std::strong_ordering`, `std::weak_ordering` and
 * `std::partial_ordering`; `weak_ordering` to the last two; `partial_ordering`
 * to the last) and is constructed implicitly from the same `std::` orderings
 * (`partial_ordering` from all three, `weak_ordering` from
 * `std::weak_ordering` and `std::strong_ordering`, `strong_ordering` from
 * `std::strong_ordering`). The conversions are `constexpr` and `noexcept` like
 * the standard classes and take exactly those types, so they never disturb the
 * conversions among the own classes. `==` and `!=` compare an own ordering
 * with a `std::` ordering in both orders by value (`unordered` equals only
 * `std::partial_ordering::unordered`); without those operators the call would
 * be ambiguous between the conversions of the two sides. The relational
 * operators between two orderings stay absent, as in the standard.
 *
 * `strong_ordering_t`, `weak_ordering_t` and `partial_ordering_t` are plain
 * aliases of the three classes in every standard. The safe numeric comparator
 * returns these, so code that spells the result type once with the alias keeps
 * compiling when the standard changes. The trait `is_ordering` is true for the
 * classes of both families.
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

#if LUMEX_HAS_THREE_WAY_COMPARISON
/**
 * @brief True for the three ordering classes of `<compare>`.
 */
template <typename T>
struct is_std_ordering
    : std::integral_constant<
          bool, std::is_same<T, std::strong_ordering>::value
                    || std::is_same<T, std::weak_ordering>::value
                    || std::is_same<T, std::partial_ordering>::value>
{
};

/**
 * @brief The category rank of a class of `<compare>`: 2 strong, 1 weak,
 * 0 partial. An own class converts to the standard classes of its rank and
 * the lower ones, and is built from those of its rank and the higher ones.
 */
template <typename T>
struct std_ordering_rank
    : std::integral_constant<
          int, std::is_same<T, std::strong_ordering>::value
                   ? 2
                   : (std::is_same<T, std::weak_ordering>::value ? 1 : 0)>
{
};

/**
 * @brief The category rank of an own ordering class (see
 * `std_ordering_rank`); defined for the three classes below.
 */
template <typename Own> struct own_ordering_rank;

// The `ordering_value` of a `std::` ordering as a number (-1, 0, 1, and 2 for
// the one `std::partial_ordering` that is neither). They compare with the
// named values, not with the literal 0 the standard classes also take: Clang
// warns of a "zero as null pointer constant" for that literal in a template.
/// -1 less, 0 equal, 1 greater.
LUMEX_CONSTEXPR inline signed char
std_ordering_value (std::strong_ordering value) LUMEX_NOEXCEPT
{
  return value == std::strong_ordering::less ? static_cast<signed char> (-1)
         : value == std::strong_ordering::greater
             ? static_cast<signed char> (1)
             : static_cast<signed char> (0);
}

/// -1 less, 0 equivalent, 1 greater.
LUMEX_CONSTEXPR inline signed char
std_ordering_value (std::weak_ordering value) LUMEX_NOEXCEPT
{
  return value == std::weak_ordering::less      ? static_cast<signed char> (-1)
         : value == std::weak_ordering::greater ? static_cast<signed char> (1)
                                                : static_cast<signed char> (0);
}

/// -1 less, 0 equivalent, 1 greater, 2 unordered.
LUMEX_CONSTEXPR inline signed char
std_ordering_value (std::partial_ordering value) LUMEX_NOEXCEPT
{
  return value == std::partial_ordering::less ? static_cast<signed char> (-1)
         : value == std::partial_ordering::greater
             ? static_cast<signed char> (1)
         : value == std::partial_ordering::equivalent
             ? static_cast<signed char> (0)
             : static_cast<signed char> (2);
}

template <>
struct own_ordering_rank<strong_ordering> : std::integral_constant<int, 2>
{
};
template <>
struct own_ordering_rank<weak_ordering> : std::integral_constant<int, 1>
{
};
template <>
struct own_ordering_rank<partial_ordering> : std::integral_constant<int, 0>
{
};

/// The `std::strong_ordering` of an ordering value (`equal` for 0).
LUMEX_CONSTEXPR inline std::strong_ordering
to_std_ordering (signed char value,
                 std::strong_ordering const *) LUMEX_NOEXCEPT
{
  return value < 0   ? std::strong_ordering::less
         : value > 0 ? std::strong_ordering::greater
                     : std::strong_ordering::equal;
}

/// The `std::weak_ordering` of an ordering value.
LUMEX_CONSTEXPR inline std::weak_ordering
to_std_ordering (signed char value, std::weak_ordering const *) LUMEX_NOEXCEPT
{
  return value < 0   ? std::weak_ordering::less
         : value > 0 ? std::weak_ordering::greater
                     : std::weak_ordering::equivalent;
}

/// The `std::partial_ordering` of an ordering value (`unordered` for 2).
LUMEX_CONSTEXPR inline std::partial_ordering
to_std_ordering (signed char value,
                 std::partial_ordering const *) LUMEX_NOEXCEPT
{
  return value == 2  ? std::partial_ordering::unordered
         : value < 0 ? std::partial_ordering::less
         : value > 0 ? std::partial_ordering::greater
                     : std::partial_ordering::equivalent;
}
#endif

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

#if LUMEX_HAS_THREE_WAY_COMPARISON
  /// `own == std`: the same value as the standard ordering (C++20).
  template <typename Std>
  friend LUMEX_CONSTEXPR
      typename std::enable_if<is_std_ordering<Std>::value, bool>::type
      operator== (Derived lhs, Std rhs) LUMEX_NOEXCEPT
  {
    return lhs.m_value == std_ordering_value (rhs);
  }

  /// `std == own`: the same value as the standard ordering (C++20).
  template <typename Std>
  friend LUMEX_CONSTEXPR
      typename std::enable_if<is_std_ordering<Std>::value, bool>::type
      operator== (Std lhs, Derived rhs) LUMEX_NOEXCEPT
  {
    return std_ordering_value (lhs) == rhs.m_value;
  }

  /// `own != std`: not the same value as the standard ordering (C++20).
  template <typename Std>
  friend LUMEX_CONSTEXPR
      typename std::enable_if<is_std_ordering<Std>::value, bool>::type
      operator!= (Derived lhs, Std rhs) LUMEX_NOEXCEPT
  {
    return lhs.m_value != std_ordering_value (rhs);
  }

  /// `std != own`: not the same value as the standard ordering (C++20).
  template <typename Std>
  friend LUMEX_CONSTEXPR
      typename std::enable_if<is_std_ordering<Std>::value, bool>::type
      operator!= (Std lhs, Derived rhs) LUMEX_NOEXCEPT
  {
    return std_ordering_value (lhs) != rhs.m_value;
  }

public:
  /**
   * @brief Implicit conversion to the standard ordering of the same category
   * or a weaker one (C++20).
   * @details A conversion function template that accepts exactly
   * `std::strong_ordering`, `std::weak_ordering` or `std::partial_ordering`
   * as the target, as far as the category of the own class reaches (a
   * `weak_ordering` does not convert to `std::strong_ordering`, as the
   * standard class does not). The value is the same; the `equivalent` of an
   * own `strong_ordering` is the `equal` of `std::strong_ordering`.
   * @tparam Std Deduced from the target.
   */
  template <typename Std, typename std::enable_if<
                              is_std_ordering<Std>::value
                                  && (std_ordering_rank<Std>::value
                                      <= own_ordering_rank<Derived>::value),
                              int>::type
                          = 0>
  LUMEX_CONSTEXPR
  operator Std () const LUMEX_NOEXCEPT
  {
    return to_std_ordering (m_value, static_cast<Std const *> (nullptr));
  }

protected:
#endif

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

#if LUMEX_HAS_THREE_WAY_COMPARISON
  /**
   * @brief Implicit construction from the standard ordering of the same
   * category or a stronger one (C++20).
   * @details A constructor template that accepts exactly the classes of
   * `<compare>` of that reach, so the conversions among the own classes are
   * not disturbed.
   * @tparam Std Deduced from the argument.
   * @param value The standard ordering.
   */
  template <typename Std,
            typename std::enable_if<
                Detail::is_std_ordering<Std>::value
                    && (Detail::std_ordering_rank<Std>::value >= 0),
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  partial_ordering (Std value) LUMEX_NOEXCEPT
      : Detail::ordering_base<partial_ordering> (
            static_cast<Detail::ordering_value> (
                Detail::std_ordering_value (value)))
  {
  }
#endif
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

#if LUMEX_HAS_THREE_WAY_COMPARISON
  /**
   * @brief Implicit construction from the standard ordering of the same
   * category or a stronger one (C++20).
   * @details A constructor template that accepts exactly the classes of
   * `<compare>` of that reach, so the conversions among the own classes are
   * not disturbed.
   * @tparam Std Deduced from the argument.
   * @param value The standard ordering.
   */
  template <typename Std,
            typename std::enable_if<
                Detail::is_std_ordering<Std>::value
                    && (Detail::std_ordering_rank<Std>::value >= 1),
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  weak_ordering (Std value) LUMEX_NOEXCEPT
      : Detail::ordering_base<weak_ordering> (
            static_cast<Detail::ordering_value> (
                Detail::std_ordering_value (value)))
  {
  }
#endif

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

#if LUMEX_HAS_THREE_WAY_COMPARISON
  /**
   * @brief Implicit construction from the standard ordering of the same
   * category or a stronger one (C++20).
   * @details A constructor template that accepts exactly the classes of
   * `<compare>` of that reach, so the conversions among the own classes are
   * not disturbed.
   * @tparam Std Deduced from the argument.
   * @param value The standard ordering.
   */
  template <typename Std,
            typename std::enable_if<
                Detail::is_std_ordering<Std>::value
                    && (Detail::std_ordering_rank<Std>::value >= 2),
                int>::type
            = 0>
  LUMEX_CONSTEXPR_CTOR
  strong_ordering (Std value) LUMEX_NOEXCEPT
      : Detail::ordering_base<strong_ordering> (
            static_cast<Detail::ordering_value> (
                Detail::std_ordering_value (value)))
  {
  }
#endif

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

namespace Detail
{
/// True for the three own ordering classes (no cv-qualifier, no reference).
template <typename T>
struct is_own_ordering
    : std::integral_constant<bool,
                             std::is_same<T, strong_ordering>::value
                                 || std::is_same<T, weak_ordering>::value
                                 || std::is_same<T, partial_ordering>::value>
{
};
} // namespace Detail

// The functions below are templates over the own classes, not functions of
// `partial_ordering`: with the converting constructors of C++20 a
// `partial_ordering` parameter would also take a `std::` ordering, and a call
// `is_eq (a <=> b)` in a scope that has `using namespace` of this namespace
// would be ambiguous with `std::is_eq` found through argument-dependent
// lookup.

/**
 * @brief True for `value == 0`: equivalent (equal).
 * @tparam Ordering `strong_ordering`, `weak_ordering` or `partial_ordering`
 * of this header (a standard ordering has `std::is_eq`).
 * @param[in] value The ordering.
 * @return true if `value` is `equivalent`.
 */
template <typename Ordering>
LUMEX_CONSTEXPR
    typename std::enable_if<Detail::is_own_ordering<Ordering>::value,
                            bool>::type
    is_eq (Ordering value) LUMEX_NOEXCEPT
{
  return partial_ordering (value) == partial_ordering::equivalent;
}

/**
 * @brief True for `value != 0`: not equivalent, `unordered` included.
 * @tparam Ordering An ordering class of this header.
 * @param[in] value The ordering.
 * @return true if `value` is not `equivalent`.
 */
template <typename Ordering>
LUMEX_CONSTEXPR
    typename std::enable_if<Detail::is_own_ordering<Ordering>::value,
                            bool>::type
    is_neq (Ordering value) LUMEX_NOEXCEPT
{
  return partial_ordering (value) != partial_ordering::equivalent;
}

/**
 * @brief True for `value < 0`: less.
 * @tparam Ordering An ordering class of this header.
 * @param[in] value The ordering.
 * @return true if `value` is `less`.
 */
template <typename Ordering>
LUMEX_CONSTEXPR
    typename std::enable_if<Detail::is_own_ordering<Ordering>::value,
                            bool>::type
    is_lt (Ordering value) LUMEX_NOEXCEPT
{
  return partial_ordering (value) == partial_ordering::less;
}

/**
 * @brief True for `value <= 0`: less or equivalent.
 * @tparam Ordering An ordering class of this header.
 * @param[in] value The ordering.
 * @return true if `value` is `less` or `equivalent`; false for `unordered`.
 */
template <typename Ordering>
LUMEX_CONSTEXPR
    typename std::enable_if<Detail::is_own_ordering<Ordering>::value,
                            bool>::type
    is_lteq (Ordering value) LUMEX_NOEXCEPT
{
  return partial_ordering (value) == partial_ordering::less
         || partial_ordering (value) == partial_ordering::equivalent;
}

/**
 * @brief True for `value > 0`: greater.
 * @tparam Ordering An ordering class of this header.
 * @param[in] value The ordering.
 * @return true if `value` is `greater`.
 */
template <typename Ordering>
LUMEX_CONSTEXPR
    typename std::enable_if<Detail::is_own_ordering<Ordering>::value,
                            bool>::type
    is_gt (Ordering value) LUMEX_NOEXCEPT
{
  return partial_ordering (value) == partial_ordering::greater;
}

/**
 * @brief True for `value >= 0`: greater or equivalent.
 * @tparam Ordering An ordering class of this header.
 * @param[in] value The ordering.
 * @return true if `value` is `greater` or `equivalent`; false for
 * `unordered`.
 */
template <typename Ordering>
LUMEX_CONSTEXPR
    typename std::enable_if<Detail::is_own_ordering<Ordering>::value,
                            bool>::type
    is_gteq (Ordering value) LUMEX_NOEXCEPT
{
  return partial_ordering (value) == partial_ordering::greater
         || partial_ordering (value) == partial_ordering::equivalent;
}

/// The strong ordering of this library: `strong_ordering`, in every standard.
using strong_ordering_t = strong_ordering;
/// The weak ordering of this library: `weak_ordering`, in every standard.
using weak_ordering_t = weak_ordering;
/// The partial ordering of this library: `partial_ordering`, in every
/// standard.
using partial_ordering_t = partial_ordering;

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
