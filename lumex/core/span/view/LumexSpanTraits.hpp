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
 * @file LumexSpanTraits.hpp
 * @brief Constants, the byte type and the traits behind `span`.
 * @details Holds everything that `span` and the free functions around it
 * share: `dynamic_extent`, `byte` (an own scoped enumeration with the
 * operators of `std::byte`, in every standard), the customization trait
 * `is_contiguous_iterator` and the detection traits of the C++11 stand-ins for
 * the concepts the C++20 `std::span` constrains its constructors with
 * (`contiguous_range`, `sized_range`, `borrowed_range`, `contiguous_iterator`,
 * `sized_sentinel_for`). The header also defines the configuration macros
 * `LUMEX_SPAN_HAS_CONCEPTS` and `LUMEX_SPAN_HAS_RANGES`. Nothing here
 * allocates or throws.
 */
#ifndef LUMEX_CORE_SPAN_VIEW_SPAN_TRAITS_HPP
#define LUMEX_CORE_SPAN_VIEW_SPAN_TRAITS_HPP

#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>
#if __cplusplus >= 202002L
#include <memory> // std::to_address
#endif

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @def LUMEX_SPAN_HAS_CONCEPTS
 * @brief 1 when the C++20 iterator concepts (`std::contiguous_iterator`,
 * `std::sized_sentinel_for`) are available, 0 otherwise.
 * @details Tested on the library feature (`__cpp_lib_concepts`) together with
 * the language one, not on `__cplusplus` alone: GCC 8 accepts `-std=c++2a`
 * without concepts.
 */
#if __cplusplus >= 202002L && LUMEX_HAS_CONCEPTS && LUMEX_HAS_STD_CONCEPTS
#define LUMEX_SPAN_HAS_CONCEPTS 1
#else
#define LUMEX_SPAN_HAS_CONCEPTS 0
#endif

/**
 * @def LUMEX_SPAN_HAS_RANGES
 * @brief 1 when `std::ranges::enable_borrowed_range` and
 * `std::ranges::enable_view` exist, 0 otherwise.
 * @details When it is 1, `span` opts into both and accepts the rvalues of
 * every standard borrowed range (for example `std::span`) in its range
 * constructor.
 */
#if LUMEX_SPAN_HAS_CONCEPTS && LUMEX_HAS_STD_RANGES && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#define LUMEX_SPAN_HAS_RANGES 1
#endif
#endif
#ifndef LUMEX_SPAN_HAS_RANGES
#define LUMEX_SPAN_HAS_RANGES 0
#endif

/**
 * @def LUMEX_SPAN_HAS_STD_SPAN
 * @brief 1 when `std::span` and `std::byte` both exist (C++20 with `<span>`),
 * 0 otherwise.
 * @details When it is 1, `span` converts to and from a `std::span` of the
 * other byte type (`byte` and `std::byte`).
 */
#if __cplusplus >= 202002L && LUMEX_HAS_STD_SPAN && LUMEX_HAS_STD_BYTE        \
    && defined(__has_include)
#if __has_include(<span>)
#include <span>
#define LUMEX_SPAN_HAS_STD_SPAN 1
#endif
#endif
#ifndef LUMEX_SPAN_HAS_STD_SPAN
#define LUMEX_SPAN_HAS_STD_SPAN 0
#endif

/**
 * @def LUMEX_SPAN_MAY_ALIAS
 * @brief Marks the own `byte` as a type that may alias any object, as the
 * standard allows `std::byte` to.
 * @details GCC and Clang do not treat a scoped enumeration with an
 * `unsigned char` base as a character type, so without the attribute a write
 * through a `span<byte>` is not seen by a read of the viewed object.
 */
#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_SPAN_MAY_ALIAS __attribute__ ((__may_alias__))
#else
#define LUMEX_SPAN_MAY_ALIAS
#endif

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

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
/**
 * @brief The `span` module: a non-owning view of a contiguous sequence.
 */
