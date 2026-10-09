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
 * @file LumexAggregateFields.hpp
 * @brief Non-intrusive field count, indexed access and field names for simple
 *        aggregates, from C++11.
 * @details Replaces the three Boost.PFR calls FieldReflection used
 *          (`tuple_size`, `get`, `names_as_array`). This header is original
 *          Lumex code (MIT), not a relicensed Boost dump.
 *
 *          Field count is C++11: aggregate initialization against a
 *          converting placeholder, binary-searched up to 32 members.
 *
 *          Indexed `get` and field names have two sources.
 *          - Automatic, without any registration. `get` is C++14: friend-
 *            injected field types (ADL `friend auto` on the tag, Boost.PFR
 *            shape) plus sequential layout offsets, and from C++17
 *            structured bindings. Names are C++20: `LUMEX_FUNCTION_NAME`
 *            embeds an identifier only when that identifier is a template
 *            argument, and C++11 pointer NTTPs accept `&Class::member` (the
 *            name is already known) or the address of a complete object, not
 *            a pointer to the I-th member of a static instance. C++20
 *            `template<auto>` plus a structured binding into a declared-but-
 *            undefined phantom aggregate (`fake_object`) is what puts the
 *            real member name into the pretty string. A defined `inline T`
 *            instance fails on MSVC: a local reference into it is not a
 *            constant expression for the NTTP. The name is the identifier
 *            after the last `.`, `->` or `::` of the pretty string
 *            (`detail::member_name_begin`), whatever the compiler prints in
 *            front of it: GCC 13 qualifies the member by its class
 *            (`.ns::T::id`), the older GCC and Clang do not, MSVC separates
 *            the path with `->`. GCC 8 cannot take that pointer as a template
 *            argument, and reports 201709L at `-std=c++2a`, so it has no
 *            compiler names (the registration works there).
 *          - Registered, in every standard: `LUMEX_DEFINE_FIELD_NAMES (Type,
 *            a, b, c)` next to the aggregate lists its members once. The
 *            names come from the tokens and `get` from pointers to the
 *            members (a `decltype` of `&Type::a` gives the type, so C++11
 *            needs nothing else), hence neither needs a newer standard, and
 *            a registered type gives the same names and the same references
 *            at every standard, compiler and optimization level. The
 *            registration wins over the automatic sources wherever both
 *            exist (names at C++20, `get` from C++14); the I-th name and
 *            the I-th member are one field by construction. Without a
 *            registration, `names_as_array` and `to_json` below C++20 and
 *            `get` below C++14 are a `static_assert` that names the macro; an
 *            aggregate without fields needs no registration.
 *
 *          Limit: 32 public data members. No base classes, no
 *          bit-fields, no reference members, no array members (the count
 *          sees an array as several fields; the registration then fails its
 *          arity `static_assert`). From C++17,
 *          indexed `get` uses structured bindings (Boost.PFR
 *          core17 shape) so optional-only aggregates never hit the
 *          CWG 2118 loophole. The loophole remains for C++14 only:
 *          once-only friend body + `operator U&() const&&` (plain
 *          `operator U()` redefines the friend for both the wrapper
 *          and the contained type -> C2084; locking the contained
 *          type first then fails the layout sizeof check).
 */

#ifndef LUMEX_CORE_REFLECTION_FIELD_REFLECTION_AGGREGATE_FIELDS_HPP
#define LUMEX_CORE_REFLECTION_FIELD_REFLECTION_AGGREGATE_FIELDS_HPP

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
// The phantom object of an aggregate in an unnamed namespace is declared,
// never defined, and only its address is taken.
#pragma clang diagnostic ignored "-Wundefined-internal"
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

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

// Prefer structured bindings for get whenever the compiler can do them
// (feature test or C++17+). Otherwise the C++14 loophole path runs, and below
// C++14 get reads the member pointers of a registration. The three must be
// mutually exclusive: MSVC /std:c++20 sometimes omits
// __cpp_structured_bindings while still compiling bindings, and a stale
// loophole lock of std::optional's contained type then fails the layout
// sizeof check (ChannelAmqpError::error_message_t). Structured bindings need
// decltype (auto), so a compiler that reports the feature below C++14 still
// takes the registered path.
#if (defined(__cpp_structured_bindings) && __cplusplus >= 201402L)            \
    || __cplusplus >= 201703L
