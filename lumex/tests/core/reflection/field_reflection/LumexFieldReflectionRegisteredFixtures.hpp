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

// The aggregates registered with LUMEX_DEFINE_FIELD_NAMES that the
// LumexFieldNames test files of every standard share. They are small (up to
// five members, one with twelve), and reg_fields_of<N> has a registered
// aggregate of every size from 1 to 32 (the members f0..f31 of fields_of<N>,
// the same type cycle) for the registration alone: what a test does with them
// is chosen per file, because every instantiation per member costs compile
// time and code size. None of these types is also used by the tests of the
// automatic path (LumexFieldReflectionTestFixtures.hpp): a registration wins
// over the automatic names, so the same type would test the registration
// there.

#ifndef LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_REGISTERED_HPP
#define LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_REGISTERED_HPP

#include <cstddef>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/field_reflection/LumexFieldReflectionTestFixtures.hpp"

// Registered at global scope.
struct RegGlobalType
{
  int g0;
  double g1;
};
LUMEX_DEFINE_FIELD_NAMES (RegGlobalType, g0, g1);

// Registered inside an unnamed namespace: ADL finds the function there too.
namespace
{
struct RegHiddenType
{
  long long v;
  bool flag;
};
LUMEX_DEFINE_FIELD_NAMES (RegHiddenType, v, flag);
} // namespace

