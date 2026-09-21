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
 * @file LumexReflectedEnum.hpp
 * @brief X-macro system that generates a scoped enum together with its own
 *        compile-time reflection data and a `toString()` function.
 * @details `DEFINE_REFLECTED_ENUM(EnumName, UnderlyingType, (A), (B, 5), (C))`
 *          expands to an `enum class EnumName : UnderlyingType { A, B = 5, C
 * };` plus four `constexpr` companions declared right next to it -
 *          `EnumNameValues` (a `std::array` of every enumerator, in
 *          declaration order), `EnumNameFirst`, `EnumNameSize`, and
 *          `EnumNameLast` - and a `toString(EnumName)` function.
 */
#ifndef LUMEX_CORE_REFLECTION_REFLECTED_ENUM_HPP
#define LUMEX_CORE_REFLECTION_REFLECTED_ENUM_HPP

#include <array>
#include <cstddef>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// clang-format off

// Each entry passed to DEFINE_REFLECTED_ENUM is (Name) or (Name, Value), e.g.
//   DEFINE_REFLECTED_ENUM(Foo, std::uint8_t, (A), (B, 5), (C))
// The wrapping parens make every entry ONE macro argument regardless of an
// internal comma, so LUMEX_PP_ARG_COUNT below counts entries, not tokens.
//
// DEFINE_REFLECTED_ENUM always emits toString(EnumName). Display strings
// default to the enumerator identifiers (#Name): toString(Foo::B) == "B".
//
// Custom strings: do NOT also call DEFINE_REFLECTED_ENUM (that would define
// toString twice). Use DEFINE_REFLECTED_ENUM_TO_STRING instead:
//   #define FOO_STRINGS(ENTRY) ENTRY(A, "alpha") ENTRY(B, "beta") ENTRY(C, "charlie")
//   DEFINE_REFLECTED_ENUM_TO_STRING(Foo, std::uint8_t, FOO_STRINGS, (A), (B, 5), (C))
//   char const* s = toString(Foo::B); // "beta"
// There must be no comma between ENTRY(...) entries. Do not name the X-macro
// parameter after an enumerator (ENTRY(X, "ex") is fine; X(X, "ex") is not:
// the preprocessor would replace both identifiers). Unlisted enumerators and
// non-enumerator values of a fixed underlying type return "<Unknown>" (an
// enumerator can still map to "" explicitly via its own X-macro entry - that
// is a distinct, deliberate case, not the fallback). The returned pointer is
// never null.
//
// Reflection data is emitted as plain EnumName##Values/First/Size/Last
// constants next to the enum itself, NOT as an EnumTraits<Enum> specialization.
// Two independent reasons rule that out, both stemming from [temp.expl.spec]
// (an explicit specialization must be declared in the SAME namespace as its
// primary template):
//   1. MSVC (C2888) additionally refuses a lambda inside such a specialization
//      when it is defined inside a namespace - an earlier design built
//      EnumTraits<Enum>::values via `[]{ using enum Enum; return std::array{...}; }()`.
//   2. Even lambda-free, DEFINE_REFLECTED_ENUM is meant to be invoked from
//      inside arbitrary namespaces, or even inside a nested struct/class -
//      neither is the global scope EnumTraits<T> would need, so the
//      specialization itself would be ill-formed regardless of what is inside it.
// `static constexpr` on each generated constant is deliberate: at namespace
// scope it just gives ordinary internal linkage (harmless per-TU duplication
// of a few small constexpr values); at class scope it is required for the
// constant to be a shared class constant rather than a bogus non-static data
// member. One spelling, both invocation contexts.

// - FOR_EACH, unrolled up to 16 entries (extend if a reflected enum needs more) -
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define LUMEX_PP_EXPAND(x) x

#define LUMEX_PP_ARG_N(                                                    \
  _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N
#define LUMEX_PP_ARG_COUNT(...) \
  LUMEX_PP_EXPAND(LUMEX_PP_ARG_N(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))

#define LUMEX_PP_CAT_(a, b) a##b
#define LUMEX_PP_CAT(a, b)  LUMEX_PP_CAT_(a, b)

// apply F(x) to each entry, comma-joined
#define LUMEX_PP_FE_1(F, x)       F(x)
#define LUMEX_PP_FE_2(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_1(F, __VA_ARGS__))
#define LUMEX_PP_FE_3(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_2(F, __VA_ARGS__))
#define LUMEX_PP_FE_4(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_3(F, __VA_ARGS__))
#define LUMEX_PP_FE_5(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_4(F, __VA_ARGS__))
#define LUMEX_PP_FE_6(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_5(F, __VA_ARGS__))
#define LUMEX_PP_FE_7(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_6(F, __VA_ARGS__))
#define LUMEX_PP_FE_8(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_7(F, __VA_ARGS__))
#define LUMEX_PP_FE_9(F, x, ...)  F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_8(F, __VA_ARGS__))
#define LUMEX_PP_FE_10(F, x, ...) F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_9(F, __VA_ARGS__))
#define LUMEX_PP_FE_11(F, x, ...) F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_10(F, __VA_ARGS__))
#define LUMEX_PP_FE_12(F, x, ...) F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_11(F, __VA_ARGS__))
#define LUMEX_PP_FE_13(F, x, ...) F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_12(F, __VA_ARGS__))
#define LUMEX_PP_FE_14(F, x, ...) F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_13(F, __VA_ARGS__))
#define LUMEX_PP_FE_15(F, x, ...) F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_14(F, __VA_ARGS__))
#define LUMEX_PP_FE_16(F, x, ...) F(x), LUMEX_PP_EXPAND(LUMEX_PP_FE_15(F, __VA_ARGS__))

