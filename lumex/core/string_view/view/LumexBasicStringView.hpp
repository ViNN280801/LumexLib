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
 * @file LumexBasicStringView.hpp
 * @brief Header-only, C++11-first `basic_string_view`: the class template
 * behind `lumex_string_view`, `lumex_wstring_view` and the other character
 * views.
 * @details `lumex_basic_string_view<CharT, Traits>` is a non-owning view over
 * a contiguous character sequence with the API of `std::basic_string_view`
 * (C++17) plus what later standards added (`contains`, the range constructor,
 * `operator<=>`), available from C++11. Everything is inline: there is no
 * compiled part, no export macro and no library to link.
 *
 * Constant evaluation follows the language of each standard. From C++14 every
 * query is `constexpr`, and so are the modifiers (`remove_prefix`,
 * `remove_suffix`, `swap`, `clear`, `copy`). In C++11 the queries are
 * `constexpr` as well (a single `return` statement, loops replaced by a
 * recursion that splits the range in halves, so its depth is the logarithm of
 * the length), the modifiers are not, and `rbegin`/`rend` become `constexpr`
 * in C++17 (`std::reverse_iterator`). The run-time path never recurses: it is
 * a loop over `std::char_traits` (`memchr`, `memcmp` and their wide twins).
 *
 * @note The view does not manage the lifetime of the characters it refers to.
 * Using a view whose characters have been destroyed is undefined behavior.
 */
#ifndef LUMEX_CORE_STRING_VIEW_VIEW_BASIC_STRING_VIEW_HPP
#define LUMEX_CORE_STRING_VIEW_VIEW_BASIC_STRING_VIEW_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <cstddef>     // std::size_t, std::ptrdiff_t
#include <cstdint>     // std::uint64_t
#include <cstring>     // std::memcpy
#include <functional>  // std::hash
#include <ios>         // std::ios_base, std::streamsize
#include <iterator>    // std::reverse_iterator
#include <memory>      // std::allocator, std::to_address
#include <ostream>     // std::basic_ostream
#include <stdexcept>   // std::out_of_range
#include <streambuf>   // std::basic_streambuf
#include <string>      // std::basic_string, std::char_traits
#include <type_traits> // std::enable_if, std::is_same
#if __cplusplus >= 201703L
#include <string_view>
#endif
#if defined(__GLIBCXX__)
#include <cxxabi.h> // abi::__forced_unwind
#endif

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @def LUMEX_STRING_VIEW_HAS_STD_VIEW
 * @brief 1 when `std::basic_string_view` exists (C++17), 0 otherwise.
 * @details When it is 1, each view converts implicitly to and from the
 * standard one and compares with it.
 */
#if __cplusplus >= 201703L
#define LUMEX_STRING_VIEW_HAS_STD_VIEW 1
#else
#define LUMEX_STRING_VIEW_HAS_STD_VIEW 0
#endif

/**
 * @def LUMEX_STRING_VIEW_HAS_THREE_WAY
 * @brief 1 when `operator<=>` and `<compare>` are available, 0 otherwise.
 * @details Tested on the language and the library feature together: GCC 8
 * accepts `-std=c++2a` without them.
 */
#if __cplusplus >= 202002L && LUMEX_HAS_IMPL_THREE_WAY_COMPARISON             \
    && defined(__cpp_lib_three_way_comparison)
#include <compare>
#define LUMEX_STRING_VIEW_HAS_THREE_WAY 1
#else
#define LUMEX_STRING_VIEW_HAS_THREE_WAY 0
#endif

/**
 * @def LUMEX_STRING_VIEW_HAS_CONCEPTS
 * @brief 1 when the C++20 iterator concepts (`std::contiguous_iterator`,
 * `std::sized_sentinel_for`) are available, 0 otherwise.
 * @details When it is 1, the iterator-pair constructor accepts every
 * contiguous iterator, as the standard one does; otherwise only pointers.
 */
#if __cplusplus >= 202002L && LUMEX_HAS_CONCEPTS && LUMEX_HAS_STD_CONCEPTS
#include <iterator>
#define LUMEX_STRING_VIEW_HAS_CONCEPTS 1
#else
#define LUMEX_STRING_VIEW_HAS_CONCEPTS 0
#endif

/**
 * @def LUMEX_STRING_VIEW_HAS_RANGES
 * @brief 1 when `std::ranges::enable_view` and
 * `std::ranges::enable_borrowed_range` exist, 0 otherwise.
 * @details When it is 1, every view opts into both.
 */
#if LUMEX_STRING_VIEW_HAS_CONCEPTS && LUMEX_HAS_STD_RANGES                    \
    && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#define LUMEX_STRING_VIEW_HAS_RANGES 1
#endif
#endif
#ifndef LUMEX_STRING_VIEW_HAS_RANGES
#define LUMEX_STRING_VIEW_HAS_RANGES 0
#endif

// Private helper macros, removed at the end of the header.
//
// LUMEX_SV_CONSTANT_P (x) is true while a constant expression is evaluated.
// The standard library has std::is_constant_evaluated from C++20 only; the
// compilers have the builtin in every mode (GCC 9, Clang 9, MSVC 19.25), and
// GCC 8 folds __builtin_constant_p of a value that is constant in the
// evaluation. Without any of them the answer is "yes": the constant-expression
// code, which is correct at run time as well, is used everywhere.
#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_is_constant_evaluated)
#define LUMEX_SV_CONSTANT_P(x) (__builtin_is_constant_evaluated ())
#endif
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
#define LUMEX_SV_CONSTANT_P(x) (__builtin_is_constant_evaluated ())
#elif defined(_MSC_VER) && _MSC_VER >= 1925
#define LUMEX_SV_CONSTANT_P(x) (__builtin_is_constant_evaluated ())
#elif defined(__GNUC__)
#define LUMEX_SV_CONSTANT_P(x) (__builtin_constant_p (x))
#endif
#ifndef LUMEX_SV_CONSTANT_P
#define LUMEX_SV_CONSTANT_P(x) (true)
#endif

// constexpr from C++17 only (std::reverse_iterator).
#if __cplusplus >= 201703L
#define LUMEX_SV_CONSTEXPR_CXX17 constexpr
#else
#define LUMEX_SV_CONSTEXPR_CXX17
#endif

// The constexpr char_traits of C++17 are used when the standard library gets
// them right. GCC 8 evaluates the __builtin_memcmp behind
// char_traits<char>::compare as a signed comparison in a constant expression
// (a byte 0x80 is less than 'a'), so there the library's own code is used.
#if __cplusplus >= 201703L                                                    \
    && !(defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 9)
#define LUMEX_SV_CONSTEXPR_TRAITS 1
#else
#define LUMEX_SV_CONSTEXPR_TRAITS 0
#endif

// Binding the reference of catch (abi::__forced_unwind &) is reported by
// UndefinedBehaviorSanitizer as a reference to null (the exception object of a
// thread cancellation); libstdc++'s own inserters are not instrumented.
#if defined(__GLIBCXX__) && defined(__clang__)
#define LUMEX_SV_NO_UBSAN __attribute__ ((no_sanitize ("undefined")))
#elif defined(__GLIBCXX__) && defined(__GNUC__)
#define LUMEX_SV_NO_UBSAN __attribute__ ((no_sanitize_undefined))
#else
#define LUMEX_SV_NO_UBSAN
#endif

#define LUMEX_SV_NODISCARD                                                    \
  LUMEX_ATTRIBUTE_NODISCARD ("the result of a string view query is not used")

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
/**
 * @brief Core library components.
 * @details This namespace encapsulates fundamental, low-level utilities
 *          and data structures used across the Lumex Core library.
 */
