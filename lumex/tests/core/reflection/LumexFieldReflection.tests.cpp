// LumexFieldReflection.tests.cpp
//
// Built when LUMEX_WITH_FIELD_REFLECTION is ON and
// nlohmann_json::nlohmann_json exists. The same macro gates the body
// so clangd on the C++11 sibling flags does not report missing headers.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#if __cplusplus >= 201402L
#include <utility>
#endif

#include <gtest/gtest.h>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/reflection/LumexReflection"
#include "lumex/core/utility/assert/LumexAssert.hpp"

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
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::reflection::field_reflection;

namespace lumex_field_reflection_tests
{
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

template <std::size_t N> struct field_count_tag_t
{
  static std::size_t const value = N;
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

#if __cplusplus >= 202002L
inline std::string
expected_field_name (std::size_t i)
{
  return std::string ("f") + std::to_string (i);
}

template <std::size_t I>
void
check_name_index (char const *name)
{
  EXPECT_STREQ (name, expected_field_name (I).c_str ());
}

template <std::size_t I>
void
check_json_index (nlohmann::json const &j)
{
  typedef typename field_type_at<I>::type field_t;
  std::string const key = expected_field_name (I);
  ASSERT_TRUE (j.contains (key)) << key;
  expect_sample_eq (j.at (key).get<field_t> (), make_sample<field_t> (I));
}

template <std::size_t N, std::size_t... I>
void
check_names (std::array<char const *, N> const &names,
             std::index_sequence<I...>)
{
  int swallow[] = { 0, (check_name_index<I> (names[I]), 0)... };
  (void)swallow;
}

template <std::size_t... I>
void
check_json_fields (nlohmann::json const &j, std::index_sequence<I...>)
{
  int swallow[] = { 0, (check_json_index<I> (j), 0)... };
  (void)swallow;
}
#endif
} // namespace

using namespace lumex_field_reflection_tests;

TEST (LumexAggregateFieldsTest, GivenEmptyAggregate_WhenSized_ThenZero)
{
  EXPECT_EQ (tuple_size<Empty>::value, 0u);
}

TEST (LumexAggregateFieldsTest, GivenOneField_WhenSized_ThenOne)
{
  EXPECT_EQ (tuple_size<OneField>::value, 1u);
}

TEST (LumexAggregateFieldsTest, GivenPlainAggregate_WhenSized_ThenTwo)
{
  EXPECT_EQ (tuple_size<Plain>::value, 2u);
}

TEST (LumexAggregateFieldsTest, GivenPaddedAggregate_WhenSized_ThenThree)
{
  EXPECT_EQ (tuple_size<Padded>::value, 3u);
}

TEST (LumexAggregateFieldsTest, GivenEightFields_WhenSized_ThenEight)
{
  EXPECT_EQ (tuple_size<EightFields>::value, 8u);
}

#if __cplusplus >= 202002L
TEST (LumexAggregateFieldsTest,
      GivenEmptyAggregate_WhenNamed_ThenEmptyNameArray)
{
  std::array<char const *, 0> const names = names_as_array<Empty> ();
  EXPECT_TRUE (names.empty ());
}

TEST (LumexAggregateFieldsTest,
      GivenPlainAggregate_WhenNamed_ThenExactFieldNames)
{
  EXPECT_EQ (tuple_size<Plain>::value, 2u);
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  ASSERT_EQ (names.size (), 2u);
  EXPECT_STREQ (names[0], "id");
  EXPECT_STREQ (names[1], "name");
}

TEST (LumexAggregateFieldsTest,
      GivenPlainAggregate_WhenUnknownNameQueried_ThenNotFound)
{
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  bool found_id = false;
  bool found_unknown = false;
  for (char const *name : names)
    {
      if (std::string (name) == "id")
        found_id = true;
      if (std::string (name) == "not_a_field")
        found_unknown = true;
    }
  EXPECT_TRUE (found_id);
  EXPECT_FALSE (found_unknown);
}

TEST (LumexAggregateFieldsTest, NamesAsArray_WhenFound_ThenContainsId)
{
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  bool found_id = false;
  for (char const *name : names)
    {
      if (std::string (name) == "id")
        found_id = true;
    }
  EXPECT_TRUE (found_id);
}

TEST (LumexAggregateFieldsTest, NamesAsArray_WhenUnfound_ThenAbsent)
{
  std::array<char const *, 2> const names = names_as_array<Plain> ();
  bool found_unknown = false;
  for (char const *name : names)
    {
      if (std::string (name) == "not_a_field")
        found_unknown = true;
    }
  EXPECT_FALSE (found_unknown);
}
#endif

#if __cplusplus >= 201402L
TEST (LumexAggregateFieldsTest, GivenPlainAggregate_WhenGet_ThenFieldValues)
{
  EXPECT_EQ (tuple_size<Plain>::value, 2u);
  EXPECT_EQ (tuple_size<OneField>::value, 1u);
  Plain obj{ 7, "x" };
  EXPECT_EQ (get<0> (obj), 7);
  EXPECT_EQ (get<1> (obj), "x");
  get<0> (obj) = 8;
  EXPECT_EQ (obj.id, 8);
}

TEST (LumexAggregateFieldsTest,
      GivenConstPlainAggregate_WhenGet_ThenFieldValues)
{
  Plain const obj{ 3, "c" };
  EXPECT_EQ (get<0> (obj), 3);
  EXPECT_EQ (get<1> (obj), "c");
}

TEST (LumexAggregateFieldsTest, GivenPaddedAggregate_WhenGet_ThenAllFields)
{
  Padded obj{ 'z', 11, 2.5 };
  EXPECT_EQ (get<0> (obj), 'z');
  EXPECT_EQ (get<1> (obj), 11);
  EXPECT_DOUBLE_EQ (get<2> (obj), 2.5);
  get<1> (obj) = 12;
  EXPECT_EQ (obj.b, 12);
  EXPECT_EQ (obj.a, 'z');
  EXPECT_DOUBLE_EQ (obj.c, 2.5);
}

TEST (LumexAggregateFieldsTest, GivenEightFields_WhenGet_ThenEachIndex)
{
  EightFields obj{ 0, 1, 2, 3, 4, 5, 6, 7 };
  EXPECT_EQ (get<0> (obj), 0);
  EXPECT_EQ (get<3> (obj), 3);
  EXPECT_EQ (get<7> (obj), 7);
}
#endif // __cplusplus >= 201402L

#if __cplusplus >= 202002L
TEST (LumexFieldReflectionTest, GivenEmptyAggregate_WhenToJson_ThenEmptyObject)
{
  nlohmann::json const j = to_json (Empty{});
  EXPECT_TRUE (j.is_object ());
  EXPECT_TRUE (j.empty ());
}

TEST (LumexFieldReflectionTest, GivenOneField_WhenToJson_ThenSingleKey)
{
  nlohmann::json const j = to_json (OneField{ 5 });
  ASSERT_TRUE (j.contains ("only"));
  EXPECT_EQ (j["only"].get<int> (), 5);
  EXPECT_EQ (j.size (), 1u);
  EXPECT_FALSE (j.contains ("not_a_field"));
}

TEST (LumexFieldReflectionTest,
      GivenPlainAggregate_WhenToJson_ThenAllFieldsAreSerializedByName)
{
  Plain const obj{ 42, "hello" };
  nlohmann::json const j = to_json (obj);

  ASSERT_TRUE (j.contains ("id"));
  ASSERT_TRUE (j.contains ("name"));
  EXPECT_EQ (j["id"].get<int> (), 42);
  EXPECT_EQ (j["name"].get<std::string> (), "hello");
}

TEST (LumexFieldReflectionTest,
      GivenOptionalFieldWithValue_WhenToJson_ThenFieldIsWritten)
{
  WithOptionalPresent obj;
  obj.id = 1;
  obj.label = std::string ("present");
  nlohmann::json const j = to_json (obj);

  ASSERT_TRUE (j.contains ("label"));
  EXPECT_EQ (j["label"].get<std::string> (), "present");
}

TEST (LumexFieldReflectionTest,
      GivenOptionalFieldWithoutValue_WhenToJson_ThenFieldIsOmitted)
{
  WithOptionalAbsent obj;
  obj.id = 2;
  obj.label = nullopt;
  nlohmann::json const j = to_json (obj);

  EXPECT_TRUE (j.contains ("id"));
  EXPECT_FALSE (j.contains ("label"));
}

TEST (LumexFieldReflectionTest,
      GivenContainerField_WhenToJson_ThenSerializedAsJsonArray)
{
  WithVector const obj{ { 1, 2, 3 } };
  nlohmann::json const j = to_json (obj);

  ASSERT_TRUE (j.contains ("values"));
  EXPECT_TRUE (j["values"].is_array ());
  EXPECT_EQ (j["values"].size (), 3u);
}

TEST (LumexFieldReflectionTest,
      GivenMixedOptionalFields_WhenToJson_ThenOnlyEngagedOptionalsAreWritten)
{
  MixedOptionals obj;
  obj.missing = nullopt;
  obj.always = 9;
  obj.present = 4;
  nlohmann::json const j = to_json (obj);

  EXPECT_FALSE (j.contains ("missing"));
  ASSERT_TRUE (j.contains ("always"));
  ASSERT_TRUE (j.contains ("present"));
  EXPECT_EQ (j["always"].get<int> (), 9);
  EXPECT_EQ (j["present"].get<int> (), 4);
}

TEST (LumexFieldReflectionTest,
      GivenScalarFields_WhenToJson_ThenTypesAndEmptyStringArePreserved)
{
  Scalars const obj{ true, 1.5, "" };
  nlohmann::json const j = to_json (obj);

  EXPECT_EQ (j["flag"].get<bool> (), true);
  EXPECT_DOUBLE_EQ (j["ratio"].get<double> (), 1.5);
  EXPECT_EQ (j["empty"].get<std::string> (), "");
}

TEST (LumexFieldReflectionTest,
      GivenUnderscoreFieldNames_WhenToJson_ThenKeysMatchIdentifiers)
{
  UnderscoreNames const obj{ 1, 2 };
  nlohmann::json const j = to_json (obj);
  ASSERT_TRUE (j.contains ("field_a"));
  ASSERT_TRUE (j.contains ("field_b"));
  EXPECT_EQ (j["field_a"].get<int> (), 1);
  EXPECT_EQ (j["field_b"].get<int> (), 2);
}

TEST (LumexFieldReflectionTest,
      GivenEightFields_WhenToJson_ThenEveryIndexIsPresent)
{
  EightFields const obj{ 0, 1, 2, 3, 4, 5, 6, 7 };
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j.size (), 8u);
  EXPECT_EQ (j["a0"].get<int> (), 0);
  EXPECT_EQ (j["a7"].get<int> (), 7);
}