#define LUMEX_PP_FOR_EACH(F, ...) \
  LUMEX_PP_EXPAND(LUMEX_PP_CAT(LUMEX_PP_FE_, LUMEX_PP_ARG_COUNT(__VA_ARGS__))(F, __VA_ARGS__))

// same, but F also receives a fixed leading argument A on every call
#define LUMEX_PP_FEA_1(F, A, x)       F(A, x)
#define LUMEX_PP_FEA_2(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_1(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_3(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_2(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_4(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_3(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_5(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_4(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_6(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_5(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_7(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_6(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_8(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_7(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_9(F, A, x, ...)  F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_8(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_10(F, A, x, ...) F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_9(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_11(F, A, x, ...) F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_10(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_12(F, A, x, ...) F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_11(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_13(F, A, x, ...) F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_12(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_14(F, A, x, ...) F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_13(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_15(F, A, x, ...) F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_14(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_16(F, A, x, ...) F(A, x), LUMEX_PP_EXPAND(LUMEX_PP_FEA_15(F, A, __VA_ARGS__))

#define LUMEX_PP_FOR_EACH_ARG(F, A, ...) \
  LUMEX_PP_EXPAND(LUMEX_PP_CAT(LUMEX_PP_FEA_, LUMEX_PP_ARG_COUNT(__VA_ARGS__))(F, A, __VA_ARGS__))

// same as LUMEX_PP_FOR_EACH_ARG but not comma-joined (switch arms)
#define LUMEX_PP_FEA_NC_1(F, A, x)       F(A, x)
#define LUMEX_PP_FEA_NC_2(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_1(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_3(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_2(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_4(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_3(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_5(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_4(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_6(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_5(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_7(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_6(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_8(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_7(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_9(F, A, x, ...)  F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_8(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_10(F, A, x, ...) F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_9(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_11(F, A, x, ...) F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_10(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_12(F, A, x, ...) F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_11(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_13(F, A, x, ...) F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_12(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_14(F, A, x, ...) F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_13(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_15(F, A, x, ...) F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_14(F, A, __VA_ARGS__))
#define LUMEX_PP_FEA_NC_16(F, A, x, ...) F(A, x) LUMEX_PP_EXPAND(LUMEX_PP_FEA_NC_15(F, A, __VA_ARGS__))

#define LUMEX_PP_FOR_EACH_ARG_NC(F, A, ...) \
  LUMEX_PP_EXPAND(LUMEX_PP_CAT(LUMEX_PP_FEA_NC_, LUMEX_PP_ARG_COUNT(__VA_ARGS__))(F, A, __VA_ARGS__))

// per-entry expansion: entry is (Name) or (Name, Value)
#define LUMEX_PP_ENUM_BODY_PICK(_1, _2, N, ...) N
#define LUMEX_PP_ENUM_BODY_1(name)        name
#define LUMEX_PP_ENUM_BODY_2(name, value) name = value
#define LUMEX_PP_ENUM_BODY_DISPATCH(...) \
  LUMEX_PP_EXPAND(LUMEX_PP_ENUM_BODY_PICK(__VA_ARGS__, LUMEX_PP_ENUM_BODY_2, LUMEX_PP_ENUM_BODY_1)(__VA_ARGS__))
#define LUMEX_PP_ENUM_BODY(entry) LUMEX_PP_ENUM_BODY_DISPATCH entry

#define LUMEX_PP_UNWRAP(...) __VA_ARGS__

#define LUMEX_PP_ENUM_VALUE_PICK(_1, _2, N, ...) N
#define LUMEX_PP_ENUM_VALUE_1(enumName, name)        enumName::name
#define LUMEX_PP_ENUM_VALUE_2(enumName, name, value) enumName::name
#define LUMEX_PP_ENUM_VALUE_DISPATCH(enumName, ...) \
  LUMEX_PP_EXPAND(LUMEX_PP_ENUM_VALUE_PICK(__VA_ARGS__, LUMEX_PP_ENUM_VALUE_2, LUMEX_PP_ENUM_VALUE_1)(enumName, __VA_ARGS__))
#define LUMEX_PP_ENUM_VALUE(enumName, entry) LUMEX_PP_ENUM_VALUE_DISPATCH(enumName, LUMEX_PP_UNWRAP entry)