#define LUMEX_AGGREGATE_FIELDS_USE_SB 1
#include <tuple>
#else
#define LUMEX_AGGREGATE_FIELDS_USE_SB 0
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

// The index sequences of utility: std::index_sequence from C++14, an own
// class below it.
using lumex::core::utility::sequence::index_sequence;

template <std::size_t N> struct make_index_sequence
{
  typedef lumex::core::utility::sequence::make_index_sequence<N> type;
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

#if __cplusplus >= 201402L && !LUMEX_AGGREGATE_FIELDS_USE_SB
// CWG 2118 friend-injection loophole (Boost.PFR / Alexandr Poltavsky shape).
// MSVC may instantiate the converting operator for both a field type U and a
// type convertible into U (e.g. int and std::optional<int>). Defining the
// friend body on every hit yields C2084. Probe whether loophole_fn is already
// declared for (T, N); only the first successful U gets a body. operator U&()
// const&& (not operator U()) is required so optional-like fields resolve to
// the wrapper type, not the contained type.
template <typename T, std::size_t N> struct loophole_tag
{
  // The loophole: a non-template friend that loophole_set defines.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif
  friend auto loophole_fn (loophole_tag<T, N>);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
};

template <typename T, typename U, std::size_t N, bool AlreadyDefined>
struct loophole_set;

template <typename T, typename U, std::size_t N>
struct loophole_set<T, U, N, true>
{
};

template <typename T, typename U, std::size_t N>
struct loophole_set<T, U, N, false>
{
  friend auto
  loophole_fn (loophole_tag<T, N>)
  {
    return static_cast<U *> (nullptr);
  }
};

template <typename T, std::size_t N> struct loophole_ubiq
{
  template <typename U, std::size_t M> static std::size_t probe (...);

  template <typename U, std::size_t M,
            std::size_t = sizeof (loophole_fn (loophole_tag<T, M>{}))>
  static char probe (int);

  template <
      typename U,
      std::size_t = sizeof (
          loophole_set<T, U, N, (sizeof (probe<U, N> (0)) == sizeof (char))>)>
  operator U &() const &&;
};

template <typename T, std::size_t N,
          typename Seq = typename make_index_sequence<N>::type>
struct inject_fields;

template <typename T, std::size_t N, std::size_t... I>
struct inject_fields<T, N, index_sequence<I...>>
{
  static const std::size_t trigger = sizeof (T{ loophole_ubiq<T, I>{}... });
};

// Defines loophole_fn for every field of T. A static_assert of a class
// template is instantiated with the class, so a base of this type injects
// the fields before its derived class reads them. Without it only
// aggregate_traits<T> (tuple_size<T>) injected them, and a get<I> before
// the first tuple_size<T> of a translation unit did not compile.
template <typename T> struct inject_all_fields
{
  static const std::size_t count
      = count_fields_impl<T, 0, k_max_aggregate_fields>::value;
  LUMEX_STATIC_ASSERT_MSG (inject_fields<T, count>::trigger > 0,
                           "field types of the aggregate are injected");
};

template <typename T, std::size_t I> struct field_type : inject_all_fields<T>
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

#if __cplusplus >= 201402L && !LUMEX_AGGREGATE_FIELDS_USE_SB
  typedef inject_fields<T, count> injected_t;
  static const std::size_t injected = injected_t::trigger;

  LUMEX_STATIC_ASSERT_MSG (
      count == 0
          || (injected > 0 && computed_sizeof<T, count>::value == sizeof (T)),
      "field reflection layout mismatch (no bit-fields, no "
      "reference members, no base classes)");
#endif
};