namespace string_view
{
/**
 * @brief The string views: `lumex_basic_string_view` and its aliases.
 */
namespace view
{
template <class CharT, class Traits = std::char_traits<CharT>>
class lumex_basic_string_view;

/**
 * @brief Implementation details of the string views; not part of the API.
 */
namespace detail
{
/** @brief The value of `npos` of every view. */
LUMEX_CONSTEXPR std::size_t const no_pos = static_cast<std::size_t> (-1);

LUMEX_ATTRIBUTE_NORETURN inline void
throw_out_of_range (char const *what)
{
  throw std::out_of_range (what);
}

LUMEX_CONSTEXPR inline std::size_t
min_size (std::size_t lhs, std::size_t rhs) LUMEX_NOEXCEPT
{
  return rhs < lhs ? rhs : lhs;
}

/** @brief The result of `compare`: the characters, then the sizes. */
LUMEX_CONSTEXPR inline int
compare_result (int by_chars, std::size_t lhs_size,
                std::size_t rhs_size) LUMEX_NOEXCEPT
{
  return by_chars != 0
             ? by_chars
             : (lhs_size == rhs_size ? 0 : (lhs_size < rhs_size ? -1 : 1));
}

// Predicates of the scans: aggregates (literal types in C++11), called with an
// index into the viewed characters.
template <class Traits> struct eq_at
{
  typename Traits::char_type const *data;
  typename Traits::char_type chr;
  LUMEX_CONSTEXPR bool
  operator() (std::size_t idx) const LUMEX_NOEXCEPT
  {
    return Traits::eq (data[idx], chr);
  }
};

template <class Traits> struct ne_at
{
  typename Traits::char_type const *data;
  typename Traits::char_type chr;
  LUMEX_CONSTEXPR bool
  operator() (std::size_t idx) const LUMEX_NOEXCEPT
  {
    return !Traits::eq (data[idx], chr);
  }
};

template <class Traits> struct diff_at
{
  typename Traits::char_type const *lhs;
  typename Traits::char_type const *rhs;
  LUMEX_CONSTEXPR bool
  operator() (std::size_t idx) const LUMEX_NOEXCEPT
  {
    return Traits::lt (lhs[idx], rhs[idx]) || Traits::lt (rhs[idx], lhs[idx]);
  }
};

// The scans: the first (last) index of [lo, hi) that satisfies pred, else
// no_pos. From C++14 they are loops. In C++11 a constexpr function is a
// single return statement, so constant evaluation uses a recursion over
// halves (the depth is log2 of the length) and run time a plain loop.
#if __cplusplus >= 201402L
template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_fwd (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  for (; lo < hi; ++lo)
    if (pred (lo))
      return lo;
  return no_pos;
}

template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_bwd (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  for (; hi > lo; --hi)
    if (pred (hi - 1))
      return hi - 1;
  return no_pos;
}
#else
template <class Pred>
LUMEX_CONSTEXPR std::size_t scan_fwd_rec (Pred const &pred, std::size_t lo,
                                          std::size_t hi) LUMEX_NOEXCEPT;

template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_fwd_pick (std::size_t found, Pred const &pred, std::size_t mid,
               std::size_t hi) LUMEX_NOEXCEPT
{
  return found != no_pos ? found : scan_fwd_rec (pred, mid, hi);
}

template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_fwd_rec (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  return hi <= lo
             ? no_pos
             : (hi - lo == 1 ? (pred (lo) ? lo : no_pos)
                             : scan_fwd_pick (
                                   scan_fwd_rec (pred, lo, lo + (hi - lo) / 2),
                                   pred, lo + (hi - lo) / 2, hi));
}

template <class Pred>
LUMEX_CONSTEXPR std::size_t scan_bwd_rec (Pred const &pred, std::size_t lo,
                                          std::size_t hi) LUMEX_NOEXCEPT;

template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_bwd_pick (std::size_t found, Pred const &pred, std::size_t lo,
               std::size_t mid) LUMEX_NOEXCEPT
{
  return found != no_pos ? found : scan_bwd_rec (pred, lo, mid);
}

template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_bwd_rec (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  return hi <= lo
             ? no_pos
             : (hi - lo == 1 ? (pred (lo) ? lo : no_pos)
                             : scan_bwd_pick (
                                   scan_bwd_rec (pred, lo + (hi - lo) / 2, hi),
                                   pred, lo, lo + (hi - lo) / 2));
}

template <class Pred>
std::size_t
scan_fwd_loop (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  for (; lo < hi; ++lo)
    if (pred (lo))
      return lo;
  return no_pos;
}

template <class Pred>
std::size_t
scan_bwd_loop (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  for (; hi > lo; --hi)
    if (pred (hi - 1))
      return hi - 1;
  return no_pos;
}

template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_fwd (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  return LUMEX_SV_CONSTANT_P (hi - lo) ? scan_fwd_rec (pred, lo, hi)
                                       : scan_fwd_loop (pred, lo, hi);
}

template <class Pred>
LUMEX_CONSTEXPR std::size_t
scan_bwd (Pred const &pred, std::size_t lo, std::size_t hi) LUMEX_NOEXCEPT
{
  return LUMEX_SV_CONSTANT_P (hi - lo) ? scan_bwd_rec (pred, lo, hi)
                                       : scan_bwd_loop (pred, lo, hi);
}
#endif

// The character functions of Traits that a view needs: the length of a C
// string, the order of two ranges, a character in a range. char_traits is
// constexpr from C++17 (and the libraries implement it with the builtins);
// before that constant evaluation uses the code below and run time calls
// char_traits (memchr, memcmp and the wide twins).
template <class Traits> struct chars
{
  typedef typename Traits::char_type char_type;

#if defined(__GNUC__) || defined(__clang__)
  // A narrow string: the builtin is constant in every mode and the libc call
  // at run time.
  template <class C>
  static LUMEX_CONSTEXPR std::size_t
  length_of (C const *str, std::true_type) LUMEX_NOEXCEPT
  {
    return __builtin_strlen (str);
  }
  typedef std::integral_constant<
      bool, std::is_same<Traits, std::char_traits<char>>::value>
      narrow_t;
#else
  typedef std::false_type narrow_t;
#endif

#if LUMEX_SV_CONSTEXPR_TRAITS
  template <class C>
  static LUMEX_CONSTEXPR std::size_t
  length_of (C const *str, std::false_type) LUMEX_NOEXCEPT
  {
    return Traits::length (str);
  }

  static LUMEX_CONSTEXPR int
  compare (char_type const *lhs, char_type const *rhs,
           std::size_t count) LUMEX_NOEXCEPT
  {
    return count == 0 ? 0 : Traits::compare (lhs, rhs, count);
  }

  static LUMEX_CONSTEXPR char_type const *
  find (char_type const *str, std::size_t count, char_type chr) LUMEX_NOEXCEPT
  {
    return count == 0 ? nullptr : Traits::find (str, count, chr);
  }
#elif __cplusplus >= 201402L
  // C++14: loops are allowed, so the constant-evaluation code is a loop. The
  // choice between it and char_traits is a conditional expression, not an
  // if statement: GCC 8 folds __builtin_constant_p only in the former.
  static LUMEX_CONSTEXPR std::size_t
  length_ct (char_type const *str) LUMEX_NOEXCEPT
  {
    std::size_t len = 0;
    while (!Traits::eq (str[len], char_type ()))
      ++len;
    return len;
  }

  template <class C>
  static LUMEX_CONSTEXPR std::size_t
  length_of (C const *str, std::false_type) LUMEX_NOEXCEPT
  {
    return LUMEX_SV_CONSTANT_P (*str) ? length_ct (str) : Traits::length (str);
  }

  static LUMEX_CONSTEXPR int
  compare_ct (char_type const *lhs, char_type const *rhs,
              std::size_t count) LUMEX_NOEXCEPT
  {
    for (std::size_t i = 0; i < count; ++i)
      {
        if (Traits::lt (lhs[i], rhs[i]))
          return -1;
        if (Traits::lt (rhs[i], lhs[i]))
          return 1;
      }
    return 0;
  }

  static LUMEX_CONSTEXPR int
  compare (char_type const *lhs, char_type const *rhs,
           std::size_t count) LUMEX_NOEXCEPT
  {
    return count == 0 ? 0
                      : (LUMEX_SV_CONSTANT_P (count)
                             ? compare_ct (lhs, rhs, count)
                             : Traits::compare (lhs, rhs, count));
  }

  static LUMEX_CONSTEXPR char_type const *
  find_ct (char_type const *str, std::size_t count,
           char_type chr) LUMEX_NOEXCEPT
  {
    for (std::size_t i = 0; i < count; ++i)
      if (Traits::eq (str[i], chr))
        return str + i;
    return nullptr;
  }

  static LUMEX_CONSTEXPR char_type const *
  find (char_type const *str, std::size_t count, char_type chr) LUMEX_NOEXCEPT
  {
    return count == 0 ? nullptr
                      : (LUMEX_SV_CONSTANT_P (count)
                             ? find_ct (str, count, chr)
                             : Traits::find (str, count, chr));
  }
#else
  // The length in steps of 4 characters: the recursion depth is length / 4.
  static LUMEX_CONSTEXPR std::size_t
  length_rec (char_type const *str, std::size_t base) LUMEX_NOEXCEPT
  {
    return Traits::eq (str[base], char_type ())
               ? base
               : (Traits::eq (str[base + 1], char_type ())
                      ? base + 1
                      : (Traits::eq (str[base + 2], char_type ())
                             ? base + 2
                             : (Traits::eq (str[base + 3], char_type ())
                                    ? base + 3
                                    : length_rec (str, base + 4))));
  }

  template <class C>
  static LUMEX_CONSTEXPR std::size_t
  length_of (C const *str, std::false_type) LUMEX_NOEXCEPT
  {
    return LUMEX_SV_CONSTANT_P (*str) ? length_rec (str, 0)
                                      : Traits::length (str);
  }

  static LUMEX_CONSTEXPR int
  order_at (char_type const *lhs, char_type const *rhs,
            std::size_t idx) LUMEX_NOEXCEPT
  {
    return idx == no_pos ? 0 : (Traits::lt (lhs[idx], rhs[idx]) ? -1 : 1);
  }

  static LUMEX_CONSTEXPR int
  compare (char_type const *lhs, char_type const *rhs,
           std::size_t count) LUMEX_NOEXCEPT
  {
    return count == 0
               ? 0
               : (LUMEX_SV_CONSTANT_P (count)
                      ? order_at (lhs, rhs,
                                  scan_fwd_rec (diff_at<Traits>{ lhs, rhs }, 0,
                                                count))
                      : Traits::compare (lhs, rhs, count));
  }

  static LUMEX_CONSTEXPR char_type const *
  pointer_at (char_type const *str, std::size_t idx) LUMEX_NOEXCEPT
  {
    return idx == no_pos ? nullptr : str + idx;
  }

  static LUMEX_CONSTEXPR char_type const *
  find (char_type const *str, std::size_t count, char_type chr) LUMEX_NOEXCEPT
  {
    return count == 0
               ? nullptr
               : (LUMEX_SV_CONSTANT_P (count)
                      ? pointer_at (
                            str,
                            scan_fwd_rec (eq_at<Traits>{ str, chr }, 0, count))
                      : Traits::find (str, count, chr));
  }
#endif

  /** @brief The length of a non-null C string. */
  static LUMEX_CONSTEXPR std::size_t
  length (char_type const *str) LUMEX_NOEXCEPT
  {
    return length_of (str, narrow_t ());
  }
};

/**
 * @brief The searches of a view over `Traits`.
 */
template <class Traits> struct search
{
  typedef typename Traits::char_type char_type;
  typedef chars<Traits> chars_t;

  /** @brief `data[idx..idx + count)` equals `needle[0..count)`. */
  struct match_at
  {
    char_type const *data;
    char_type const *needle;
    std::size_t count;
    LUMEX_CONSTEXPR bool
    operator() (std::size_t idx) const LUMEX_NOEXCEPT
    {
      return chars_t::compare (data + idx, needle, count) == 0;
    }
  };

  /** @brief `data[idx]` is one of the characters of the set. */
  struct in_set_at
  {
    char_type const *data;
    char_type const *set;
    std::size_t set_size;
    LUMEX_CONSTEXPR bool
    operator() (std::size_t idx) const LUMEX_NOEXCEPT
    {
      return chars_t::find (set, set_size, data[idx]) != nullptr;
    }
  };

  /** @brief `data[idx]` is none of the characters of the set. */
  struct not_in_set_at
  {
    char_type const *data;
    char_type const *set;
    std::size_t set_size;
    LUMEX_CONSTEXPR bool
    operator() (std::size_t idx) const LUMEX_NOEXCEPT
    {
      return chars_t::find (set, set_size, data[idx]) == nullptr;
    }
  };

  /** @brief The index of `found` in `base`, or `no_pos` for null. */
  static LUMEX_CONSTEXPR std::size_t
  index_of (char_type const *base, char_type const *found) LUMEX_NOEXCEPT
  {
    return found == nullptr ? no_pos : static_cast<std::size_t> (found - base);
  }

  /**
   * @brief The first match of `needle` (`count >= 1`) at an index of
   * `[pos, size - count]`; requires `pos <= size - count`.
   * @details The first character is looked up with `char_traits::find`
   * (`memchr`) and the rest is compared only where it was found: a text
   * without the first character costs one call.
   */
  static LUMEX_CONSTEXPR_CXX14 std::size_t
  find_view_loop (char_type const *data, std::size_t size,
                  char_type const *needle, std::size_t count,
                  std::size_t pos) LUMEX_NOEXCEPT
  {
#if __cplusplus >= 201402L
    std::size_t const last = size - count;
    while (true)
      {
        char_type const *const head
            = chars_t::find (data + pos, last - pos + 1, needle[0]);
        if (head == nullptr)
          return no_pos;
        if (chars_t::compare (head + 1, needle + 1, count - 1) == 0)
          return static_cast<std::size_t> (head - data);
        pos = static_cast<std::size_t> (head - data) + 1;
        if (pos > last)
          return no_pos;
      }
#else
    // Run time of C++11: char_traits directly.
    std::size_t const last = size - count;
    while (true)
      {
        char_type const *const head
            = Traits::find (data + pos, last - pos + 1, needle[0]);
        if (head == nullptr)
          return no_pos;
        if (Traits::compare (head + 1, needle + 1, count - 1) == 0)
          return static_cast<std::size_t> (head - data);
        pos = static_cast<std::size_t> (head - data) + 1;
        if (pos > last)
          return no_pos;
      }
#endif
  }

  static LUMEX_CONSTEXPR std::size_t
  find_view (char_type const *data, std::size_t size, char_type const *needle,
             std::size_t count, std::size_t pos) LUMEX_NOEXCEPT
  {
#if __cplusplus >= 201402L
    return find_view_loop (data, size, needle, count, pos);
#else
    return LUMEX_SV_CONSTANT_P (size)
               ? scan_fwd_rec (match_at{ data, needle, count }, pos,
                               size - count + 1)
               : find_view_loop (data, size, needle, count, pos);
#endif
  }
};

/**
 * @brief The hash of the characters of a view: the same value in every C++
 * standard, with every compiler and standard library.
 * @details A 64-bit block hash (the rounds of xxHash64 over 8-byte blocks and
 * the finalizer of MurmurHash3), the length is mixed in, so a string and the
 * same string followed by NULs differ. It is not the hash of
 * `std::hash<std::basic_string_view>` (whose value is the choice of the
 * standard library and changes with it): a library built at C++11 and a
 * program built at C++17 must hash a key alike.
 *
 * What is hashed: for one-byte characters (`char`, `char8_t`) the bytes, read
 * eight at a time as little-endian integers (so a big-endian machine gives the
 * same value, through a byte swap); for wider characters (`wchar_t`,
 * `char16_t`, `char32_t`) the code unit values, each zero-extended to 32 bits
 * and two of them packed into one block (so a `wchar_t` string and a
 * `char32_t` string of the same code points hash alike, and `wchar_t` of 2
 * bytes (Windows) and 4 bytes give the same value for the same code units;
 * a unit above 2^32 is reduced modulo 2^32). The value is a 64-bit one;
 * on a 32-bit platform `size_t` takes its low 32 bits.
 */
inline std::uint64_t
hash_round (std::uint64_t acc, std::uint64_t block) LUMEX_NOEXCEPT
{
  block *= 0xC2B2AE3D27D4EB4FULL;
  block = (block << 31) | (block >> 33);
  block *= 0x9E3779B185EBCA87ULL;
  acc ^= block;
  acc = ((acc << 27) | (acc >> 37)) * 0x9E3779B185EBCA87ULL
        + 0x85EBCA77C2B2AE63ULL;
  return acc;
}

inline std::uint64_t
hash_final (std::uint64_t acc) LUMEX_NOEXCEPT
{
  acc ^= acc >> 33;
  acc *= 0xFF51AFD7ED558CCDULL;
  acc ^= acc >> 33;
  acc *= 0xC4CEB9FE1A85EC53ULL;
  acc ^= acc >> 33;
  return acc;
}

/** @brief Eight bytes as a little-endian integer, on any byte order. */
inline std::uint64_t
load_le64 (unsigned char const *bytes) LUMEX_NOEXCEPT
{
  std::uint64_t value;
  std::memcpy (&value, bytes, sizeof value);
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)                  \
    && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  value = ((value & 0x00000000000000FFULL) << 56)
          | ((value & 0x000000000000FF00ULL) << 40)
          | ((value & 0x0000000000FF0000ULL) << 24)
          | ((value & 0x00000000FF000000ULL) << 8)
          | ((value & 0x000000FF00000000ULL) >> 8)
          | ((value & 0x0000FF0000000000ULL) >> 24)
          | ((value & 0x00FF000000000000ULL) >> 40)
          | ((value & 0xFF00000000000000ULL) >> 56);
#endif
  return value;
}

template <class CharT>
std::uint64_t
block_hash (CharT const *data, std::size_t size, std::true_type) LUMEX_NOEXCEPT
{
  // One-byte characters: the bytes, eight per round.
  unsigned char const *bytes = reinterpret_cast<unsigned char const *> (data);
  std::uint64_t acc = 0x27D4EB2F165667C5ULL ^ (size * 0x9E3779B97F4A7C15ULL);
  std::size_t at = 0;
  for (; at + 8 <= size; at += 8)
    acc = hash_round (acc, load_le64 (bytes + at));
  if (at < size)
    {
      std::uint64_t tail = 0;
      for (std::size_t i = 0; at + i < size; ++i)
        tail |= static_cast<std::uint64_t> (bytes[at + i]) << (8 * i);
      acc = hash_round (acc, tail);
    }
  return hash_final (acc);
}

template <class CharT>
std::uint64_t
block_hash (CharT const *data, std::size_t size,
            std::false_type) LUMEX_NOEXCEPT
{
  // Wider characters: the code unit values, two per round.
  typedef typename std::make_unsigned<CharT>::type unit_t;
  std::uint64_t acc = 0x27D4EB2F165667C5ULL ^ (size * 0x9E3779B97F4A7C15ULL);
  std::size_t at = 0;
  for (; at + 2 <= size; at += 2)
    {
      std::uint64_t const low
          = static_cast<std::uint32_t> (static_cast<unit_t> (data[at]));
      std::uint64_t const high
          = static_cast<std::uint32_t> (static_cast<unit_t> (data[at + 1]));
      acc = hash_round (acc, low | (high << 32));
    }
  if (at < size)
    acc = hash_round (
        acc, static_cast<std::uint32_t> (static_cast<unit_t> (data[at])));
  return hash_final (acc);
}

/** @brief The hash of `size` characters at `data` (may be null if size is 0).
 */
template <class CharT>
std::size_t
hash_chars (CharT const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return static_cast<std::size_t> (block_hash (
      data, size, std::integral_constant<bool, sizeof (CharT) == 1> ()));
}

// Which types a view accepts in its range constructor (C++23 in the standard,
// every standard here): a type with data () and size (), whose elements are
// the view's characters, that is not itself convertible to the view, to a
// pointer to characters or to a C++ string.
template <class Range, class CharT, class Traits, class = void>
struct is_char_range : std::false_type
{
};

template <class...> struct make_void
{
  typedef void type;
};

// A range type that names its traits must name the view's.
template <class T, class Traits, class = void>
struct traits_match : std::true_type
{
};

template <class T, class Traits>
struct traits_match<T, Traits,
                    typename make_void<typename T::traits_type>::type>
    : std::is_same<typename T::traits_type, Traits>
{
};

template <class Range, class CharT, class Traits>
struct is_char_range<
    Range, CharT, Traits,
    typename std::enable_if<
        std::is_convertible<decltype (std::declval<Range const &> ().data ()),
                            CharT const *>::value
        && std::is_convertible<
            decltype (std::declval<Range const &> ().size ()),
            std::size_t>::value
        && std::is_same<typename std::remove_cv<typename std::remove_pointer<
                            decltype (std::declval<Range const &> ()
                                          .data ())>::type>::type,
                        CharT>::value
        && traits_match<Range, Traits>::value
        && !std::is_convertible<Range const &, CharT const *>::value
        && !std::is_convertible<
            Range const &, lumex_basic_string_view<CharT, Traits>>::value>::
        type> : std::true_type
{
};

#if LUMEX_STRING_VIEW_HAS_THREE_WAY
template <class Traits, class = void> struct comparison_category
{
  // [string.view.comparison]: traits::comparison_category if it names a type,
  // else weak_ordering (std::char_traits names strong_ordering).
  typedef std::weak_ordering type;
};

template <class Traits>
struct comparison_category<
    Traits, typename make_void<typename Traits::comparison_category>::type>
{
  typedef typename Traits::comparison_category type;
};
#endif

/** @brief Writes `count` copies of `fill` to `buf`; false when it fails. */
template <class CharT, class Traits>
bool
put_fill (std::basic_streambuf<CharT, Traits> *buf, CharT fill,
          std::streamsize count)
{
  for (; count > 0; --count)
    if (Traits::eq_int_type (buf->sputc (fill), Traits::eof ()))
      return false;
  return true;
}
} // namespace detail

/**
 * @brief A lightweight, non-owning view over a contiguous sequence of
 * characters; the equivalent of `std::basic_string_view` from C++11.
 * @details The view holds a pointer and a length. It is immutable (the
 * characters cannot be changed through it, only the bounds of the view), cheap
 * to copy (trivially copyable) and never allocates. The API is the one of
 * `std::basic_string_view` of the newest standard, with these differences:
 *  - a `nullptr` C string gives an empty view (the standard leaves it
 *    undefined) and `remove_prefix`/`remove_suffix` clamp to the size;
 *  - `contains`, the range constructor and `operator<=>` are available below
 *    the standard that added them (see the header for the iterator-pair
 *    constructor);
 *  - `clear()`, `to_string()` and the conversion to `std::basic_string` are
 *    additions of the library;
 *  - the constructor from a `std::basic_string` is implicit, as `std::string`
 *    converts to `std::string_view`.
 *
 * @tparam CharT The character type.
 * @tparam Traits The character traits; `Traits::char_type` is `CharT`.
 * @warning The view does not own the characters. Dangling views are undefined
 * behavior.
 */
template <class CharT, class Traits> class lumex_basic_string_view
{
  static_assert (std::is_same<typename Traits::char_type, CharT>::value,
                 "Traits::char_type must be CharT");

  typedef detail::chars<Traits> chars_t;
  typedef detail::search<Traits> search_t;

public:
  // -- Types --
  /** @brief The traits of the characters. */
  typedef Traits traits_type;
  /** @brief The character type. */
  typedef CharT value_type;
  /** @brief A pointer to a const character. */
  typedef CharT const *pointer;
  /** @brief A pointer to a const character. */
  typedef CharT const *const_pointer;
  /** @brief A const reference to a character. */
  typedef CharT const &reference;
  /** @brief A const reference to a character. */
  typedef CharT const &const_reference;
  /** @brief The iterator: a pointer, as the view is constant. */
  typedef const_pointer iterator;
  /** @brief The const iterator: a pointer. */
  typedef const_pointer const_iterator;
  /** @brief The reverse iterator. */
  typedef std::reverse_iterator<const_iterator> reverse_iterator;
  /** @brief The const reverse iterator. */
  typedef reverse_iterator const_reverse_iterator;
  /** @brief The size type. */
  typedef std::size_t size_type;
  /** @brief The difference type. */
  typedef std::ptrdiff_t difference_type;

  /**
   * @brief "Not a position": the result of a failed search and the default
   * length of `substr`.
   */
  static LUMEX_CONSTEXPR size_type const npos = detail::no_pos;

  // -- Construction --
  /**
   * @brief Creates an empty view: `data()` is `nullptr`, `size()` is `0`.
   */
  LUMEX_CONSTEXPR
  lumex_basic_string_view () LUMEX_NOEXCEPT : m_data (nullptr), m_size (0) {}

  /**
   * @brief Views a null-terminated string, up to the terminator.
   * @details Implicit, like the standard one: a literal or a string can be
   * passed where a view is expected. `nullptr` gives an empty view.
   * @param str The string.
   * @note Complexity: O(N), `strlen`.
   */
  LUMEX_CONSTEXPR
  lumex_basic_string_view (
      CharT const *str) // NOLINT(google-explicit-constructor)
      LUMEX_NOEXCEPT : m_data (str),
                       m_size (str != nullptr ? chars_t::length (str) : 0)
  {
  }

  /**
   * @brief Views `len` characters from `str`.
   * @details `str` may be `nullptr` when `len` is 0.
   */
  LUMEX_CONSTEXPR
  lumex_basic_string_view (CharT const *str, size_type len) LUMEX_NOEXCEPT
      : m_data (str),
        m_size (len)
  {
  }

  /**
   * @brief Views the characters of a `std::basic_string` (implicit).
   * @param str The string; it must outlive the view.
   */
  template <class Allocator>
  LUMEX_CONSTEXPR
  lumex_basic_string_view ( // NOLINT(google-explicit-constructor)
      std::basic_string<CharT, Traits, Allocator> const &str) LUMEX_NOEXCEPT
      : m_data (str.data ()),
        m_size (str.size ())
  {
  }

#if LUMEX_STRING_VIEW_HAS_STD_VIEW
  /**
   * @brief Converts from the standard view (C++17, implicit).
   * @details A template that accepts exactly `std::basic_string_view<CharT,
   * Traits>`, so that no other type converts through it.
   */
  template <
      class StdView,
      typename std::enable_if<
          std::is_same<StdView, std::basic_string_view<CharT, Traits>>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR
  lumex_basic_string_view (StdView view) // NOLINT(google-explicit-constructor)
      LUMEX_NOEXCEPT : m_data (view.data ()),
                       m_size (view.size ())
  {
  }
#endif

#if LUMEX_STRING_VIEW_HAS_CONCEPTS
  /**
   * @brief Views `[first, last)` (C++20 constructor, as the standard).
   * @details Requires a contiguous iterator of `CharT` and a sized sentinel
   * for it that does not convert to `size_type`.
   */
  template <std::contiguous_iterator It, std::sized_sentinel_for<It> End>
    requires (std::is_same_v<std::iter_value_t<It>, CharT>
              && !std::is_convertible_v<End, size_type>)
  LUMEX_CONSTEXPR
  lumex_basic_string_view (It first, End last) LUMEX_NOEXCEPT_IF (
      noexcept (last - first) && noexcept (std::to_address (first)))
      : m_data (std::to_address (first)),
        m_size (static_cast<size_type> (last - first))
  {
  }
#else
  /**
   * @brief Views `[first, last)`; below C++20 the iterators are pointers.
   * @details The standard constructor takes any contiguous iterator, which
   * cannot be recognized before C++20; the library accepts the pointers to
   * `CharT` (const or not), the form that is contiguous by construction.
   */
  template <class It, class End,
            typename std::enable_if<
                std::is_pointer<It>::value && std::is_pointer<End>::value
                    && std::is_convertible<It, CharT const *>::value
                    && std::is_convertible<End, CharT const *>::value,
                int>::type
            = 0>
  LUMEX_CONSTEXPR
  lumex_basic_string_view (It first, End last) LUMEX_NOEXCEPT
      : m_data (first),
        m_size (static_cast<size_type> (last - first))
  {
  }
#endif

  /**
   * @brief Views a contiguous range (explicit; C++23 constructor, here from
   * C++11).
   * @details Accepts a type with `data()` and `size()` whose elements are
   * `CharT`, such as `std::vector<char>`, `std::array<char, N>` or a span of
   * characters. A type that converts to the view, to a pointer to characters
   * or to a `std::basic_string`, and one whose `traits_type` differs, is not
   * accepted. The view refers to the range: it must outlive the view.
   */
  template <class Range,
            typename std::enable_if<
                detail::is_char_range<
                    typename std::remove_cv<
                        typename std::remove_reference<Range>::type>::type,
                    CharT, Traits>::value,
                int>::type
            = 0>
  explicit LUMEX_CONSTEXPR
  lumex_basic_string_view (Range &&range)
      LUMEX_NOEXCEPT_IF (noexcept (range.data ()) && noexcept (range.size ()))
      : m_data (range.data ()), m_size (static_cast<size_type> (range.size ()))
  {
  }

  // -- Iterators --
  /** @brief The iterator to the first character. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_iterator
  begin () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

  /** @brief The const iterator to the first character. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_iterator
  cbegin () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

  /** @brief The iterator past the last character. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_iterator
  end () const LUMEX_NOEXCEPT
  {
    return m_data
           + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /** @brief The const iterator past the last character. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_iterator
  cend () const LUMEX_NOEXCEPT
  {
    return m_data
           + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /** @brief The reverse iterator to the last character (`constexpr` from
   * C++17). */
  LUMEX_SV_NODISCARD
  LUMEX_SV_CONSTEXPR_CXX17 const_reverse_iterator
  rbegin () const LUMEX_NOEXCEPT
  {
    return const_reverse_iterator (end ());
  }

  /** @brief The const reverse iterator to the last character. */
  LUMEX_SV_NODISCARD
  LUMEX_SV_CONSTEXPR_CXX17 const_reverse_iterator
  crbegin () const LUMEX_NOEXCEPT
  {
    return const_reverse_iterator (end ());
  }

  /** @brief The reverse iterator before the first character. */
  LUMEX_SV_NODISCARD
  LUMEX_SV_CONSTEXPR_CXX17 const_reverse_iterator
  rend () const LUMEX_NOEXCEPT
  {
    return const_reverse_iterator (begin ());
  }

  /** @brief The const reverse iterator before the first character. */
  LUMEX_SV_NODISCARD
  LUMEX_SV_CONSTEXPR_CXX17 const_reverse_iterator
  crend () const LUMEX_NOEXCEPT
  {
    return const_reverse_iterator (begin ());
  }

  // -- Capacity --
  /** @brief The number of characters. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  size () const LUMEX_NOEXCEPT
  {
    return m_size;
  }

  /** @brief The number of characters (alias of `size()`). */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  length () const LUMEX_NOEXCEPT
  {
    return m_size;
  }

  /** @brief The largest possible size. */
  LUMEX_SV_NODISCARD
  static LUMEX_CONSTEXPR size_type
  max_size () LUMEX_NOEXCEPT
  {
    return npos / 2;
  }

  /** @brief `true` when the view has no characters. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  empty () const LUMEX_NOEXCEPT
  {
    return m_size == 0;
  }

  // -- Element access --
  /** @brief The character at `idx`; no bounds check. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_reference
  operator[] (size_type idx) const LUMEX_NOEXCEPT
  {
    return m_data
        [idx]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /**
   * @brief The character at `idx`.
   * @throws std::out_of_range If `idx >= size()`.
   */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_reference
  at (size_type idx) const
  {
    return idx < m_size
               ? m_data[idx]
               : (detail::throw_out_of_range ("lumex_basic_string_view::at"),
                  m_data[0]);
  }

  /** @brief The first character; undefined for an empty view. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_reference
  front () const LUMEX_NOEXCEPT
  {
    return m_data[0];
  }

  /** @brief The last character; undefined for an empty view. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_reference
  back () const LUMEX_NOEXCEPT
  {
    return m_data[m_size - 1];
  }

  /** @brief The pointer to the first character; null for a default view. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR const_pointer
  data () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

  // -- Modifiers (of the view, not of the characters; constexpr from C++14) --
  /** @brief Makes the view empty: `data()` is `nullptr`, `size()` is `0`. */
  LUMEX_CONSTEXPR_CXX14 void
  clear () LUMEX_NOEXCEPT
  {
    m_data = nullptr;
    m_size = 0;
  }

  /** @brief Drops the first `n` characters (all of them when `n > size()`). */
  LUMEX_CONSTEXPR_CXX14 void
  remove_prefix (size_type n) LUMEX_NOEXCEPT
  {
    n = detail::min_size (n, m_size);
    m_data += n;
    m_size -= n;
  }

  /** @brief Drops the last `n` characters (all of them when `n > size()`). */
  LUMEX_CONSTEXPR_CXX14 void
  remove_suffix (size_type n) LUMEX_NOEXCEPT
  {
    m_size -= detail::min_size (n, m_size);
  }

  /** @brief Exchanges the pointer and the size with another view. */
  LUMEX_CONSTEXPR_CXX14 void
  swap (lumex_basic_string_view &other) LUMEX_NOEXCEPT
  {
    const_pointer const data_copy = m_data;
    size_type const size_copy = m_size;
    m_data = other.m_data;
    m_size = other.m_size;
    other.m_data = data_copy;
    other.m_size = size_copy;
  }

  // -- Operations --
  /**
   * @brief Copies `min(count, size() - pos)` characters from `pos` to `dest`.
   * @return The number of characters copied.
   * @throws std::out_of_range If `pos > size()`.
   */
  LUMEX_CONSTEXPR_CXX14 size_type
  copy (CharT *dest, size_type count, size_type pos = 0) const
  {
    if (pos > m_size)
      detail::throw_out_of_range ("lumex_basic_string_view::copy");
    size_type const rlen = detail::min_size (count, m_size - pos);
    for (size_type i = 0; i < rlen; ++i)
      dest[i] = m_data
          [pos + i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return rlen;
  }

  /**
   * @brief The view of `min(n, size() - pos)` characters from `pos`.
   * @throws std::out_of_range If `pos > size()`.
   */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR lumex_basic_string_view
  substr (size_type pos = 0, size_type n = npos) const
  {
    return pos > m_size
               ? (detail::throw_out_of_range (
                      "lumex_basic_string_view::substr"),
                  lumex_basic_string_view ())
               : lumex_basic_string_view (m_data + pos,
                                          detail::min_size (n, m_size - pos));
  }

  // -- Comparison --
  /**
   * @brief Lexicographic comparison: negative, zero or positive.
   * @details The characters are compared as `Traits::lt` orders them (for
   * `char` as `unsigned char`, whatever the signedness of `char`) and an
   * embedded `'\0'` is an ordinary character; of two views with a common
   * prefix the shorter is less.
   */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR int
  compare (lumex_basic_string_view other) const LUMEX_NOEXCEPT
  {
    return detail::compare_result (
        chars_t::compare (m_data, other.m_data,
                          detail::min_size (m_size, other.m_size)),
        m_size, other.m_size);
  }

  /** @brief `substr(pos1, n1).compare(other)`. @throws std::out_of_range If
   * `pos1 > size()`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR int
  compare (size_type pos1, size_type n1, lumex_basic_string_view other) const
  {
    return substr (pos1, n1).compare (other);
  }

  /**
   * @brief `substr(pos1, n1).compare(other.substr(pos2, n2))`.
   * @throws std::out_of_range If `pos1 > size()` or `pos2 > other.size()`.
   */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR int
  compare (size_type pos1, size_type n1, lumex_basic_string_view other,
           size_type pos2, size_type n2) const
  {
    return substr (pos1, n1).compare (other.substr (pos2, n2));
  }

  /** @brief `compare(lumex_basic_string_view(str))`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR int
  compare (CharT const *str) const
  {
    return compare (lumex_basic_string_view (str));
  }

  /** @brief `substr(pos1, n1).compare(lumex_basic_string_view(str))`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR int
  compare (size_type pos1, size_type n1, CharT const *str) const
  {
    return substr (pos1, n1).compare (lumex_basic_string_view (str));
  }

  /** @brief `substr(pos1, n1).compare(lumex_basic_string_view(str, n2))`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR int
  compare (size_type pos1, size_type n1, CharT const *str, size_type n2) const
  {
    return substr (pos1, n1).compare (lumex_basic_string_view (str, n2));
  }

  // -- Prefix, suffix, containment --
  /** @brief The view begins with `prefix`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  starts_with (lumex_basic_string_view prefix) const LUMEX_NOEXCEPT
  {
    return m_size >= prefix.m_size
           && chars_t::compare (m_data, prefix.m_data, prefix.m_size) == 0;
  }

  /** @brief The view begins with the character `chr`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  starts_with (CharT chr) const LUMEX_NOEXCEPT
  {
    return !empty () && Traits::eq (front (), chr);
  }

  /** @brief The view begins with the null-terminated `prefix`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  starts_with (CharT const *prefix) const
  {
    return starts_with (lumex_basic_string_view (prefix));
  }

  /** @brief The view ends with `suffix`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  ends_with (lumex_basic_string_view suffix) const LUMEX_NOEXCEPT
  {
    return m_size >= suffix.m_size
           && chars_t::compare (m_data + (m_size - suffix.m_size),
                                suffix.m_data, suffix.m_size)
                  == 0;
  }

  /** @brief The view ends with the character `chr`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  ends_with (CharT chr) const LUMEX_NOEXCEPT
  {
    return !empty () && Traits::eq (back (), chr);
  }

  /** @brief The view ends with the null-terminated `suffix`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  ends_with (CharT const *suffix) const
  {
    return ends_with (lumex_basic_string_view (suffix));
  }

  /** @brief `find(needle) != npos` (C++23 in the standard). */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  contains (lumex_basic_string_view needle) const LUMEX_NOEXCEPT
  {
    return find (needle) != npos;
  }

  /** @brief `find(chr) != npos` (C++23 in the standard). */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  contains (CharT chr) const LUMEX_NOEXCEPT
  {
    return find (chr) != npos;
  }

  /** @brief `find(needle) != npos` for a null-terminated `needle`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR bool
  contains (CharT const *needle) const
  {
    return find (needle) != npos;
  }

  // -- Searches --
  // The index of a match, or npos. A needle or a set that is a null view is an
  // empty one. The standard describes each result by the smallest (largest)
  // position that satisfies a condition; the positions of the "last" searches
  // start at min (pos, size () - 1) (min (pos, size () - n) for rfind).

  /**
   * @brief The first occurrence of `needle` at an index `>= pos`.
   * @details An empty `needle` is found at `pos` when `pos <= size()`. The
   * first character is looked up with `Traits::find` (`memchr`), so a text
   * without it costs one call.
   */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find (lumex_basic_string_view needle, size_type pos = 0) const LUMEX_NOEXCEPT
  {
    return needle.m_size == 0
               ? (pos <= m_size ? pos : npos)
               : ((needle.m_size > m_size || pos > m_size - needle.m_size)
                      ? npos
                      : search_t::find_view (m_data, m_size, needle.m_data,
                                             needle.m_size, pos));
  }

  /** @brief The first occurrence of `chr` at an index `>= pos`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find (CharT chr, size_type pos = 0) const LUMEX_NOEXCEPT
  {
    return pos < m_size
               ? search_t::index_of (
                     m_data, chars_t::find (m_data + pos, m_size - pos, chr))
               : npos;
  }

  /** @brief `find(lumex_basic_string_view(str, count), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find (CharT const *str, size_type pos, size_type count) const LUMEX_NOEXCEPT
  {
    return find (lumex_basic_string_view (str, count), pos);
  }

  /** @brief `find(lumex_basic_string_view(str), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find (CharT const *str, size_type pos = 0) const
  {
    return find (lumex_basic_string_view (str), pos);
  }

  /**
   * @brief The last occurrence of `needle` that starts at an index `<= pos`.
   * @details An empty `needle` is found at `min(pos, size())`.
   */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  rfind (lumex_basic_string_view needle,
         size_type pos = npos) const LUMEX_NOEXCEPT
  {
    return needle.m_size > m_size
               ? npos
               : detail::scan_bwd (
                     typename search_t::match_at{ m_data, needle.m_data,
                                                  needle.m_size },
                     0, detail::min_size (pos, m_size - needle.m_size) + 1);
  }

  /** @brief The last occurrence of `chr` at an index `<= pos`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  rfind (CharT chr, size_type pos = npos) const LUMEX_NOEXCEPT
  {
    return m_size == 0
               ? npos
               : detail::scan_bwd (detail::eq_at<Traits>{ m_data, chr }, 0,
                                   detail::min_size (pos, m_size - 1) + 1);
  }

  /** @brief `rfind(lumex_basic_string_view(str, count), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  rfind (CharT const *str, size_type pos, size_type count) const LUMEX_NOEXCEPT
  {
    return rfind (lumex_basic_string_view (str, count), pos);
  }

  /** @brief `rfind(lumex_basic_string_view(str), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  rfind (CharT const *str, size_type pos = npos) const
  {
    return rfind (lumex_basic_string_view (str), pos);
  }

  /** @brief The first character at an index `>= pos` that is in `set`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_of (lumex_basic_string_view set,
                 size_type pos = 0) const LUMEX_NOEXCEPT
  {
    return pos < m_size ? detail::scan_fwd (
                              typename search_t::in_set_at{ m_data, set.m_data,
                                                            set.m_size },
                              pos, m_size)
                        : npos;
  }

  /** @brief Same as `find(chr, pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_of (CharT chr, size_type pos = 0) const LUMEX_NOEXCEPT
  {
    return find (chr, pos);
  }

  /** @brief `find_first_of(lumex_basic_string_view(str, count), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_of (CharT const *str, size_type pos,
                 size_type count) const LUMEX_NOEXCEPT
  {
    return find_first_of (lumex_basic_string_view (str, count), pos);
  }

  /** @brief `find_first_of(lumex_basic_string_view(str), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_of (CharT const *str, size_type pos = 0) const
  {
    return find_first_of (lumex_basic_string_view (str), pos);
  }

  /** @brief The last character at an index `<= pos` that is in `set`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_of (lumex_basic_string_view set,
                size_type pos = npos) const LUMEX_NOEXCEPT
  {
    return m_size == 0 ? npos
                       : detail::scan_bwd (
                             typename search_t::in_set_at{ m_data, set.m_data,
                                                           set.m_size },
                             0, detail::min_size (pos, m_size - 1) + 1);
  }

  /** @brief Same as `rfind(chr, pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_of (CharT chr, size_type pos = npos) const LUMEX_NOEXCEPT
  {
    return rfind (chr, pos);
  }

  /** @brief `find_last_of(lumex_basic_string_view(str, count), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_of (CharT const *str, size_type pos,
                size_type count) const LUMEX_NOEXCEPT
  {
    return find_last_of (lumex_basic_string_view (str, count), pos);
  }

  /** @brief `find_last_of(lumex_basic_string_view(str), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_of (CharT const *str, size_type pos = npos) const
  {
    return find_last_of (lumex_basic_string_view (str), pos);
  }

  /** @brief The first character at an index `>= pos` that is not in `set`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_not_of (lumex_basic_string_view set,
                     size_type pos = 0) const LUMEX_NOEXCEPT
  {
    return pos < m_size ? detail::scan_fwd (
                              typename search_t::not_in_set_at{
                                  m_data, set.m_data, set.m_size },
                              pos, m_size)
                        : npos;
  }

  /** @brief The first character at an index `>= pos` that is not `chr`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_not_of (CharT chr, size_type pos = 0) const LUMEX_NOEXCEPT
  {
    return pos < m_size
               ? detail::scan_fwd (detail::ne_at<Traits>{ m_data, chr }, pos,
                                   m_size)
               : npos;
  }

  /** @brief `find_first_not_of(lumex_basic_string_view(str, count), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_not_of (CharT const *str, size_type pos,
                     size_type count) const LUMEX_NOEXCEPT
  {
    return find_first_not_of (lumex_basic_string_view (str, count), pos);
  }

  /** @brief `find_first_not_of(lumex_basic_string_view(str), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_first_not_of (CharT const *str, size_type pos = 0) const
  {
    return find_first_not_of (lumex_basic_string_view (str), pos);
  }

  /** @brief The last character at an index `<= pos` that is not in `set`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_not_of (lumex_basic_string_view set,
                    size_type pos = npos) const LUMEX_NOEXCEPT
  {
    return m_size == 0 ? npos
                       : detail::scan_bwd (
                             typename search_t::not_in_set_at{
                                 m_data, set.m_data, set.m_size },
                             0, detail::min_size (pos, m_size - 1) + 1);
  }

  /** @brief The last character at an index `<= pos` that is not `chr`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_not_of (CharT chr, size_type pos = npos) const LUMEX_NOEXCEPT
  {
    return m_size == 0
               ? npos
               : detail::scan_bwd (detail::ne_at<Traits>{ m_data, chr }, 0,
                                   detail::min_size (pos, m_size - 1) + 1);
  }

  /** @brief `find_last_not_of(lumex_basic_string_view(str, count), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_not_of (CharT const *str, size_type pos,
                    size_type count) const LUMEX_NOEXCEPT
  {
    return find_last_not_of (lumex_basic_string_view (str, count), pos);
  }

  /** @brief `find_last_not_of(lumex_basic_string_view(str), pos)`. */
  LUMEX_SV_NODISCARD
  LUMEX_CONSTEXPR size_type
  find_last_not_of (CharT const *str, size_type pos = npos) const
  {
    return find_last_not_of (lumex_basic_string_view (str), pos);
  }

  // -- Conversion to std::basic_string --
  /**
   * @brief Copies the characters into a `std::basic_string` (explicit).
   */
  template <class Allocator = std::allocator<CharT>>
  explicit
  operator std::basic_string<CharT, Traits, Allocator> () const
  {
    return std::basic_string<CharT, Traits, Allocator> (m_data, m_size);
  }

  /**
   * @brief Copies the characters into a `std::basic_string` with `alloc`.
   */
  template <class Allocator = std::allocator<CharT>>
  LUMEX_SV_NODISCARD std::basic_string<CharT, Traits, Allocator>
  to_string (Allocator const &alloc = Allocator ()) const
  {
    return std::basic_string<CharT, Traits, Allocator> (m_data, m_size, alloc);
  }

#if LUMEX_STRING_VIEW_HAS_STD_VIEW
  /**
   * @brief Converts to the standard view (C++17, implicit).
   * @details A template that produces exactly `std::basic_string_view<CharT,
   * Traits>`.
   */
  template <
      class StdView,
      typename std::enable_if<
          std::is_same<StdView, std::basic_string_view<CharT, Traits>>::value,
          int>::type
      = 0>
  LUMEX_CONSTEXPR
  operator StdView () const // NOLINT(google-explicit-constructor)
      LUMEX_NOEXCEPT
  {
    return StdView (m_data, m_size);
  }
#endif

  // -- Comparison operators (found by argument-dependent lookup) --
  /** @brief Equal: the same size and characters. */
  friend LUMEX_CONSTEXPR bool
  operator== (lumex_basic_string_view lhs,
              lumex_basic_string_view rhs) LUMEX_NOEXCEPT
  {
    return lhs.m_size == rhs.m_size && lhs.compare (rhs) == 0;
  }

  /** @brief Not equal. */
  friend LUMEX_CONSTEXPR bool
  operator!= (lumex_basic_string_view lhs,
              lumex_basic_string_view rhs) LUMEX_NOEXCEPT
  {
    return !(lhs == rhs);
  }

  /** @brief Lexicographically less. */
  friend LUMEX_CONSTEXPR bool
  operator< (lumex_basic_string_view lhs,
             lumex_basic_string_view rhs) LUMEX_NOEXCEPT
  {
    return lhs.compare (rhs) < 0;
  }

  /** @brief Lexicographically greater. */
  friend LUMEX_CONSTEXPR bool
  operator> (lumex_basic_string_view lhs,
             lumex_basic_string_view rhs) LUMEX_NOEXCEPT
  {
    return lhs.compare (rhs) > 0;
  }

  /** @brief Lexicographically less or equal. */
  friend LUMEX_CONSTEXPR bool
  operator<= (lumex_basic_string_view lhs,
              lumex_basic_string_view rhs) LUMEX_NOEXCEPT
  {
    return lhs.compare (rhs) <= 0;
  }

  /** @brief Lexicographically greater or equal. */
  friend LUMEX_CONSTEXPR bool
  operator>= (lumex_basic_string_view lhs,
              lumex_basic_string_view rhs) LUMEX_NOEXCEPT
  {
    return lhs.compare (rhs) >= 0;
  }

#if LUMEX_STRING_VIEW_HAS_THREE_WAY
  /**
   * @brief Three-way comparison (C++20): `std::strong_ordering` for
   * `std::char_traits<CharT>`, as the standard view.
   */
  friend LUMEX_CONSTEXPR typename detail::comparison_category<Traits>::type
  operator<=> (lumex_basic_string_view lhs,
               lumex_basic_string_view rhs) LUMEX_NOEXCEPT
  {
    return static_cast<typename detail::comparison_category<Traits>::type> (
        lhs.compare (rhs) <=> 0);
  }
#endif

private:
  const_pointer m_data;
  size_type m_size;
};

#if __cplusplus < 201703L
// Before C++17 the in-class initialization of a static constexpr member is a
// declaration; an odr-use (a reference parameter) needs this definition.
template <class CharT, class Traits>
LUMEX_CONSTEXPR
    typename lumex_basic_string_view<CharT, Traits>::size_type const
        lumex_basic_string_view<CharT, Traits>::npos;
#endif

#if LUMEX_STRING_VIEW_HAS_STD_VIEW
// -- Comparison with the standard view (C++17) --
// The two views convert to each other, so a comparison of a view with a
// `std::basic_string_view` would be ambiguous between the operators of the
// class and those of the standard library. These templates accept exactly
// `std::basic_string_view<CharT, Traits>` (a C string or a `std::basic_string`
// still goes to the operators of the class) and win the overload resolution.
#define LUMEX_SV_MIXED_OPERATOR(op)                                           \
  template <class CharT, class Traits>                                        \
  LUMEX_CONSTEXPR bool operator op (                                          \
      lumex_basic_string_view<CharT, Traits> lhs,                             \
      std::basic_string_view<CharT, Traits> rhs) LUMEX_NOEXCEPT               \
  {                                                                           \
    return lhs op lumex_basic_string_view<CharT, Traits> (rhs);               \
  }                                                                           \
  template <class CharT, class Traits>                                        \
  LUMEX_CONSTEXPR bool operator op (                                          \
      std::basic_string_view<CharT, Traits> lhs,                              \
      lumex_basic_string_view<CharT, Traits> rhs) LUMEX_NOEXCEPT              \
  {                                                                           \
    return lumex_basic_string_view<CharT, Traits> (lhs) op rhs;               \
  }

/** @brief `==` of a view and a `std::basic_string_view`, in both orders. */
LUMEX_SV_MIXED_OPERATOR (==)
/** @brief `!=` of a view and a `std::basic_string_view`, in both orders. */
LUMEX_SV_MIXED_OPERATOR (!=)
/** @brief `<` of a view and a `std::basic_string_view`, in both orders. */
LUMEX_SV_MIXED_OPERATOR (<)
/** @brief `>` of a view and a `std::basic_string_view`, in both orders. */
LUMEX_SV_MIXED_OPERATOR (>)
/** @brief `<=` of a view and a `std::basic_string_view`, in both orders. */
LUMEX_SV_MIXED_OPERATOR (<=)
/** @brief `>=` of a view and a `std::basic_string_view`, in both orders. */
LUMEX_SV_MIXED_OPERATOR (>=)
#undef LUMEX_SV_MIXED_OPERATOR

#if LUMEX_STRING_VIEW_HAS_THREE_WAY
/** @brief `<=>` of a view and a `std::basic_string_view`. */
template <class CharT, class Traits>
LUMEX_CONSTEXPR typename detail::comparison_category<Traits>::type
operator<=> (lumex_basic_string_view<CharT, Traits> lhs,
             std::basic_string_view<CharT, Traits> rhs) LUMEX_NOEXCEPT
{
  return lhs <=> lumex_basic_string_view<CharT, Traits> (rhs);
}

/** @brief `<=>` of a `std::basic_string_view` and a view. */
template <class CharT, class Traits>
LUMEX_CONSTEXPR typename detail::comparison_category<Traits>::type
operator<=> (std::basic_string_view<CharT, Traits> lhs,
             lumex_basic_string_view<CharT, Traits> rhs) LUMEX_NOEXCEPT
{
  return lumex_basic_string_view<CharT, Traits> (lhs) <=> rhs;
}
#endif
#endif

// -- Stream inserter --
/**
 * @brief Writes the view to a stream as the standard one does.
 * @details A formatted output function: the field is `width()` wide (the view
 * is padded with `fill()` on the left, or on the right when `adjustfield` is
 * `left`), `width` is reset to `0` after the write, a stream that does not
 * pass the sentry is left untouched, a failing stream buffer sets `badbit`
 * and an exception of the buffer is handled as the standard says.
 */
template <class CharT, class Traits>
LUMEX_SV_NO_UBSAN std::basic_ostream<CharT, Traits> &
operator<< (std::basic_ostream<CharT, Traits> &ostr,
            lumex_basic_string_view<CharT, Traits> sview)
{
  typename std::basic_ostream<CharT, Traits>::sentry const guard (ostr);
  if (guard)
    {
      try
        {
          std::streamsize const size
              = static_cast<std::streamsize> (sview.size ());
          std::streamsize const width = ostr.width ();
          std::streamsize const pad = width > size ? width - size : 0;
          bool const left = (ostr.flags () & std::ios_base::adjustfield)
                            == std::ios_base::left;
          std::basic_streambuf<CharT, Traits> *const buf = ostr.rdbuf ();
          bool good = true;
          if (!left)
            good = detail::put_fill (buf, ostr.fill (), pad);
          if (good && size > 0)
            good = buf->sputn (sview.data (), size) == size;
          if (good && left)
            good = detail::put_fill (buf, ostr.fill (), pad);
          ostr.width (0);
          if (!good)
            ostr.setstate (std::ios_base::badbit);
        }
#if defined(__GLIBCXX__)
      catch (abi::__forced_unwind &)
        {
          // The unwind of a cancelled thread must go on, as libstdc++'s own
          // inserters do: set the badbit (without throwing) and rethrow.
          try
            {
              ostr.setstate (std::ios_base::badbit);
            }
          catch (...)
            {
            }
          throw;
        }
#endif
      catch (...)
        {
          bool const rethrow
              = (ostr.exceptions () & std::ios_base::badbit) != 0;
          try
            {
              ostr.setstate (std::ios_base::badbit);
            }
          catch (...)
            {
              // setstate throws std::ios_base::failure here; the original
              // exception is the one to rethrow.
            }
          if (rethrow)
            throw;
        }
    }
  return ostr;
}

// -- Aliases --
/** @brief The view of `char`: the `std::string_view` of the library. */
typedef lumex_basic_string_view<char> lumex_string_view;
/** @brief The view of `wchar_t`: the `std::wstring_view` of the library. */
typedef lumex_basic_string_view<wchar_t> lumex_wstring_view;
/** @brief The view of `char16_t`: the `std::u16string_view` of the library. */
typedef lumex_basic_string_view<char16_t> lumex_u16string_view;
/** @brief The view of `char32_t`: the `std::u32string_view` of the library. */
typedef lumex_basic_string_view<char32_t> lumex_u32string_view;
#if LUMEX_HAS_CHAR8_T
/** @brief The view of `char8_t`: the `std::u8string_view` of the library. */
typedef lumex_basic_string_view<char8_t> lumex_u8string_view;
#endif

/**
 * @brief The `_sv` literals: `"text"_sv` is a `lumex_string_view`,
 * `L"text"_sv` a `lumex_wstring_view`, and so on.
 * @details Like `std::literals::string_view_literals`, with the same suffix
 * and the same lengths (embedded `'\0'` included). The namespace is not
 * inline: `using namespace lumex::core::string_view::view::literals;` brings
 * the suffix in, and it does not clash with `std::literals` when both are
 * used.
 */
namespace literals
{
/** @brief `"text"_sv`: a `lumex_string_view` of `len` characters. */
LUMEX_CONSTEXPR inline lumex_string_view
operator""_sv (char const *str, std::size_t len) LUMEX_NOEXCEPT
{
  return lumex_string_view (str, len);
}

/** @brief `L"text"_sv`: a `lumex_wstring_view` of `len` characters. */
LUMEX_CONSTEXPR inline lumex_wstring_view
operator""_sv (wchar_t const *str, std::size_t len) LUMEX_NOEXCEPT
{
  return lumex_wstring_view (str, len);
}

/** @brief `u"text"_sv`: a `lumex_u16string_view` of `len` characters. */
LUMEX_CONSTEXPR inline lumex_u16string_view
operator""_sv (char16_t const *str, std::size_t len) LUMEX_NOEXCEPT
{
  return lumex_u16string_view (str, len);
}

/** @brief `U"text"_sv`: a `lumex_u32string_view` of `len` characters. */
LUMEX_CONSTEXPR inline lumex_u32string_view
operator""_sv (char32_t const *str, std::size_t len) LUMEX_NOEXCEPT
{
  return lumex_u32string_view (str, len);
}

#if LUMEX_HAS_CHAR8_T
/** @brief `u8"text"_sv`: a `lumex_u8string_view` of `len` characters. */
LUMEX_CONSTEXPR inline lumex_u8string_view
operator""_sv (char8_t const *str, std::size_t len) LUMEX_NOEXCEPT
{
  return lumex_u8string_view (str, len);
}
#endif
} // namespace literals
} // namespace view
} // namespace string_view
} // namespace core
} // namespace lumex

