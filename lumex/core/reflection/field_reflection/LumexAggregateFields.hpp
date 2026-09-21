/**
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
 * @file LumexAggregateFields.hpp
 * @brief Non-intrusive field count, indexed access, and (from C++20) field
 *        names for simple aggregates.
 * @details Replaces the three Boost.PFR calls FieldReflection used
 *          (`tuple_size`, `get`, `names_as_array`). This header is original
 *          Lumex code (MIT), not a relicensed Boost dump.
 *
 *          Field count is C++11: aggregate initialization against a
 *          converting placeholder, binary-searched up to 32 members.
 *          Indexed `get` is C++14: friend-injected field types (ADL
 *          `friend auto` on the tag, Boost.PFR shape) plus sequential
 *          layout offsets. Structured bindings are not used for access.
 *          C++11 has count only: a friend return type cannot be
 *          deduced without `auto`.
 *
 *          Field names need C++20. `LUMEX_FUNCTION_NAME` embeds an
 *          identifier only when that identifier is a template argument.
 *          C++11 pointer NTTPs accept `&Class::member` (the name is
 *          already known) or the address of a complete object, not a
 *          pointer to the I-th member of a static instance. C++20
 *          `template<auto>` plus a structured binding into a declared-
 *          but-undefined phantom aggregate (`fake_object`) is what puts
 *          the real member name into the pretty string. A defined
 *          `inline T` instance fails on MSVC: a local reference into it
 *          is not a constant expression for the NTTP.
 *
 *          Limit: 32 public data members. No base classes, no
 *          bit-fields, no reference members. A computed-size check
 *          rejects types whose layout does not match sequential
 *          aligned members.
 */

#ifndef LUMEX_CORE_REFLECTION_FIELD_REFLECTION_AGGREGATE_FIELDS_HPP
#define LUMEX_CORE_REFLECTION_FIELD_REFLECTION_AGGREGATE_FIELDS_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"

#if __cplusplus >= 202002L
#include <tuple>
#endif