// ---------------------------------------------------------------------------
// Registration (LUMEX_DEFINE_FIELD_NAMES), every standard.
//
// The macro defines, in the namespace of the aggregate, an overload of
// lumex_field_registry that takes registry_tag<Aggregate> and returns a
// registered_fields_t: the names (from the tokens) and, as the template
// arguments of the result type, one member_constant per field, in the order
// given. The pointers to the members are therefore constants of the type: get
// reads them at compile time, and nothing is built at run time except the
// pointer to the static array of names. Boost.Describe uses the same ADL
// shape, because a class template of this library cannot be specialized from
// the namespace of the user (an explicit specialization must be declared in a
// namespace that encloses the template).
// ---------------------------------------------------------------------------

// Whether the names of an unregistered aggregate come from the compiler
// (pointer NTTP pretty names). Below C++20 they do not.
LUMEX_CONSTEXPR bool k_automatic_names = (__cplusplus >= 202002L);

// The argument of the probe. The associated namespaces of a class template
// specialization include those of its template arguments, so the overload of
// the registration in the namespace of Aggregate is found by ADL.
template <typename Aggregate> struct registry_tag
{
};

// Result of the probe for an aggregate without a registration.
struct unregistered_t
{
};

// Fallback of the probe: declared, never defined, named only inside decltype.
// The overload of a registration is an exact match and beats the ellipsis.
unregistered_t lumex_field_registry (...);

// A pointer to a member as a type (C++11 has no template<auto>).
template <typename Pointer, Pointer Value> struct member_constant
{
  typedef Pointer pointer_t;

  static LUMEX_CONSTEXPR pointer_t
  get () LUMEX_NOEXCEPT
  {
    return Value;
  }
};

// Everything a registration knows about Aggregate: the names (a static array
// of the registration function) and, as Members, one member_constant per
// field.
template <typename Aggregate, typename... Members> struct registered_fields_t
{
  char const *const *names;
};

// The Index-th of the member constants.
template <std::size_t Index, typename... Members> struct nth_member;

template <typename Head, typename... Tail> struct nth_member<0, Head, Tail...>
{
  typedef Head type;
};

template <std::size_t Index, typename Head, typename... Tail>
struct nth_member<Index, Head, Tail...> : nth_member<Index - 1, Tail...>
{
};

template <typename Fields>
struct fields_count : std::integral_constant<std::size_t, 0>
{
};

template <typename Aggregate, typename... Members>
struct fields_count<registered_fields_t<Aggregate, Members...>>
    : std::integral_constant<std::size_t, sizeof...(Members)>
{
};

template <typename Fields, std::size_t Index> struct fields_member;

template <typename Aggregate, typename... Members, std::size_t Index>
struct fields_member<registered_fields_t<Aggregate, Members...>, Index>
    : nth_member<Index, Members...>
{
};

// Derives from true_type when Aggregate has a registration visible at the
// point of instantiation (declare the registration before the first use of
// the aggregate with this module).
template <typename Aggregate>
struct registry_of
    : std::integral_constant<bool,
                             !std::is_same<decltype (lumex_field_registry (
                                               registry_tag<Aggregate> ())),
                                           unregistered_t>::value>
{
  typedef decltype (lumex_field_registry (
      registry_tag<Aggregate> ())) fields_t;

  static fields_t
  fetch () LUMEX_NOEXCEPT
  {
    return lumex_field_registry (registry_tag<Aggregate> ());
  }
};

template <typename Aggregate, std::size_t... I>
std::array<char const *, sizeof...(I)>
registered_names (index_sequence<I...>) LUMEX_NOEXCEPT
{
  typename registry_of<Aggregate>::fields_t const fields
      = registry_of<Aggregate>::fetch ();
  std::array<char const *, sizeof...(I)> names = { { fields.names[I]... } };
  return names;
}

template <typename Pointer> struct member_value;

template <typename Class, typename Value> struct member_value<Value Class::*>
{
  typedef Value type;
};

// One field of a registration: the type of the member and the pointer to it.
// Usable is false for an unregistered aggregate or an index past the last
// field; get then reports it with a static_assert and returns a placeholder.
template <typename Aggregate, std::size_t Index, bool Usable>
struct registered_member
{
  typedef unregistered_t value_t;
};

template <typename Aggregate, std::size_t Index>
struct registered_member<Aggregate, Index, true>
{
  typedef typename fields_member<typename registry_of<Aggregate>::fields_t,
                                 Index>::type constant_t;
  typedef typename constant_t::pointer_t pointer_t;
  typedef typename member_value<pointer_t>::type value_t;