namespace std
{
/**
 * @brief `std::hash` of a view: `detail::hash_chars`, one value on every
 * standard, compiler and standard library (not the value of
 * `std::hash<std::basic_string_view>`, which depends on the standard library).
 */
template <class CharT, class Traits>
struct hash<
    ::lumex::core::string_view::view::lumex_basic_string_view<CharT, Traits>>
{
#if __cplusplus < 201703L
  /** @brief The type that is hashed. */
  typedef ::lumex::core::string_view::view::lumex_basic_string_view<CharT,
                                                                    Traits>
      argument_type;
  /** @brief The type of the result. */
  typedef std::size_t result_type;
#endif

  /** @brief The hash of the characters of `sview`. */
  std::size_t
  operator() (::lumex::core::string_view::view::lumex_basic_string_view<
              CharT, Traits> const &sview) const LUMEX_NOEXCEPT
  {
    return ::lumex::core::string_view::view::detail::hash_chars (
        sview.data (), sview.size ());
  }
};

#if LUMEX_STRING_VIEW_HAS_RANGES
namespace ranges
{
/** @brief A view is a borrowed range: its iterators outlive it. */
template <class CharT, class Traits>
LUMEX_INLINE_VARIABLE LUMEX_CONSTEXPR bool enable_borrowed_range<
    ::lumex::core::string_view::view::lumex_basic_string_view<CharT, Traits>>
    = true;

/** @brief A view is a view: copying it is cheap and copies no characters. */
template <class CharT, class Traits>
LUMEX_INLINE_VARIABLE LUMEX_CONSTEXPR bool enable_view<
    ::lumex::core::string_view::view::lumex_basic_string_view<CharT, Traits>>
    = true;
} // namespace ranges
#endif
} // namespace std

#undef LUMEX_SV_NODISCARD
#undef LUMEX_SV_NO_UBSAN
#undef LUMEX_SV_CONSTEXPR_CXX17
#undef LUMEX_SV_CONSTEXPR_TRAITS
#undef LUMEX_SV_CONSTANT_P

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // LUMEX_CORE_STRING_VIEW_VIEW_BASIC_STRING_VIEW_HPP