namespace lumex
{
namespace core
{
namespace reflection
{
namespace field_reflection
{
namespace detail
{
LUMEX_CONSTEXPR std::size_t k_max_aggregate_fields = 32;

template <std::size_t... I> struct index_sequence
{
};

template <std::size_t N, std::size_t... I>
struct make_index_sequence_impl : make_index_sequence_impl<N - 1, N - 1, I...>
{
};

template <std::size_t... I> struct make_index_sequence_impl<0, I...>
{
  typedef index_sequence<I...> type;
};

template <std::size_t N>
struct make_index_sequence : make_index_sequence_impl<N>
{
};

template <std::size_t I> struct any_field
{
  template <typename Type> operator Type () const;
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
template <typename Aggregate, std::size_t... I>
auto can_construct_fn (index_sequence<I...>)
    -> decltype (Aggregate{ any_field<I>{}... }, std::true_type{});
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

template <typename Aggregate> std::false_type can_construct_fn (...);

template <typename Aggregate, std::size_t N> struct can_construct_n
{
  typedef decltype (can_construct_fn<Aggregate> (
      typename make_index_sequence<N>::type{})) type;
  static const bool value = type::value;
};

template <typename Aggregate, std::size_t Lo, std::size_t Hi,
          bool Done = (Lo == Hi)>
struct count_fields_impl;

template <typename Aggregate, std::size_t Lo, std::size_t Hi>
struct count_fields_impl<Aggregate, Lo, Hi, true>
{
  static const std::size_t value = Lo;
};

template <typename Aggregate, std::size_t Lo, std::size_t Hi>
struct count_fields_impl<Aggregate, Lo, Hi, false>
{
  static const std::size_t mid = Lo + (Hi - Lo + 1) / 2;
  static const bool ok = can_construct_n<Aggregate, mid>::value;
  static const std::size_t value = count_fields_impl < Aggregate,
                           ok ? mid : Lo, ok ? Hi : mid - 1 > ::value;
};

#if __cplusplus >= 201402L
template <typename T, std::size_t N> struct loophole_tag
{
  friend auto loophole_fn (loophole_tag<T, N>);
};

template <typename T, typename U, std::size_t N> struct loophole_set
{
  friend auto
  loophole_fn (loophole_tag<T, N>)
  {
    return static_cast<U *> (nullptr);
  }
};

template <typename T, std::size_t N> struct loophole_ubiq
{
  template <typename U, std::size_t = sizeof (loophole_set<T, U, N>)>
  operator U ();
};

template <typename T, std::size_t N,
          typename Seq = typename make_index_sequence<N>::type>
struct inject_fields;

template <typename T, std::size_t N, std::size_t... I>
struct inject_fields<T, N, index_sequence<I...>>
{
  static const std::size_t trigger = sizeof (T{ loophole_ubiq<T, I>{}... });
};

template <typename T, std::size_t I> struct field_type
{
  typedef typename std::remove_pointer<decltype (loophole_fn (
      loophole_tag<T, I>{}))>::type type;
};

template <typename T, std::size_t I> struct field_offset;

template <typename T> struct field_offset<T, 0>
{
  static const std::size_t value = 0;
};

template <typename T, std::size_t I> struct field_offset
{
  typedef typename field_type<T, I - 1>::type prev_t;
  typedef typename field_type<T, I>::type cur_t;
  static const std::size_t value
      = (field_offset<T, I - 1>::value + sizeof (prev_t) + alignof (cur_t) - 1)
        / alignof (cur_t) * alignof (cur_t);
};

template <typename T, std::size_t N> struct computed_sizeof
{
  typedef typename field_type<T, N - 1>::type last_t;
  static const std::size_t end
      = field_offset<T, N - 1>::value + sizeof (last_t);
  static const std::size_t value
      = (end + alignof (T) - 1) / alignof (T) * alignof (T);
};

template <typename T> struct computed_sizeof<T, 0>
{
  static const std::size_t value = 0;
};
#endif

template <typename T> struct aggregate_traits
{
  LUMEX_STATIC_ASSERT_MSG (std::is_class<T>::value && !std::is_union<T>::value,
                           "field reflection requires a non-union class type");
  LUMEX_STATIC_ASSERT_MSG (
      !std::is_polymorphic<T>::value,
      "field reflection does not support polymorphic types");
#if __cplusplus >= 201703L
  LUMEX_STATIC_ASSERT_MSG (std::is_aggregate<T>::value,
                           "field reflection requires an aggregate type");
#endif
  LUMEX_STATIC_ASSERT_MSG (can_construct_n<T, 0>::value,
                           "field reflection requires brace initialization");
  LUMEX_STATIC_ASSERT_MSG (
      !can_construct_n<T, k_max_aggregate_fields + 1>::value,
      "aggregate has more than 32 fields");

  static const std::size_t count
      = count_fields_impl<T, 0, k_max_aggregate_fields>::value;

#if __cplusplus >= 201402L
  typedef inject_fields<T, count> injected_t;
  static const std::size_t injected = injected_t::trigger;

  LUMEX_STATIC_ASSERT_MSG (
      count == 0
          || (injected > 0 && computed_sizeof<T, count>::value == sizeof (T)),
      "field reflection layout mismatch (no bit-fields, no "
      "reference members, no base classes)");
#endif
};

inline bool
is_ident_char (char ch) LUMEX_NOEXCEPT
{
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
         || (ch >= '0' && ch <= '9') || ch == '_';
}

inline void
copy_parsed_name (char *dest, std::size_t dest_size,
                  char const *pretty) LUMEX_NOEXCEPT
{
  std::size_t len = 0;
  while (pretty[len] != '\0')
    ++len;

  // GCC/Clang pretty strings use `.member`; MSVC __FUNCSIG__ uses
  // `->member` (`...storage<T>->value->id`). Prefer the last arrow, then
  // the last dot. Falling back to `:` alone picks the aggregate type name
  // on MSVC (`...::Plain,&...->value->id`) and is wrong.
  std::size_t arrow = static_cast<std::size_t> (-1);
  std::size_t dot = static_cast<std::size_t> (-1);
  std::size_t colon = static_cast<std::size_t> (-1);
  for (std::size_t i = 0; i < len; ++i)
    {
      if (pretty[i] == '.')
        dot = i;
      if (pretty[i] == ':')
        colon = i;
      if (i + 1 < len && pretty[i] == '-' && pretty[i + 1] == '>')
        arrow = i;
    }

  std::size_t start = 0;
  if (arrow != static_cast<std::size_t> (-1))
    start = arrow + 2;
  else if (dot != static_cast<std::size_t> (-1))
    start = dot + 1;
  else if (colon != static_cast<std::size_t> (-1))
    start = colon + 1;

  std::size_t out = 0;
  while (start < len && is_ident_char (pretty[start]) && out + 1 < dest_size)
    dest[out++] = pretty[start++];
  dest[out] = '\0';
}

#if __cplusplus >= 202002L
// Phantom storage for name extraction only: declared, never defined. Taking
// the address of a member is enough for a pointer NTTP; constructing a real
// `inline T fake{}` fails on MSVC (C2672) because a local reference into
// that object is not a constant expression for `template <auto>`.
template <typename T> struct fake_object_wrapper_t
{
  T const value;
};

template <typename T>
extern fake_object_wrapper_t<T> const fake_object_storage;

template <typename T>
LUMEX_CONSTEXPR T const &
fake_object () LUMEX_NOEXCEPT
{
  return fake_object_storage<T>.value;
}

#define LUMEX_AF_TIE(N, ...)                                                  \
  template <typename Aggregate>                                               \
  LUMEX_CONSTEXPR auto as_tied (Aggregate &&value,                            \
                                std::integral_constant<std::size_t, N>)       \
      LUMEX_NOEXCEPT                                                          \
  {                                                                           \
    auto &&[__VA_ARGS__] = value;                                             \
    return std::tie (__VA_ARGS__);                                            \
  }

template <typename Aggregate>
LUMEX_CONSTEXPR std::tuple<>
as_tied (Aggregate &&, std::integral_constant<std::size_t, 0>) LUMEX_NOEXCEPT
{
  return std::tuple<> ();
}

LUMEX_AF_TIE (1, a0)
LUMEX_AF_TIE (2, a0, a1)
LUMEX_AF_TIE (3, a0, a1, a2)
LUMEX_AF_TIE (4, a0, a1, a2, a3)
LUMEX_AF_TIE (5, a0, a1, a2, a3, a4)
LUMEX_AF_TIE (6, a0, a1, a2, a3, a4, a5)
LUMEX_AF_TIE (7, a0, a1, a2, a3, a4, a5, a6)
LUMEX_AF_TIE (8, a0, a1, a2, a3, a4, a5, a6, a7)
LUMEX_AF_TIE (9, a0, a1, a2, a3, a4, a5, a6, a7, a8)
LUMEX_AF_TIE (10, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)
LUMEX_AF_TIE (11, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
LUMEX_AF_TIE (12, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
LUMEX_AF_TIE (13, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
LUMEX_AF_TIE (14, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
LUMEX_AF_TIE (15, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14)
LUMEX_AF_TIE (16, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15)
LUMEX_AF_TIE (17, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16)
LUMEX_AF_TIE (18, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17)
LUMEX_AF_TIE (19, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18)
LUMEX_AF_TIE (20, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19)
LUMEX_AF_TIE (21, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20)
LUMEX_AF_TIE (22, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21)
LUMEX_AF_TIE (23, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22)
LUMEX_AF_TIE (24, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23)
LUMEX_AF_TIE (25, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24)
LUMEX_AF_TIE (26, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25)
LUMEX_AF_TIE (27, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26)
LUMEX_AF_TIE (28, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
              a27)
LUMEX_AF_TIE (29, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
              a27, a28)
LUMEX_AF_TIE (30, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
              a27, a28, a29)
LUMEX_AF_TIE (31, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
              a27, a28, a29, a30)
LUMEX_AF_TIE (32, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
              a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
              a27, a28, a29, a30, a31)

#undef LUMEX_AF_TIE

// Agg is unused but required: MSVC __FUNCSIG__ can collapse pointer NTTPs
// from different aggregates into the same string unless the type is a
// template argument alongside the pointer.
template <typename Agg, auto Pointer>
char const *
nttp_pretty () LUMEX_NOEXCEPT
{
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (sizeof (Agg));
  return LUMEX_FUNCTION_NAME;
}

template <typename Agg, std::size_t I>
char const *
field_name () LUMEX_NOEXCEPT
{
  static char buf[128] = { 0 };
  if (buf[0] == '\0')
    {
      copy_parsed_name (
          buf, sizeof (buf),
          nttp_pretty<Agg, std::addressof (std::get<I> (as_tied (
                               fake_object<Agg> (),
                               std::integral_constant<
                                   std::size_t,
                                   aggregate_traits<Agg>::count>{})))> ());
    }
  return buf;
}

template <typename Agg, typename Seq> struct names_builder;

template <typename Agg, std::size_t... I>
struct names_builder<Agg, index_sequence<I...>>
{
  static std::array<char const *, sizeof...(I)>
  build () LUMEX_NOEXCEPT
  {
    std::array<char const *, sizeof...(I)> names
        = { { field_name<Agg, I> ()... } };
    return names;
  }
};

template <typename Agg> struct names_builder<Agg, index_sequence<>>
{
  static std::array<char const *, 0>
  build () LUMEX_NOEXCEPT
  {
    return std::array<char const *, 0> ();
  }
};
#endif

#if __cplusplus >= 201402L
template <std::size_t I, typename Agg> struct qualified_field
{
  typedef typename std::remove_const<
      typename std::remove_reference<Agg>::type>::type bare_t;
  typedef typename field_type<bare_t, I>::type field_t;
  typedef typename std::conditional<
      std::is_const<typename std::remove_reference<Agg>::type>::value,
      field_t const, field_t>::type type;
};
#endif
} // namespace detail

template <typename Aggregate> struct tuple_size
{
  static const std::size_t value = detail::aggregate_traits<
      typename std::remove_cv<Aggregate>::type>::count;
};

#if __cplusplus >= 201402L
template <typename Aggregate>
LUMEX_CONSTEXPR std::size_t tuple_size_v = tuple_size<Aggregate>::value;
#endif

#if __cplusplus >= 201402L
template <std::size_t Index, typename Aggregate>
typename detail::qualified_field<Index, Aggregate &>::type &
get (Aggregate &value) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Aggregate>::type bare_t;
  LUMEX_STATIC_ASSERT_MSG (Index < tuple_size<bare_t>::value,
                           "field index out of range");
  typedef typename detail::qualified_field<Index, Aggregate &>::type qual_t;
  return *reinterpret_cast<qual_t *> (
      reinterpret_cast<char *> (std::addressof (value))
      + detail::field_offset<bare_t, Index>::value);
}

template <std::size_t Index, typename Aggregate>
typename detail::qualified_field<Index, Aggregate const &>::type &
get (Aggregate const &value) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Aggregate>::type bare_t;
  LUMEX_STATIC_ASSERT_MSG (Index < tuple_size<bare_t>::value,
                           "field index out of range");
  typedef
      typename detail::qualified_field<Index, Aggregate const &>::type qual_t;
  return *reinterpret_cast<qual_t const *> (
      reinterpret_cast<char const *> (std::addressof (value))
      + detail::field_offset<bare_t, Index>::value);
}
#endif

template <typename Aggregate>
std::array<char const *, tuple_size<Aggregate>::value>
names_as_array () LUMEX_NOEXCEPT
{
#if __cplusplus >= 202002L
  typedef typename std::remove_cv<Aggregate>::type bare_t;
  return detail::names_builder<bare_t,
                               typename detail::make_index_sequence<
                                   tuple_size<bare_t>::value>::type>::build ();
#else
  LUMEX_STATIC_ASSERT_MSG (
      sizeof (Aggregate) == 0,
      "names_as_array requires C++20 (pointer NTTP pretty "
      "names from LUMEX_FUNCTION_NAME)");
  return std::array<char const *, tuple_size<Aggregate>::value> ();
#endif
}
} // namespace field_reflection
} // namespace reflection
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_REFLECTION_FIELD_REFLECTION_AGGREGATE_FIELDS_HPP