  static LUMEX_CONSTEXPR pointer_t
  pointer () LUMEX_NOEXCEPT
  {
    return constant_t::get ();
  }
};

template <typename Aggregate, std::size_t Index>
struct registered_usable
    : std::integral_constant<
          bool, registry_of<Aggregate>::value
                    && (Index < fields_count<
                            typename registry_of<Aggregate>::fields_t>::value)>
{
};

// Where the names of Aggregate come from; the value picks a names_of overload.
//   0 the registration, 1 the compiler (C++20 pointer NTTP pretty names),
//   2 nothing is needed (no fields), 3 missing: a static_assert reports it.
template <typename Aggregate>
struct names_source
    : std::integral_constant<
          int, registry_of<Aggregate>::value
                   ? 0
                   : (k_automatic_names
                          ? 1
                          : (aggregate_traits<Aggregate>::count == 0 ? 2 : 3))>
{
};

// A character of an identifier. A byte of a multibyte UTF-8 sequence counts,
// so that an identifier written in UTF-8 is read whole.
inline bool
is_ident_char (char ch) LUMEX_NOEXCEPT
{
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
         || (ch >= '0' && ch <= '9') || ch == '_'
         || static_cast<unsigned char> (ch) >= 0x80;
}

// An identifier does not start with a digit.
inline bool
is_ident_start (char ch) LUMEX_NOEXCEPT
{
  return is_ident_char (ch) && !(ch >= '0' && ch <= '9');
}

// Where the name of the member begins in the pretty string (the
// LUMEX_FUNCTION_NAME of nttp_pretty), or len if there is none.
//
// The pointer is the last template argument of nttp_pretty, and the member is
// the last thing the pointer names, so the name is the identifier that follows
// the last member access (`.`, `->`) or scope (`::`) operator of the string,
// and nothing but closing delimiters follows it. What stands before the
// operator does not matter, and that is the point: the compilers print the
// path to the member differently (the data in the parser tests).
//   GCC 8 .. 12   `...Pointer = (&
//   fake_object_storage<T>.fake_object_wrapper_t<T>::value.id)]` GCC 13
//   `...Pointer = (&
//   fake_object_storage<T>.fake_object_wrapper_t<T>::value.ns::T::id)]`
//                 (the member qualified by its class, which is also printed
//                 after the last `.`)
//   Clang         `...Pointer = &fake_object_storage.value.id]`
//   MSVC          `...nttp_pretty<struct ns::T,&fake_object_storage<struct
//   ns::T>->value->id>(void) noexcept`
// Taking the text after the last `.` or `->` alone yields `ns` for GCC 13.
// The operator must be followed by an identifier start: a `.` inside a
// floating-point template argument is followed by a digit, and an ellipsis by
// a dot. Such text can only stand before the member, and the last operator
// wins anyway.
inline std::size_t
member_name_begin (char const *pretty, std::size_t len) LUMEX_NOEXCEPT
{
  std::size_t begin = len;
  for (std::size_t i = 0; i < len; ++i)
    {
      std::size_t op_len = 0;
      if (pretty[i] == '.')
        op_len = 1;
      else if (i + 1 < len
               && ((pretty[i] == ':' && pretty[i + 1] == ':')
                   || (pretty[i] == '-' && pretty[i + 1] == '>')))
        op_len = 2;
      if (op_len == 0)
        continue;
      if (i + op_len < len && is_ident_start (pretty[i + op_len]))
        begin = i + op_len;
      i += op_len - 1; // both characters of `::` and `->` are consumed
    }
  return begin;
}

// Copies the name of the member out of the pretty string into dest, which is
// always NUL-terminated (dest_size characters at most, the name is cut if it
// does not fit). A string without a member gives an empty name.
inline void
copy_parsed_name (char *dest, std::size_t dest_size,
                  char const *pretty) LUMEX_NOEXCEPT
{
  if (dest_size == 0)
    return;

  std::size_t len = 0;
  while (pretty[len] != '\0')
    ++len;

  std::size_t start = member_name_begin (pretty, len);
  std::size_t out = 0;
  while (start < len && is_ident_char (pretty[start]) && out + 1 < dest_size)
    dest[out++] = pretty[start++];
  dest[out] = '\0';
}