namespace lumex_field_reflection_tests
{
struct RegPlain
{
  int id;
  std::string name;
};
LUMEX_DEFINE_FIELD_NAMES (RegPlain, id, name);

struct RegOneField
{
  int only;
};
LUMEX_DEFINE_FIELD_NAMES (RegOneField, only);

struct RegPadded
{
  char a;
  int b;
  double c;
};
LUMEX_DEFINE_FIELD_NAMES (RegPadded, a, b, c);

struct RegScalars
{
  bool flag;
  double ratio;
  std::string empty;
};
LUMEX_DEFINE_FIELD_NAMES (RegScalars, flag, ratio, empty);

struct RegUnderscoreNames
{
  int field_a;
  int field_b;
};
LUMEX_DEFINE_FIELD_NAMES (RegUnderscoreNames, field_a, field_b);

struct RegWithVector
{
  std::vector<int> values;
};
LUMEX_DEFINE_FIELD_NAMES (RegWithVector, values);

struct RegWithOptional
{
  int id;
  optional<std::string> label;
};
LUMEX_DEFINE_FIELD_NAMES (RegWithOptional, id, label);

struct RegMixedOptionals
{
  optional<int> missing;
  int always;
  optional<int> present;
};
LUMEX_DEFINE_FIELD_NAMES (RegMixedOptionals, missing, always, present);

// A nested aggregate: one field of its parent, written by nlohmann through
// the to_json overload below, which ADL finds.
struct RegPoint
{
  int x;
  int y;
};
LUMEX_DEFINE_FIELD_NAMES (RegPoint, x, y);

inline void
to_json (nlohmann::json &j, RegPoint const &point)
{
  j = lumex::core::reflection::field_reflection::to_json (point);
}

struct RegShape
{
  std::string title;
  RegPoint origin;
  std::vector<RegPoint> corners;
  double scale;
};
LUMEX_DEFINE_FIELD_NAMES (RegShape, title, origin, corners, scale);

// An aggregate nested in a class: the registration names it by its qualified
// name, at namespace scope.
class RegHost
{
public:
  struct inner_t
  {
    int device;
    std::string label;
  };
};
LUMEX_DEFINE_FIELD_NAMES (RegHost::inner_t, device, label);

namespace reg_inner_ns
{
struct RegInNamespace
{
  long long big;
  bool flag;
};
LUMEX_DEFINE_FIELD_NAMES (RegInNamespace, big, flag);
} // namespace reg_inner_ns

// A class template instance: the macro takes the type without a top-level
// comma, so the instance is named through a typedef.
template <typename First, typename Second> struct RegPairLike
{
  First first;
  Second second;
};
typedef RegPairLike<int, double> reg_pair_t;
LUMEX_DEFINE_FIELD_NAMES (reg_pair_t, first, second);

struct RegTwelveFields
{
  int a0;
  int a1;
  int a2;
  int a3;
  int a4;
  int a5;
  int a6;
  int a7;
  int a8;
  int a9;
  int a10;
  int a11;
};
LUMEX_DEFINE_FIELD_NAMES (RegTwelveFields, a0, a1, a2, a3, a4, a5, a6, a7, a8,
                          a9, a10, a11);

// Registered in the reverse of the declaration order on purpose: the
// registration is the source of truth for the names and the members of a
// registered type, in every standard, even where an automatic source exists.
struct RegSwapped
{
  int a;
  int b;
};
LUMEX_DEFINE_FIELD_NAMES (RegSwapped, b, a);

template <std::size_t N> struct reg_fields_of;

#define LUMEX_FR_NAMES_1 f0
#define LUMEX_FR_NAMES_2 LUMEX_FR_NAMES_1, f1
#define LUMEX_FR_NAMES_3 LUMEX_FR_NAMES_2, f2
#define LUMEX_FR_NAMES_4 LUMEX_FR_NAMES_3, f3
#define LUMEX_FR_NAMES_5 LUMEX_FR_NAMES_4, f4
#define LUMEX_FR_NAMES_6 LUMEX_FR_NAMES_5, f5
#define LUMEX_FR_NAMES_7 LUMEX_FR_NAMES_6, f6
#define LUMEX_FR_NAMES_8 LUMEX_FR_NAMES_7, f7
#define LUMEX_FR_NAMES_9 LUMEX_FR_NAMES_8, f8
#define LUMEX_FR_NAMES_10 LUMEX_FR_NAMES_9, f9
#define LUMEX_FR_NAMES_11 LUMEX_FR_NAMES_10, f10
#define LUMEX_FR_NAMES_12 LUMEX_FR_NAMES_11, f11
#define LUMEX_FR_NAMES_13 LUMEX_FR_NAMES_12, f12
#define LUMEX_FR_NAMES_14 LUMEX_FR_NAMES_13, f13
#define LUMEX_FR_NAMES_15 LUMEX_FR_NAMES_14, f14
#define LUMEX_FR_NAMES_16 LUMEX_FR_NAMES_15, f15
#define LUMEX_FR_NAMES_17 LUMEX_FR_NAMES_16, f16
#define LUMEX_FR_NAMES_18 LUMEX_FR_NAMES_17, f17
#define LUMEX_FR_NAMES_19 LUMEX_FR_NAMES_18, f18
#define LUMEX_FR_NAMES_20 LUMEX_FR_NAMES_19, f19
#define LUMEX_FR_NAMES_21 LUMEX_FR_NAMES_20, f20
#define LUMEX_FR_NAMES_22 LUMEX_FR_NAMES_21, f21
#define LUMEX_FR_NAMES_23 LUMEX_FR_NAMES_22, f22
#define LUMEX_FR_NAMES_24 LUMEX_FR_NAMES_23, f23
#define LUMEX_FR_NAMES_25 LUMEX_FR_NAMES_24, f24
#define LUMEX_FR_NAMES_26 LUMEX_FR_NAMES_25, f25
#define LUMEX_FR_NAMES_27 LUMEX_FR_NAMES_26, f26
#define LUMEX_FR_NAMES_28 LUMEX_FR_NAMES_27, f27
#define LUMEX_FR_NAMES_29 LUMEX_FR_NAMES_28, f28
#define LUMEX_FR_NAMES_30 LUMEX_FR_NAMES_29, f29
#define LUMEX_FR_NAMES_31 LUMEX_FR_NAMES_30, f30
#define LUMEX_FR_NAMES_32 LUMEX_FR_NAMES_31, f31

#define LUMEX_FR_REG_DEFINE(N)                                                \
  struct reg_fields_##N##_t                                                   \
  {                                                                           \
    LUMEX_FR_DECL_##N;                                                        \
  };                                                                          \
  LUMEX_DEFINE_FIELD_NAMES (reg_fields_##N##_t, LUMEX_FR_NAMES_##N);          \
  template <> struct reg_fields_of<N>                                         \
  {                                                                           \
    typedef reg_fields_##N##_t type;                                          \
  }

LUMEX_FR_REG_DEFINE (1);
LUMEX_FR_REG_DEFINE (2);
LUMEX_FR_REG_DEFINE (3);
LUMEX_FR_REG_DEFINE (4);
LUMEX_FR_REG_DEFINE (5);
LUMEX_FR_REG_DEFINE (6);
LUMEX_FR_REG_DEFINE (7);
LUMEX_FR_REG_DEFINE (8);
LUMEX_FR_REG_DEFINE (9);
LUMEX_FR_REG_DEFINE (10);
LUMEX_FR_REG_DEFINE (11);
LUMEX_FR_REG_DEFINE (12);
LUMEX_FR_REG_DEFINE (13);
LUMEX_FR_REG_DEFINE (14);
LUMEX_FR_REG_DEFINE (15);
LUMEX_FR_REG_DEFINE (16);
LUMEX_FR_REG_DEFINE (17);
LUMEX_FR_REG_DEFINE (18);
LUMEX_FR_REG_DEFINE (19);
LUMEX_FR_REG_DEFINE (20);
LUMEX_FR_REG_DEFINE (21);
LUMEX_FR_REG_DEFINE (22);
LUMEX_FR_REG_DEFINE (23);
LUMEX_FR_REG_DEFINE (24);
LUMEX_FR_REG_DEFINE (25);
LUMEX_FR_REG_DEFINE (26);
LUMEX_FR_REG_DEFINE (27);
LUMEX_FR_REG_DEFINE (28);
LUMEX_FR_REG_DEFINE (29);
LUMEX_FR_REG_DEFINE (30);
LUMEX_FR_REG_DEFINE (31);
LUMEX_FR_REG_DEFINE (32);

#undef LUMEX_FR_REG_DEFINE
} // namespace lumex_field_reflection_tests

#endif // !LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_REGISTERED_HPP