namespace span
{
namespace view
{
/**
 * @brief The extent of a `span` whose number of elements is known at run time.
 * @details Equals `std::dynamic_extent`: the largest `std::size_t`.
 */
LUMEX_INLINE_VARIABLE LUMEX_CONSTEXPR std::size_t dynamic_extent
    = static_cast<std::size_t> (-1);

/**
 * @brief The element type of the byte views `as_bytes` and
 * `as_writable_bytes` return: an own scoped enumeration with the underlying
 * type `unsigned char`, the operators and `to_integer` of `std::byte`.
 * @details It is the same type in every standard and never an alias of
 * `std::byte` (C++17). An enumeration cannot have conversion functions, so
 * the two bytes convert explicitly: `static_cast<std::byte> (value)` and
 * `static_cast<byte> (stdValue)` (both enumerations have the base
 * `unsigned char`). The views convert implicitly: a `span` of `byte` and a
 * `span` of `std::byte` convert to each other, and to and from a `std::span`
 * of the other byte type (C++20), see the constructors and the conversion
 * function of `span`.
 */
enum class LUMEX_SPAN_MAY_ALIAS byte : unsigned char
{
};

/**
 * @brief Converts a `byte` to an integer type.
 * @tparam IntegerType An integral type.
 * @param value The byte.
 * @return The value of the byte as `IntegerType`.
 */
template <typename IntegerType>
LUMEX_CONSTEXPR typename std::enable_if<std::is_integral<IntegerType>::value,
                                        IntegerType>::type
to_integer (byte value) LUMEX_NOEXCEPT
{
  return static_cast<IntegerType> (value);
}

/**
 * @brief Shifts a `byte` to the left.
 * @param value The byte.
 * @param shift The number of bit positions.
 * @return The shifted byte.
 */
template <typename IntegerType>
LUMEX_CONSTEXPR
    typename std::enable_if<std::is_integral<IntegerType>::value, byte>::type
    operator<< (byte value, IntegerType shift) LUMEX_NOEXCEPT
{
  return static_cast<byte> (
      static_cast<unsigned char> (static_cast<unsigned int> (value) << shift));
}

/**
 * @brief Shifts a `byte` to the right.
 * @param value The byte.
 * @param shift The number of bit positions.
 * @return The shifted byte.
 */
template <typename IntegerType>
LUMEX_CONSTEXPR
    typename std::enable_if<std::is_integral<IntegerType>::value, byte>::type
    operator>> (byte value, IntegerType shift) LUMEX_NOEXCEPT
{
  return static_cast<byte> (
      static_cast<unsigned char> (static_cast<unsigned int> (value) >> shift));
}

/**
 * @brief Bitwise or of two bytes.
 */
LUMEX_CONSTEXPR inline byte
operator| (byte lhs, byte rhs) LUMEX_NOEXCEPT
{
  return static_cast<byte> (static_cast<unsigned char> (
      static_cast<unsigned int> (lhs) | static_cast<unsigned int> (rhs)));
}

/**
 * @brief Bitwise and of two bytes.
 */
LUMEX_CONSTEXPR inline byte
operator& (byte lhs, byte rhs) LUMEX_NOEXCEPT
{
  return static_cast<byte> (static_cast<unsigned char> (
      static_cast<unsigned int> (lhs) & static_cast<unsigned int> (rhs)));
}

/**
 * @brief Bitwise exclusive or of two bytes.
 */
LUMEX_CONSTEXPR inline byte
operator^ (byte lhs, byte rhs) LUMEX_NOEXCEPT
{
  return static_cast<byte> (static_cast<unsigned char> (
      static_cast<unsigned int> (lhs) ^ static_cast<unsigned int> (rhs)));
}

/**
 * @brief Bitwise complement of a byte.
 */
LUMEX_CONSTEXPR inline byte
operator~(byte value) LUMEX_NOEXCEPT
{
  return static_cast<byte> (
      static_cast<unsigned char> (~static_cast<unsigned int> (value)));
}

/**
 * @brief Shifts a `byte` to the left in place.
 */
template <typename IntegerType>
LUMEX_CONSTEXPR_CXX14
    typename std::enable_if<std::is_integral<IntegerType>::value, byte &>::type
    operator<<= (byte &value, IntegerType shift) LUMEX_NOEXCEPT
{
  return value = value << shift;
}

/**
 * @brief Shifts a `byte` to the right in place.
 */
template <typename IntegerType>
LUMEX_CONSTEXPR_CXX14
    typename std::enable_if<std::is_integral<IntegerType>::value, byte &>::type
    operator>>= (byte &value, IntegerType shift) LUMEX_NOEXCEPT
{
  return value = value >> shift;
}

/**
 * @brief Bitwise or of two bytes in place.
 */
LUMEX_CONSTEXPR_CXX14 inline byte &
operator|= (byte &lhs, byte rhs) LUMEX_NOEXCEPT
{
  return lhs = lhs | rhs;
}

/**
 * @brief Bitwise and of two bytes in place.
 */
LUMEX_CONSTEXPR_CXX14 inline byte &
operator&= (byte &lhs, byte rhs) LUMEX_NOEXCEPT
{
  return lhs = lhs & rhs;
}

/**
 * @brief Bitwise exclusive or of two bytes in place.
 */
LUMEX_CONSTEXPR_CXX14 inline byte &
operator^= (byte &lhs, byte rhs) LUMEX_NOEXCEPT
{
  return lhs = lhs ^ rhs;
}

template <typename ElementType, std::size_t Extent = dynamic_extent>
class span;

/**
 * @brief Tells whether a type is an iterator over contiguous storage.
 * @details The constructors of `span` that take an iterator accept a type
 * only when this trait is true for it. Raw pointers always qualify. From C++20
 * the answer is `std::contiguous_iterator`. Before C++20 the language cannot
 * tell a `std::vector` iterator from a `std::deque` one, so any other iterator
 * type has to be named by a specialization:
 * @code
 * template <> struct lumex::core::span::view::is_contiguous_iterator<my_it>
 *     : std::true_type {};
 * @endcode
 * The specialization is a promise that `*it` and `*(it + n)` are `n` elements
 * apart in memory; breaking it is undefined behavior.
 * @tparam It The iterator type.
 */
#if LUMEX_SPAN_HAS_CONCEPTS
template <typename It>
struct is_contiguous_iterator
    : std::integral_constant<bool, std::contiguous_iterator<It>>
{
};
#else
template <typename It> struct is_contiguous_iterator : std::false_type
{
};
#endif

/**
 * @brief Pointers to objects are contiguous iterators; pointers to `void` and
 * to functions are not.
 */
template <typename T> struct is_contiguous_iterator<T *> : std::is_object<T>
{
};

/**
 * @brief Helpers of the module. Not part of the public interface.
 */
namespace detail
{
/**
 * @brief The type with its reference and top-level cv qualifiers removed.
 */
template <typename T> struct remove_cvref
{
  using type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

/**
 * @brief True when `From (*)[]` converts to `To (*)[]`.
 * @details The test the standard uses to allow a qualification conversion of
 * the element type and nothing else (`int` to `const int`, never `Derived` to
 * `Base`).
 */
template <typename From, typename To, typename = void>
struct is_array_convertible : std::false_type
{
};

template <typename From, typename To>
struct is_array_convertible<From, To,
                            typename std::enable_if<std::is_convertible<
                                From (*)[], To (*)[]>::value>::type>
    : std::true_type
{
};

#if LUMEX_HAS_STD_BYTE
/**
 * @brief `Type` with the `const` and `volatile` qualifiers of `Like`.
 */
template <typename Like, typename Type> struct copy_cv
{
  using const_applied =
      typename std::conditional<std::is_const<Like>::value,
                                typename std::add_const<Type>::type,
                                Type>::type;
  using type = typename std::conditional<
      std::is_volatile<Like>::value,
      typename std::add_volatile<const_applied>::type, const_applied>::type;
};

/**
 * @brief The other byte type, with the qualifiers of `T`: `std::byte` for
 * `byte` and `byte` for `std::byte`. No `type` for any other `T`.
 */
template <typename T, typename Plain = typename std::remove_cv<T>::type>
struct byte_twin
{
};

template <typename T> struct byte_twin<T, byte>
{
  using type = typename copy_cv<T, std::byte>::type;
};

template <typename T> struct byte_twin<T, std::byte>
{
  using type = typename copy_cv<T, byte>::type;
};

/**
 * @brief True when `From` and `To` are the two byte types (`byte` and
 * `std::byte`, in either order) and a pointer to `From` becomes a pointer to
 * `To` by the exchange of the byte type plus a qualification conversion
 * (`std::byte const` to `byte const` or to `byte const volatile`, never to
 * `byte`).
 */
template <typename From, typename To, typename = void>
struct is_byte_twin_convertible : std::false_type
{
};

template <typename From, typename To>
struct is_byte_twin_convertible<
    From, To,
    typename std::enable_if<
        is_array_convertible<typename byte_twin<From>::type, To>::value>::type>
    : std::true_type
{
};
#endif

#if LUMEX_SPAN_HAS_STD_SPAN
/**
 * @brief True when `Target` is a `std::span<U, N>` that a `span<Element,
 * Extent>` of byte elements converts to: `U` is the other byte type
 * (`is_byte_twin_convertible`) and `N` is dynamic or equal to `Extent`.
 */
template <typename Target, typename Element, std::size_t Extent>
struct is_std_byte_span_target : std::false_type
{
};

template <typename U, std::size_t N, typename Element, std::size_t Extent>
struct is_std_byte_span_target<std::span<U, N>, Element, Extent>
    : std::integral_constant<bool, is_byte_twin_convertible<Element, U>::value
                                       && (N == dynamic_extent || N == Extent)>
{
};
#endif

/**
 * @brief True for every `span<T, Extent>`.
 */
template <typename T> struct is_span : std::false_type
{
};

template <typename T, std::size_t Extent>
struct is_span<span<T, Extent>> : std::true_type
{
};

/**
 * @brief True for every `std::array<T, N>`.
 */
template <typename T> struct is_std_array : std::false_type
{
};

template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type
{
};

/**
 * @brief True when a view of `Extent` elements may be made from `Other`
 * elements: at least one of the two is dynamic, or they are equal.
 */
template <std::size_t Extent, std::size_t Other>
struct is_extent_compatible
    : std::integral_constant<bool, Extent == dynamic_extent
                                       || Other == dynamic_extent
                                       || Extent == Other>
{
};

/**
 * @brief The pointer and the element count of a `span`; the count is stored
 * only when it is not part of the type.
 */
template <typename T, std::size_t Extent> class span_storage
{
public:
  LUMEX_CONSTEXPR_CTOR
  span_storage (T *data, std::size_t /*size*/) LUMEX_NOEXCEPT : m_data (data)
  {
  }

  LUMEX_CONSTEXPR std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return Extent;
  }

  LUMEX_CONSTEXPR T *
  data () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

private:
  T *m_data;
};

template <typename T> class span_storage<T, dynamic_extent>
{
public:
  LUMEX_CONSTEXPR_CTOR
  span_storage (T *data, std::size_t size) LUMEX_NOEXCEPT : m_data (data),
                                                            m_size (size)
  {
  }

  LUMEX_CONSTEXPR std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return m_size;
  }