#if LUMEX_AGGREGATE_FIELDS_USE_SB
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
#endif

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

// The pretty string of the I-th field of Agg: the LUMEX_FUNCTION_NAME of
// nttp_pretty with the pointer to that field as the template argument.
template <typename Agg, std::size_t I>
char const *
field_pretty () LUMEX_NOEXCEPT
{
  return nttp_pretty<
      Agg, std::addressof (std::get<I> (as_tied (
               fake_object<Agg> (),
               std::integral_constant<std::size_t,
                                      aggregate_traits<Agg>::count>{})))> ();
}

// The parsed name, filled once by the constructor of a function-local static,
// which the language initializes thread-safely.
template <typename Agg, std::size_t I> struct parsed_field_name
{
  char buf[128];

  parsed_field_name () LUMEX_NOEXCEPT
  {
    copy_parsed_name (buf, sizeof (buf), field_pretty<Agg, I> ());
  }
};

template <typename Agg, std::size_t I>
char const *
field_name () LUMEX_NOEXCEPT
{
  static parsed_field_name<Agg, I> const name;
  return name.buf;
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

// names_as_array, one overload per source (see names_source).
template <typename Agg>
std::array<char const *, aggregate_traits<Agg>::count>
names_of (std::integral_constant<int, 0>) LUMEX_NOEXCEPT
{
  return registered_names<Agg> (
      typename make_index_sequence<aggregate_traits<Agg>::count>::type ());
}

#if __cplusplus >= 202002L
template <typename Agg>
std::array<char const *, aggregate_traits<Agg>::count>
names_of (std::integral_constant<int, 1>) LUMEX_NOEXCEPT
{
  return names_builder<Agg, typename make_index_sequence<
                                aggregate_traits<Agg>::count>::type>::build ();
}
#endif

template <typename Agg>
std::array<char const *, aggregate_traits<Agg>::count>
names_of (std::integral_constant<int, 2>) LUMEX_NOEXCEPT
{
  return std::array<char const *, aggregate_traits<Agg>::count> ();
}

template <typename Agg>
std::array<char const *, aggregate_traits<Agg>::count>
names_of (std::integral_constant<int, 3>) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (
      sizeof (Agg) == 0,
      "names_as_array and to_json need the field names of the aggregate: "
      "register them with LUMEX_DEFINE_FIELD_NAMES (Type, field, ...) next "
      "to the definition of the type, or build for C++20 (names then come "
      "from the compiler)");
  return std::array<char const *, aggregate_traits<Agg>::count> ();
}

#if __cplusplus >= 201402L && !LUMEX_AGGREGATE_FIELDS_USE_SB
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

// get of an aggregate with a registration, in every standard: the I-th
// registered member. Agg may be a (const) reference or const. For an
// unregistered aggregate, or an index past the last registered field, value_t
// is unregistered_t: get then reports it (below C++14) or goes the automatic
// way (from C++14).
template <std::size_t I, typename Agg> struct registered_ref
{
  typedef typename std::remove_reference<Agg>::type plain_t;
  typedef typename std::remove_const<plain_t>::type bare_t;
  typedef typename registered_member<
      bare_t, I, registered_usable<bare_t, I>::value>::value_t value_t;
  typedef typename std::conditional<std::is_const<plain_t>::value,
                                    value_t const, value_t>::type &type;
};

template <std::size_t I, typename Agg>
typename registered_ref<I, Agg>::type
get_field (Agg &value, std::true_type) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Agg>::type bare_t;
  return value.*(registered_member<bare_t, I, true>::pointer ());
}

#if LUMEX_AGGREGATE_FIELDS_USE_SB
template <std::size_t I, typename Agg>
decltype (auto)
get_field (Agg &value, std::false_type) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Agg>::type bare_t;
  return std::get<I> (as_tied (
      value,
      std::integral_constant<std::size_t, aggregate_traits<bare_t>::count>{}));
}
#else
// The type get<I> returns below C++17 (from C++17 it is decltype (auto)).
template <std::size_t I, typename Agg,
          bool Registered = registered_usable<
              typename std::remove_cv<
                  typename std::remove_reference<Agg>::type>::type,
              I>::value>