TEST (LumexFieldReflectionTest, GivenPaddedAggregate_WhenToJson_ThenAllKeys)
{
  Padded const obj{ 'q', 4, 8.0 };
  nlohmann::json const j = to_json (obj);
  ASSERT_TRUE (j.contains ("a"));
  ASSERT_TRUE (j.contains ("b"));
  ASSERT_TRUE (j.contains ("c"));
  EXPECT_EQ (j["a"].get<char> (), 'q');
  EXPECT_EQ (j["b"].get<int> (), 4);
  EXPECT_DOUBLE_EQ (j["c"].get<double> (), 8.0);
}
#endif // __cplusplus >= 202002L

template <typename T> class LumexFieldArityTest : public ::testing::Test
{
};

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

TYPED_TEST_SUITE (LumexFieldArityTest, FieldArityTypes);

TYPED_TEST (LumexFieldArityTest, GivenArity_WhenTupleSize_ThenMatches)
{
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  EXPECT_EQ (tuple_size<agg_t>::value, tag_t::value);
}

#if __cplusplus >= 201402L
TYPED_TEST (LumexFieldArityTest, GivenArity_WhenGetEachIndex_ThenSampleValues)
{
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  agg_t obj{};
  fill_fields (obj, std::make_index_sequence<tag_t::value>{});
  check_fields (obj, std::make_index_sequence<tag_t::value>{});

  agg_t const frozen = obj;
  check_fields (frozen, std::make_index_sequence<tag_t::value>{});
}
#endif

#if __cplusplus >= 202002L
TYPED_TEST (LumexFieldArityTest, GivenArity_WhenNamed_ThenFIndexNames)
{
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  std::array<char const *, tag_t::value> const names
      = names_as_array<agg_t> ();
  ASSERT_EQ (names.size (), tag_t::value);
  check_names<tag_t::value> (names, std::make_index_sequence<tag_t::value>{});
}

TYPED_TEST (LumexFieldArityTest, GivenArity_WhenToJson_ThenEveryKey)
{
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  agg_t obj{};
  fill_fields (obj, std::make_index_sequence<tag_t::value>{});
  nlohmann::json const j = to_json (obj);
  EXPECT_EQ (j.size (), tag_t::value);
  check_json_fields (j, std::make_index_sequence<tag_t::value>{});
}
#endif

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