  LUMEX_CONSTEXPR T *
  data () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

private:
  T *m_data;
  std::size_t m_size;
};

/**
 * @brief The extent of `span::subspan<Offset, Count>()`.
 */
template <std::size_t Extent, std::size_t Offset, std::size_t Count>
struct subspan_extent
    : std::integral_constant<
          std::size_t, (Count != dynamic_extent
                            ? Count
                            : (Extent != dynamic_extent
                                   ? (Offset <= Extent ? Extent - Offset : 0)
                                   : dynamic_extent))>
{
};

/**
 * @brief The extent of the byte view of a `span<T, Extent>`.
 */
template <typename T, std::size_t Extent>
struct bytes_extent
    : std::integral_constant<std::size_t,
                             (Extent == dynamic_extent ? dynamic_extent
                                                       : sizeof (T) * Extent)>
{
};

/**
 * @brief Throws `std::out_of_range`; keeps the throw out of the inline
 * accessors.
 */
LUMEX_ATTRIBUTE_NORETURN inline void
throw_out_of_range (char const *what)
{
  throw std::out_of_range (what);
}

/**
 * @brief The pointer of a contiguous range: the member `data ()`.
 */
template <typename Range>
LUMEX_CONSTEXPR auto
range_data (Range &range) LUMEX_NOEXCEPT_IF (noexcept (range.data ()))
    -> decltype (range.data ())
{
  return range.data ();
}

/**
 * @brief The pointer of a `std::initializer_list`, which has no `data ()`.
 */
template <typename Element>
LUMEX_CONSTEXPR Element const *
range_data (std::initializer_list<Element> list) LUMEX_NOEXCEPT
{
  return list.begin ();
}

/**
 * @brief The element type of a range with a pointer-returning `data ()`.
 * @details Has no `type` member when `Range` is not such a range. `Range` may
 * be a reference type; the lookup is done on an lvalue of it.
 */
template <typename Range, typename = void> struct range_element
{
};

template <typename Range>
struct range_element<
    Range, typename std::enable_if<std::is_pointer<decltype (range_data (
               std::declval<Range &> ()))>::value>::type>
{
  using type = typename std::remove_pointer<decltype (range_data (
      std::declval<Range &> ()))>::type;
};

/**
 * @brief True when the range has a `size ()` convertible to `std::size_t`.
 */
template <typename Range, typename = void>
struct range_has_size : std::false_type
{
};

template <typename Range>
struct range_has_size<Range, typename std::enable_if<std::is_convertible<
                                 decltype (std::declval<Range &> ().size ()),
                                 std::size_t>::value>::type> : std::true_type
{
};

/**
 * @brief True when a range passed as `Range &&` outlives its view: an lvalue,
 * or a type that opted into `std::ranges::enable_borrowed_range`.
 */
template <typename Range>
struct is_borrowed_range
    : std::integral_constant<bool, std::is_lvalue_reference<Range>::value
#if LUMEX_SPAN_HAS_RANGES
                                       || std::ranges::enable_borrowed_range<
                                           typename remove_cvref<Range>::type>
#endif
                             >
{
};

/**
 * @brief The number of elements a range type declares in its type through a
 * static member `extent` (`std::span`, `boost::span`), `dynamic_extent` for a
 * range that does not.
 */
template <typename Range, typename = void>
struct range_declared_extent
    : std::integral_constant<std::size_t, dynamic_extent>
{
};

template <typename Range>
struct range_declared_extent<Range,
                             typename std::enable_if<std::is_convertible<
                                 decltype (remove_cvref<Range>::type::extent),
                                 std::size_t>::value>::type>
    : std::integral_constant<std::size_t, remove_cvref<Range>::type::extent>
{
};

/**
 * @brief True when `Range` may initialize a `span<T, Extent>`: the stand-in
 * for `contiguous_range && sized_range && (borrowed_range || is_const_v<T>)`
 * with an element type that converts to `T`, excluding arrays, `std::array`
 * and `span`, which have constructors of their own. A range that declares an
 * extent in its type (another library's span) must declare one compatible with
 * `Extent`, as a `span` source must.
 */
template <typename Range, typename T, std::size_t Extent, typename = void>
struct is_compatible_range : std::false_type
{
};

template <typename Range, typename T, std::size_t Extent>
struct is_compatible_range<
    Range, T, Extent,
    typename std::enable_if<
        !is_span<typename remove_cvref<Range>::type>::value
        && !is_std_array<typename remove_cvref<Range>::type>::value
        && !std::is_array<typename remove_cvref<Range>::type>::value
        && (std::is_const<T>::value || is_borrowed_range<Range>::value)
        && is_extent_compatible<Extent,
                                range_declared_extent<Range>::value>::value
        && range_has_size<Range>::value
        && is_array_convertible<typename range_element<Range>::type,
                                T>::value>::type> : std::true_type
{
};

/**
 * @brief True when `Iterator` is a contiguous iterator whose elements convert
 * to `T`.
 */
template <typename Iterator, typename T, typename = void>
struct is_compatible_iterator : std::false_type
{
};

template <typename Iterator, typename T>
struct is_compatible_iterator<
    Iterator, T,
    typename std::enable_if<
        is_contiguous_iterator<Iterator>::value
        && is_array_convertible<
            typename std::remove_reference<
                decltype (*std::declval<Iterator &> ())>::type,
            T>::value>::type> : std::true_type
{
};

/**
 * @brief True when `Sentinel` ends a range of `Iterator` in a known number of
 * steps and is not a count: the stand-in for `std::sized_sentinel_for`.
 */
#if LUMEX_SPAN_HAS_CONCEPTS
template <typename Sentinel, typename Iterator>
struct is_sized_sentinel
    : std::integral_constant<
          bool, std::sized_sentinel_for<Sentinel, Iterator>
                    && !std::is_convertible<Sentinel, std::size_t>::value>
{
};
#else
template <typename Sentinel, typename Iterator, typename = void>
struct is_sized_sentinel : std::false_type
{
};

template <typename Sentinel, typename Iterator>
struct is_sized_sentinel<
    Sentinel, Iterator,
    typename std::enable_if<
        !std::is_convertible<Sentinel, std::size_t>::value
        && std::is_convertible<decltype (std::declval<Sentinel &> ()
                                         - std::declval<Iterator &> ()),
                               std::ptrdiff_t>::value>::type> : std::true_type
{
};
#endif

/**
 * @brief The address of an object, ignoring an overloaded `operator&`: what
 * `std::addressof` does, without including `<memory>` (which costs a
 * translation unit as much as the whole module).
 */
template <typename T>
T *
address_of (T &object) LUMEX_NOEXCEPT
{
  return reinterpret_cast<T *> ( // NOLINT
      &const_cast<char &> (
          reinterpret_cast<char const volatile &> (object))); // NOLINT
}

/**
 * @brief The address of the first element of a contiguous range given by a
 * pointer; the count is not needed.
 */
template <typename T>
LUMEX_CONSTEXPR T *
iterator_address (T *iterator, std::size_t /*count*/) LUMEX_NOEXCEPT
{
  return iterator;
}

/**
 * @brief The address of the first element of a contiguous range given by an
 * iterator class.
 * @details From C++20 this is `std::to_address`. Before, an empty range yields
 * `nullptr` because the iterator may not be dereferenced.
 */
template <typename Iterator>
LUMEX_CONSTEXPR typename std::enable_if<
    !std::is_pointer<Iterator>::value,
    typename std::add_pointer<typename std::remove_reference<
        decltype (*std::declval<Iterator &> ())>::type>::type>::type
iterator_address (Iterator iterator, std::size_t count) LUMEX_NOEXCEPT
{
#if LUMEX_SPAN_HAS_CONCEPTS && LUMEX_HAS_STD_TO_ADDRESS
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count);
  return std::to_address (iterator);
#else
  return count == 0 ? nullptr : address_of (*iterator);
#endif
}
} // namespace detail
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_SPAN_VIEW_SPAN_TRAITS_HPP