struct get_result
{
  typedef typename registered_ref<I, Agg>::type type;
};

template <std::size_t I, typename Agg> struct get_result<I, Agg, false>
{
#if __cplusplus >= 201402L
  typedef typename qualified_field<I, Agg>::type &type;
#else
  typedef typename registered_ref<I, Agg>::type type;
#endif
};

#if __cplusplus >= 201402L
// CWG 2118 loophole: the field type of the injected friend, at the offset of
// the sequential layout.
template <std::size_t I, typename Agg>
typename qualified_field<I, Agg &>::type &
get_field (Agg &value, std::false_type) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Agg>::type bare_t;
  typedef typename qualified_field<I, Agg &>::type qual_t;
  return *reinterpret_cast<qual_t *> (
      reinterpret_cast<char *> (std::addressof (value))
      + field_offset<bare_t, I>::value);
}

template <std::size_t I, typename Agg>
typename qualified_field<I, Agg const &>::type &
get_field (Agg const &value, std::false_type) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Agg>::type bare_t;
  typedef typename qualified_field<I, Agg const &>::type qual_t;
  return *reinterpret_cast<qual_t const *> (
      reinterpret_cast<char const *> (std::addressof (value))
      + field_offset<bare_t, I>::value);
}
#else
// Reached only for an unregistered aggregate: the static_assert is the only
// diagnostic, the placeholder keeps the call well formed.
template <std::size_t I, typename Agg>
typename registered_ref<I, Agg>::type
get_field (Agg &, std::false_type) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Agg>::type bare_t;
  LUMEX_STATIC_ASSERT_MSG (
      registry_of<bare_t>::value,
      "get<I> below C++14 needs the members of the aggregate: register them "
      "with LUMEX_DEFINE_FIELD_NAMES (Type, field, ...) next to the "
      "definition of the type");
  static unregistered_t placeholder;
  return placeholder;
}
#endif
#endif
} // namespace detail

// Derived from std::integral_constant like std::tuple_size: `value` is then
// defined by the standard library in every standard. A plain in-class
// `static const` member is never implicitly inline, so binding it to a
// reference (EXPECT_EQ does) fails to link without optimization.
template <typename Aggregate>
struct tuple_size
    : std::integral_constant<
          std::size_t, detail::aggregate_traits<
                           typename std::remove_cv<Aggregate>::type>::count>
{
};

#if __cplusplus >= 201402L
template <typename Aggregate>
LUMEX_CONSTEXPR std::size_t tuple_size_v = tuple_size<Aggregate>::value;
#endif

/**
 * @brief The `Index`-th field of an aggregate, by reference.
 * @details An aggregate with a registration (`LUMEX_DEFINE_FIELD_NAMES`)
 *          reads the `Index`-th registered member, in every standard. An
 *          aggregate without one reads the `Index`-th field by the automatic
 *          means: from C++17 structured bindings, at C++14 the field types
 *          injected by a friend and the sequential layout. Below C++14 an
 *          aggregate without a registration is a `static_assert` that names
 *          the macro. The reference is `const` for a `const` aggregate.
 * @tparam Index The position of the field, below `tuple_size`.
 * @tparam Aggregate A simple aggregate (see `LumexFieldReflection.hpp`).
 * @param value The aggregate.
 * @return A reference to the field.
 */
#if LUMEX_AGGREGATE_FIELDS_USE_SB
template <std::size_t Index, typename Aggregate>
decltype (auto)
get (Aggregate &value) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Aggregate>::type bare_t;
  LUMEX_STATIC_ASSERT_MSG (Index < tuple_size<bare_t>::value,
                           "field index out of range");
  return detail::get_field<Index> (
      value, detail::registered_usable<bare_t, Index> ());
}

