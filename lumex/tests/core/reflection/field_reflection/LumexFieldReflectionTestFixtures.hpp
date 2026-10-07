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

// The aggregates and the arity machinery that the LumexFieldReflection test
// files of every standard share: field_count_tag_t<N> and fields_of<N> for
// aggregates of 1 to 32 fields, sample values per field type, the C++14
// get<I> helpers, and the typed fixture LumexFieldArityTest. The test files
// include it only when LUMEX_WITH_FIELD_REFLECTION is defined.

#ifndef LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_TEST_FIXTURES_HPP
#define LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_TEST_FIXTURES_HPP

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/reflection/LumexReflection"
#include "lumex/core/utility/assert/LumexAssert.hpp"

namespace lumex_field_reflection_tests
{
using namespace lumex::core::reflection::field_reflection;

struct Empty
{
};

struct OneField
{
  int only;
};

struct Plain
{
  int id;
  std::string name;
};

struct WithOptionalPresent
{
  int id;
  optional<std::string> label;
};

struct WithOptionalAbsent
{
  int id;
  optional<std::string> label;
};

struct WithVector
{
  std::vector<int> values;
};

struct MixedOptionals
{
  optional<int> missing;
  int always;
  optional<int> present;
};

struct Scalars
{
  bool flag;
  double ratio;
  std::string empty;
};

struct UnderscoreNames
{
  int field_a;
  int field_b;
};

struct EightFields
{
  int a0;
  int a1;
  int a2;
  int a3;
  int a4;
  int a5;
  int a6;
  int a7;
};

struct Padded
{
  char a;
  int b;
  double c;
};

// Type cycle for arity 1..32: int, char, double, bool, unsigned, float,
// short, long long. Field names stay f0..f31 so names_as_array / to_json
// can be checked against "f" + index.
#define LUMEX_FR_DECL_1 int f0
#define LUMEX_FR_DECL_2                                                       \
  LUMEX_FR_DECL_1;                                                            \
  char f1
#define LUMEX_FR_DECL_3                                                       \
  LUMEX_FR_DECL_2;                                                            \
  double f2
#define LUMEX_FR_DECL_4                                                       \
  LUMEX_FR_DECL_3;                                                            \
  bool f3
#define LUMEX_FR_DECL_5                                                       \
  LUMEX_FR_DECL_4;                                                            \
  unsigned f4
#define LUMEX_FR_DECL_6                                                       \
  LUMEX_FR_DECL_5;                                                            \
  float f5
#define LUMEX_FR_DECL_7                                                       \
  LUMEX_FR_DECL_6;                                                            \
  short f6
#define LUMEX_FR_DECL_8                                                       \
  LUMEX_FR_DECL_7;                                                            \
  long long f7
#define LUMEX_FR_DECL_9                                                       \
  LUMEX_FR_DECL_8;                                                            \
  int f8
#define LUMEX_FR_DECL_10                                                      \
  LUMEX_FR_DECL_9;                                                            \
  char f9
#define LUMEX_FR_DECL_11                                                      \
  LUMEX_FR_DECL_10;                                                           \
  double f10
#define LUMEX_FR_DECL_12                                                      \
  LUMEX_FR_DECL_11;                                                           \
  bool f11
#define LUMEX_FR_DECL_13                                                      \
  LUMEX_FR_DECL_12;                                                           \
  unsigned f12
#define LUMEX_FR_DECL_14                                                      \
  LUMEX_FR_DECL_13;                                                           \
  float f13
#define LUMEX_FR_DECL_15                                                      \
  LUMEX_FR_DECL_14;                                                           \
  short f14
#define LUMEX_FR_DECL_16                                                      \
  LUMEX_FR_DECL_15;                                                           \
  long long f15
#define LUMEX_FR_DECL_17                                                      \
  LUMEX_FR_DECL_16;                                                           \
  int f16
#define LUMEX_FR_DECL_18                                                      \
  LUMEX_FR_DECL_17;                                                           \
  char f17
#define LUMEX_FR_DECL_19                                                      \
  LUMEX_FR_DECL_18;                                                           \
  double f18
#define LUMEX_FR_DECL_20                                                      \
  LUMEX_FR_DECL_19;                                                           \
  bool f19
#define LUMEX_FR_DECL_21                                                      \
  LUMEX_FR_DECL_20;                                                           \
  unsigned f20
#define LUMEX_FR_DECL_22                                                      \
  LUMEX_FR_DECL_21;                                                           \
  float f21
#define LUMEX_FR_DECL_23                                                      \
  LUMEX_FR_DECL_22;                                                           \
  short f22
#define LUMEX_FR_DECL_24                                                      \
  LUMEX_FR_DECL_23;                                                           \
  long long f23
#define LUMEX_FR_DECL_25                                                      \
  LUMEX_FR_DECL_24;                                                           \
  int f24
#define LUMEX_FR_DECL_26                                                      \
  LUMEX_FR_DECL_25;                                                           \
  char f25
#define LUMEX_FR_DECL_27                                                      \
  LUMEX_FR_DECL_26;                                                           \
  double f26
#define LUMEX_FR_DECL_28                                                      \
  LUMEX_FR_DECL_27;                                                           \
  bool f27
#define LUMEX_FR_DECL_29                                                      \
  LUMEX_FR_DECL_28;                                                           \
  unsigned f28
#define LUMEX_FR_DECL_30                                                      \
  LUMEX_FR_DECL_29;                                                           \
  float f29
#define LUMEX_FR_DECL_31                                                      \
  LUMEX_FR_DECL_30;                                                           \
  short f30
#define LUMEX_FR_DECL_32                                                      \
  LUMEX_FR_DECL_31;                                                           \
  long long f31

template <std::size_t N> struct fields_of;

#define LUMEX_FR_DEFINE(N)                                                    \
  struct fields_##N##_t                                                       \
  {                                                                           \
    LUMEX_FR_DECL_##N;                                                        \
  };                                                                          \
  template <> struct fields_of<N>                                             \
  {                                                                           \
    typedef fields_##N##_t type;                                              \
  }

LUMEX_FR_DEFINE (1);
LUMEX_FR_DEFINE (2);
LUMEX_FR_DEFINE (3);
LUMEX_FR_DEFINE (4);
LUMEX_FR_DEFINE (5);
LUMEX_FR_DEFINE (6);
LUMEX_FR_DEFINE (7);
LUMEX_FR_DEFINE (8);
LUMEX_FR_DEFINE (9);
LUMEX_FR_DEFINE (10);
LUMEX_FR_DEFINE (11);
LUMEX_FR_DEFINE (12);
LUMEX_FR_DEFINE (13);
LUMEX_FR_DEFINE (14);
LUMEX_FR_DEFINE (15);
LUMEX_FR_DEFINE (16);
LUMEX_FR_DEFINE (17);
LUMEX_FR_DEFINE (18);
LUMEX_FR_DEFINE (19);
LUMEX_FR_DEFINE (20);
LUMEX_FR_DEFINE (21);
LUMEX_FR_DEFINE (22);
LUMEX_FR_DEFINE (23);
LUMEX_FR_DEFINE (24);
LUMEX_FR_DEFINE (25);
LUMEX_FR_DEFINE (26);
LUMEX_FR_DEFINE (27);
LUMEX_FR_DEFINE (28);
LUMEX_FR_DEFINE (29);
LUMEX_FR_DEFINE (30);
LUMEX_FR_DEFINE (31);
LUMEX_FR_DEFINE (32);

#undef LUMEX_FR_DEFINE

// std::integral_constant supplies the definition of `value`, which
// EXPECT_EQ odr-uses; a plain in-class `static const` member would not
// link without optimization in any standard.
template <std::size_t N>
struct field_count_tag_t : std::integral_constant<std::size_t, N>
{
};

template <std::size_t I> struct field_type_sel;

template <> struct field_type_sel<0>
{
  typedef int type;
};

template <> struct field_type_sel<1>
{
  typedef char type;
};

template <> struct field_type_sel<2>
{
  typedef double type;
};

template <> struct field_type_sel<3>
{
  typedef bool type;
};

template <> struct field_type_sel<4>
{
  typedef unsigned type;
};

template <> struct field_type_sel<5>
{
  typedef float type;
};

template <> struct field_type_sel<6>
{
  typedef short type;
};

template <> struct field_type_sel<7>
{
  typedef long long type;
};

template <std::size_t I> struct field_type_at
{
  typedef typename field_type_sel<I % 8>::type type;
};

template <typename T>
T
make_sample (std::size_t)
{
  LUMEX_STATIC_ASSERT_MSG (sizeof (T) == 0,
                           "missing make_sample specialization");
  return T ();
}

template <>
inline int
make_sample<int> (std::size_t i)
{
  return static_cast<int> (100 + i);
}

template <>
inline char
make_sample<char> (std::size_t i)
{
  return static_cast<char> ('A' + static_cast<int> (i));
}

template <>
inline double
make_sample<double> (std::size_t i)
{
  return 1.5 + static_cast<double> (i);
}

template <>
inline bool
make_sample<bool> (std::size_t i)
{
  return (i % 2u) != 0u;
}

template <>
inline unsigned
make_sample<unsigned> (std::size_t i)
{
  return 200u + static_cast<unsigned> (i);
}

template <>
inline float
make_sample<float> (std::size_t i)
{
  return 2.5f + static_cast<float> (i);
}

template <>
inline short
make_sample<short> (std::size_t i)
{
  return static_cast<short> (300 + static_cast<int> (i));
}

template <>
inline long long
make_sample<long long> (std::size_t i)
{
  return 400LL + static_cast<long long> (i);
}

inline void
expect_sample_eq (float actual, float expected)
{
  EXPECT_FLOAT_EQ (actual, expected);
}

inline void
expect_sample_eq (double actual, double expected)
{
  EXPECT_DOUBLE_EQ (actual, expected);
}

template <typename T>
void
expect_sample_eq (T actual, T expected)
{
  EXPECT_EQ (actual, expected);
}

// get<I> exists from C++14 (the C++14 and C++20 files use these helpers).
#if __cplusplus >= 201402L
template <std::size_t I, typename Agg>
void
check_get_index (Agg const &obj)
{
  typedef typename field_type_at<I>::type field_t;
  typedef typename std::remove_cv<
      typename std::remove_reference<decltype (get<I> (obj))>::type>::type
      got_t;
  LUMEX_STATIC_ASSERT_MSG (std::is_same<got_t, field_t>::value,
                           "get<I> type must match the arity type cycle");
  field_t const actual = get<I> (obj);
  expect_sample_eq (actual, make_sample<field_t> (I));
}

template <typename Agg, std::size_t... I>
void
fill_fields (Agg &obj, std::index_sequence<I...>)
{
  int swallow[]
      = { 0, (get<I> (obj) = make_sample<typename field_type_at<I>::type> (I),
              0)... };
  (void)swallow;
}

template <typename Agg, std::size_t... I>
void
check_fields (Agg const &obj, std::index_sequence<I...>)
{
  int swallow[] = { 0, (check_get_index<I> (obj), 0)... };
  (void)swallow;
}
#endif

// Types<> must be a typedef. Passing
// ::testing::Types<field_count_tag_t<1>, ...> as a macro argument
// splits on the commas.
typedef ::testing::Types<
    field_count_tag_t<1>, field_count_tag_t<2>, field_count_tag_t<3>,
    field_count_tag_t<4>, field_count_tag_t<5>, field_count_tag_t<6>,
    field_count_tag_t<7>, field_count_tag_t<8>, field_count_tag_t<9>,
    field_count_tag_t<10>, field_count_tag_t<11>, field_count_tag_t<12>,
    field_count_tag_t<13>, field_count_tag_t<14>, field_count_tag_t<15>,
    field_count_tag_t<16>, field_count_tag_t<17>, field_count_tag_t<18>,
    field_count_tag_t<19>, field_count_tag_t<20>, field_count_tag_t<21>,
    field_count_tag_t<22>, field_count_tag_t<23>, field_count_tag_t<24>,
    field_count_tag_t<25>, field_count_tag_t<26>, field_count_tag_t<27>,
    field_count_tag_t<28>, field_count_tag_t<29>, field_count_tag_t<30>,
    field_count_tag_t<31>, field_count_tag_t<32>>
    FieldArityTypes;
} // namespace lumex_field_reflection_tests

template <typename T> class LumexFieldArityTest : public ::testing::Test
{
};

TYPED_TEST_SUITE (LumexFieldArityTest,
                  lumex_field_reflection_tests::FieldArityTypes);

#endif // !LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_TEST_FIXTURES_HPP