#define LUMEX_PP_ENUM_DEFAULT_STRING_1(name)        #name
#define LUMEX_PP_ENUM_DEFAULT_STRING_2(name, value) #name
#define LUMEX_PP_ENUM_DEFAULT_STRING_DISPATCH(...) \
  LUMEX_PP_EXPAND(LUMEX_PP_ENUM_BODY_PICK(__VA_ARGS__, LUMEX_PP_ENUM_DEFAULT_STRING_2, LUMEX_PP_ENUM_DEFAULT_STRING_1)(__VA_ARGS__))
#define LUMEX_PP_ENUM_DEFAULT_STRING(entry) LUMEX_PP_ENUM_DEFAULT_STRING_DISPATCH entry

#define LUMEX_PP_ENUM_NAME_1(name)        name
#define LUMEX_PP_ENUM_NAME_2(name, value) name
#define LUMEX_PP_ENUM_NAME_DISPATCH(...) \
  LUMEX_PP_EXPAND(LUMEX_PP_ENUM_BODY_PICK(__VA_ARGS__, LUMEX_PP_ENUM_NAME_2, LUMEX_PP_ENUM_NAME_1)(__VA_ARGS__))
#define LUMEX_PP_ENUM_NAME(entry) LUMEX_PP_ENUM_NAME_DISPATCH entry

#define LUMEX_PP_ENUM_DEFAULT_TO_STRING_ARM(enumName, entry) \
  case LumexToStringEnum_::LUMEX_PP_ENUM_NAME(entry):        \
    return LUMEX_PP_ENUM_DEFAULT_STRING(entry);

#define LUMEX_PP_DEFINE_REFLECTED_ENUM_DECL(EnumName, UnderlyingType, ...)                          \
  enum class EnumName : UnderlyingType                                                              \
  {                                                                                                  \
    LUMEX_PP_FOR_EACH(LUMEX_PP_ENUM_BODY, __VA_ARGS__)                                              \
  };                                                                                                 \
                                                                                                      \
  LUMEX_ATTRIBUTE_MAYBE_UNUSED LUMEX_CONST_NUM std::array<EnumName, LUMEX_PP_ARG_COUNT(__VA_ARGS__)>            \
    LUMEX_PP_CAT(EnumName, Values){                                                                  \
      LUMEX_PP_FOR_EACH_ARG(LUMEX_PP_ENUM_VALUE, EnumName, __VA_ARGS__)                              \
    };                                                                                               \
  LUMEX_ATTRIBUTE_MAYBE_UNUSED LUMEX_CONST_NUM EnumName LUMEX_PP_CAT(EnumName, First) =                         \
    LUMEX_PP_CAT(EnumName, Values).front();                                                          \
  LUMEX_ATTRIBUTE_MAYBE_UNUSED LUMEX_CONST_NUM std::size_t LUMEX_PP_CAT(EnumName, Size) =                       \
    LUMEX_PP_CAT(EnumName, Values).size();                                                           \
  LUMEX_ATTRIBUTE_MAYBE_UNUSED LUMEX_CONST_NUM EnumName LUMEX_PP_CAT(EnumName, Last) =                          \
    LUMEX_PP_CAT(EnumName, Values).back();

#define LUMEX_PP_ENUM_TO_STRING_CASE(enumerator, str) \
  case LumexToStringEnum_::enumerator:                \
    return str;

#define LUMEX_PP_DEFINE_TO_STRING_FN(EnumName, SwitchBody)                                       \
  LUMEX_ATTRIBUTE_NODISCARD ("return value must be used") LUMEX_ATTRIBUTE_MAYBE_UNUSED static LUMEX_CONSTEXPR char const * toString(EnumName value) LUMEX_NOEXCEPT  \
  {                                                                                               \
    using LumexToStringEnum_ = EnumName;                                                         \
    switch(value)                                                                                \
    {                                                                                             \
      SwitchBody                                                                                 \
    default:                                                                                     \
      return "<Unknown>";                                                                        \
    }                                                                                             \
  }

#define DEFINE_REFLECTED_ENUM(EnumName, UnderlyingType, ...)             \
  LUMEX_PP_DEFINE_REFLECTED_ENUM_DECL(EnumName, UnderlyingType, __VA_ARGS__) \
  LUMEX_PP_DEFINE_TO_STRING_FN(                                          \
    EnumName,                                                            \
    LUMEX_PP_FOR_EACH_ARG_NC(LUMEX_PP_ENUM_DEFAULT_TO_STRING_ARM, EnumName, __VA_ARGS__))

#define DEFINE_REFLECTED_ENUM_TO_STRING(EnumName, UnderlyingType, XList, ...) \
  LUMEX_PP_DEFINE_REFLECTED_ENUM_DECL(EnumName, UnderlyingType, __VA_ARGS__)  \
  LUMEX_PP_DEFINE_TO_STRING_FN(EnumName, XList(LUMEX_PP_ENUM_TO_STRING_CASE))
// NOLINTEND(cppcoreguidelines-macro-usage)
// clang-format on

#endif // !LUMEX_CORE_REFLECTION_REFLECTED_ENUM_HPP