template <std::size_t Index, typename Aggregate>
decltype (auto)
get (Aggregate const &value) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Aggregate>::type bare_t;
  LUMEX_STATIC_ASSERT_MSG (Index < tuple_size<bare_t>::value,
                           "field index out of range");
  return detail::get_field<Index> (
      value, detail::registered_usable<bare_t, Index> ());
}
#else
template <std::size_t Index, typename Aggregate>
typename detail::get_result<Index, Aggregate &>::type
get (Aggregate &value) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Aggregate>::type bare_t;
  LUMEX_STATIC_ASSERT_MSG (Index < tuple_size<bare_t>::value,
                           "field index out of range");
  return detail::get_field<Index> (
      value, detail::registered_usable<bare_t, Index> ());
}

template <std::size_t Index, typename Aggregate>
typename detail::get_result<Index, Aggregate const &>::type
get (Aggregate const &value) LUMEX_NOEXCEPT
{
  typedef typename std::remove_const<Aggregate>::type bare_t;
  LUMEX_STATIC_ASSERT_MSG (Index < tuple_size<bare_t>::value,
                           "field index out of range");
  return detail::get_field<Index> (
      value, detail::registered_usable<bare_t, Index> ());
}
#endif

/**
 * @brief The names of the fields of an aggregate, in declaration order.
 * @details The names come from the registration of the aggregate
 *          (`LUMEX_DEFINE_FIELD_NAMES`) when there is one, in every
 *          standard; otherwise, from C++20, from the compiler. Below C++20 an
 *          aggregate without a registration is a `static_assert` that names
 *          the macro, except an aggregate without fields, whose array is
 *          empty.
 * @tparam Aggregate A simple aggregate (see `LumexFieldReflection.hpp`).
 * @return One pointer to a static string per field. The strings live for the
 *         whole run.
 */
template <typename Aggregate>
std::array<char const *, tuple_size<Aggregate>::value>
names_as_array () LUMEX_NOEXCEPT
{
  typedef typename std::remove_cv<Aggregate>::type bare_t;
  return detail::names_of<bare_t> (
      std::integral_constant<int, detail::names_source<bare_t>::value> ());
}
} // namespace field_reflection
} // namespace reflection
} // namespace core
} // namespace lumex

// clang-format off

// LUMEX_DEFINE_FIELD_NAMES helpers. The macro takes 1 to 32 names. As in
// LUMEX_DEFINE_REFLECTED_ENUM, LUMEX_FIELD_NAMES_EXPAND around every nested
// call keeps the traditional MSVC preprocessor from passing __VA_ARGS__ on as
// one argument. They stay defined: the registration macro expands to them.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define LUMEX_FIELD_NAMES_EXPAND(x) x
#define LUMEX_FIELD_NAMES_CAT_(a, b) a##b
#define LUMEX_FIELD_NAMES_CAT(a, b) LUMEX_FIELD_NAMES_CAT_ (a, b)

