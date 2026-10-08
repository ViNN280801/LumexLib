// Typed tests of the parametric CRC engine (lumex/core/crc/parametric) over
// every spec of the catalog: the catalog check value, random vectors against
// the reference CRC and the manual cases. They run from C++11 (the table is
// built on first use there and at compile time from C++14); every suite of
// this directory (C++11, C++14, C++17, C++20) runs them.
// The span checks inside RandomVectors and ManualCases run in the C++20
// suite; LumexCrcParametric.cxx20.tests.cpp adds the std::span tests of
// Crc8MaximDow.

#include <cstdint>
#include <iterator>
#include <random>
#include <tuple>
#include <vector>

#if __cplusplus >= 202002L
#include <span>
#endif

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#include "lumex/tests/core/crc/parametric/LumexCrcTestHelpers.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#ifndef LUMEX_CRC_VECTOR_COUNT
#define LUMEX_CRC_VECTOR_COUNT 10000
#endif

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

using namespace lumex::core::crc::catalog;
using namespace lumex::core::crc::parametric;

template <typename Tuple> struct TupleToTestingTypes;

template <typename... Ts> struct TupleToTestingTypes<std::tuple<Ts...>>
{
  using type = ::testing::Types<Ts...>;
};

template <typename SpecT> class CrcParametricTest : public ::testing::Test
{
};

using CrcSpecTypes = typename TupleToTestingTypes<all_crc_specs_t>::type;

TYPED_TEST_SUITE (CrcParametricTest, CrcSpecTypes);

TYPED_TEST (CrcParametricTest, CatalogCheck)
{
  using Spec = TypeParam;
  using V = typename Spec::ValueType;

  static LUMEX_CONSTEXPR std::uint8_t kMsg[]
      = { 0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U, 0x37U, 0x38U, 0x39U };

  auto const raw = crc_parametric<Spec>::calculate (kMsg, sizeof (kMsg));
  auto const vec = crc_parametric<Spec>::calculate (
      std::vector<std::uint8_t> (std::begin (kMsg), std::end (kMsg)));

  EXPECT_EQ (raw, Spec::kCatalogCheck)
      << "raw ptr CRC of \"123456789\" != kCatalogCheck";
  EXPECT_EQ (vec, Spec::kCatalogCheck)
      << "vector CRC of \"123456789\" != kCatalogCheck";
  EXPECT_EQ (raw, vec) << "raw ptr and vector overloads disagree";

  EXPECT_EQ (crc_parametric<Spec>::calculate (nullptr, 0U), V{ 0 });
  EXPECT_EQ (crc_parametric<Spec>::calculate (nullptr, 42U), V{ 0 });
  EXPECT_EQ (crc_parametric<Spec>::calculate (std::vector<std::uint8_t>{}),
             V{ 0 });
}

TYPED_TEST (CrcParametricTest, RandomVectors)
{
  using Spec = TypeParam;

  std::mt19937 rng{ 0xDEADBEEFU };
  std::uniform_int_distribution<int> lenDist{ 1, 256 };
  std::uniform_int_distribution<int> byteDist{ 0, 255 };

  for (int i = 0; i < LUMEX_CRC_VECTOR_COUNT; ++i)
    {
      std::vector<std::uint8_t> data (
          static_cast<std::size_t> (lenDist (rng)));
      for (auto &b : data)
        b = static_cast<std::uint8_t> (byteDist (rng));

      auto const expected = LumexCrcTestHelpers::reference_crc<Spec> (data);
      auto const actualRaw
          = crc_parametric<Spec>::calculate (data.data (), data.size ());
      auto const actualVec = crc_parametric<Spec>::calculate (data);

      ASSERT_EQ (actualRaw, expected)
          << "raw ptr mismatch: case " << i << " len=" << data.size ();
      ASSERT_EQ (actualVec, expected)
          << "vector mismatch: case " << i << " len=" << data.size ();

#if __cplusplus >= 202002L
      auto const actualSpan = crc_parametric<Spec>::calculate (
          std::span<std::uint8_t const>{ data });
      ASSERT_EQ (actualSpan, expected)
          << "span mismatch: case " << i << " len=" << data.size ();
#endif
    }
}

TYPED_TEST (CrcParametricTest, ManualCases)
{
  using Spec = TypeParam;

  for (auto const &data : LumexCrcTestHelpers::manual_crc_cases ())
    {
      auto const ref = LumexCrcTestHelpers::reference_crc<Spec> (data);
      auto const raw
          = crc_parametric<Spec>::calculate (data.data (), data.size ());
      auto const vec = crc_parametric<Spec>::calculate (data);
      EXPECT_EQ (raw, ref);
      EXPECT_EQ (vec, ref);
#if __cplusplus >= 202002L
      EXPECT_EQ (crc_parametric<Spec>::calculate (
                     std::span<std::uint8_t const>{ data }),
                 ref);
#endif
    }
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