// The names are counted up to 33 so that one name too many selects
// LUMEX_FIELD_NAMES_FE_33 below, which does not compile. The trailing 0 keeps
// the `...` of LUMEX_FIELD_NAMES_ARG_N from being empty (-Wpedantic before
// C++20).
#define LUMEX_FIELD_NAMES_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, N, ...) N
#define LUMEX_FIELD_NAMES_COUNT(...) \
  LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_ARG_N (__VA_ARGS__, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#define LUMEX_FIELD_NAMES_FE_1(F, A, x) F (A, x)
#define LUMEX_FIELD_NAMES_FE_2(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_1 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_3(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_2 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_4(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_3 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_5(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_4 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_6(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_5 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_7(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_6 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_8(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_7 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_9(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_8 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_10(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_9 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_11(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_10 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_12(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_11 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_13(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_12 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_14(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_13 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_15(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_14 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_16(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_15 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_17(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_16 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_18(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_17 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_19(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_18 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_20(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_19 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_21(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_20 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_22(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_21 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_23(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_22 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_24(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_23 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_25(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_24 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_26(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_25 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_27(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_26 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_28(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_27 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_29(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_28 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_30(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_29 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_31(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_30 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_32(F, A, x, ...) F (A, x), LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_FE_31 (F, A, __VA_ARGS__))
#define LUMEX_FIELD_NAMES_FE_33(...) LUMEX_DEFINE_FIELD_NAMES_TAKES_AT_MOST_32_NAMES

#define LUMEX_FIELD_NAMES_FOR_EACH(F, A, ...) \
  LUMEX_FIELD_NAMES_EXPAND(LUMEX_FIELD_NAMES_CAT(LUMEX_FIELD_NAMES_FE_, LUMEX_FIELD_NAMES_COUNT(__VA_ARGS__))(F, A, __VA_ARGS__))

#define LUMEX_FIELD_NAMES_MEMBER(Aggregate, name)                               \
  ::lumex::core::reflection::field_reflection::detail::member_constant<         \
      decltype (&Aggregate::name), &Aggregate::name>
#define LUMEX_FIELD_NAMES_STRING(Aggregate, name) #name

/**
 * @def LUMEX_DEFINE_FIELD_NAMES
 * @brief Registers the fields of an aggregate: their names, and pointers to
 *        them, from C++11.
 * @details `LUMEX_DEFINE_FIELD_NAMES (Type, a, b, c);` goes at namespace scope
 *          in the namespace of `Type`, after the definition of the type and
 *          before the first use of field reflection on it (the first
 *          `names_as_array`, `get`, `to_json`). Name every non-static data
 *          member, in declaration order, 1 to 32 of them. `Type` may be
 *          qualified (`Host::inner_t`) and may be in an unnamed namespace;
 *          it must not contain a top-level comma (use a `typedef`). An
 *          aggregate without fields needs no registration.
 *
 *          The macro defines an inline function that ADL finds from
 *          `field_reflection`, and checks the number of names against the
 *          number of fields of the aggregate with a `static_assert`. A name
 *          that is not a member of `Type` does not compile. The order of the
 *          names is not checked: list the members as they are declared.
 *
 *          With a registration, `names_as_array`, `to_json` and `get<I>`
 *          use it in every standard (the registration wins over the names
 *          the compiler supplies at C++20 and over the automatic `get` from
 *          C++14), so a registered type behaves the same at C++11 and C++20.
 *          The I-th name and the I-th member are one field by construction,
 *          also if the registration lists them in another order than the
 *          declaration; `get<I>` is then not the I-th declared member.
 *
 * @code
 * namespace app {
 * struct point { int x; int y; };
 * LUMEX_DEFINE_FIELD_NAMES (point, x, y);
 * }
 * @endcode
 */
#define LUMEX_DEFINE_FIELD_NAMES(Aggregate, ...)                                \
  inline ::lumex::core::reflection::field_reflection::detail::                  \
      registered_fields_t<Aggregate, LUMEX_FIELD_NAMES_FOR_EACH (               \
                                         LUMEX_FIELD_NAMES_MEMBER, Aggregate,   \
                                         __VA_ARGS__)>                          \
      lumex_field_registry (::lumex::core::reflection::field_reflection::       \
                                detail::registry_tag<Aggregate>)                \
  {                                                                             \
    static char const *const names[] = { LUMEX_FIELD_NAMES_FOR_EACH (           \
        LUMEX_FIELD_NAMES_STRING, Aggregate, __VA_ARGS__) };                    \
    return ::lumex::core::reflection::field_reflection::detail::                \
        registered_fields_t<Aggregate, LUMEX_FIELD_NAMES_FOR_EACH (             \
                                           LUMEX_FIELD_NAMES_MEMBER,            \
                                           Aggregate, __VA_ARGS__)>{ names };   \
  }                                                                             \
  LUMEX_STATIC_ASSERT_MSG (                                                     \
      ::lumex::core::reflection::field_reflection::tuple_size<Aggregate>::value \
          == LUMEX_FIELD_NAMES_COUNT (__VA_ARGS__),                             \
      "LUMEX_DEFINE_FIELD_NAMES: the number of names differs from the number "  \
      "of fields of the aggregate (arrays, base classes and bit-fields are "    \
      "not supported)")
// NOLINTEND(cppcoreguidelines-macro-usage)

// clang-format on

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#undef LUMEX_AGGREGATE_FIELDS_USE_SB

#endif // !LUMEX_CORE_REFLECTION_FIELD_REFLECTION_AGGREGATE_FIELDS_HPP
