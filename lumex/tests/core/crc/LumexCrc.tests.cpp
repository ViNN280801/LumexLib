#include <chrono>
#include <cstdint>
#include <iterator>
#include <random>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if __cplusplus >= 202002L
#include <span>
#endif

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#include "lumex/tests/core/crc/LumexCrcTestHelpers.hpp"
#include "lumex/tests/support/LumexPerfSkip.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#ifndef LUMEX_CRC_VECTOR_COUNT
#define LUMEX_CRC_VECTOR_COUNT 10000

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

#endif

using namespace lumex::core::crc::catalog;
using namespace lumex::core::crc::parametric;

namespace
{
using byte = std::uint8_t;

char const kRevEngCheckMessage[] = "123456789";
std::size_t const kRevEngCheckSize = 9U;

template <typename Spec>
void
expect_catalog_check ()
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  typename Spec::ValueType const actual
      = CrcParametric<Spec>::calculate (data, kRevEngCheckSize);
  EXPECT_EQ (actual, Spec::kCatalogCheck);
}

template <std::size_t... I>
void
expect_all_catalog_checks (std::index_sequence<I...>)
{
  int const expand[]
      = { 0, (expect_catalog_check<
                  typename std::tuple_element<I, all_crc_specs_t>::type> (),
              0)... };
  (void)expand;
}

template <std::size_t I>
void
expect_named_engine_matches_spec ()
{
  using Spec = typename std::tuple_element<I, all_crc_specs_t>::type;
  using Engine = typename std::tuple_element<I, all_crc_algorithms_t>::type;
  static_assert (std::is_same<Engine, CrcParametric<Spec>>::value,
                 "named engine must be CrcParametric of the paired spec");
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (Engine::calculate (data, kRevEngCheckSize), Spec::kCatalogCheck);
}

template <std::size_t... I>
void
expect_all_named_engines (std::index_sequence<I...>)
{
  int const expand[] = { 0, (expect_named_engine_matches_spec<I> (), 0)... };
  (void)expand;
}

crc_params_t
params_from_maxim_dow ()
{
  crc_params_t params{};
  params.widthBits = crc8_maxim_dow_spec_t::kWidth;
  params.poly = crc8_maxim_dow_spec_t::kPoly;
  params.init = crc8_maxim_dow_spec_t::kInit;
  params.refIn = crc8_maxim_dow_spec_t::kRefIn;
  params.refOut = crc8_maxim_dow_spec_t::kRefOut;
  params.xorOut = crc8_maxim_dow_spec_t::kXorOut;
  return params;
}

std::uint32_t
first_width8_catalog_index ()
{
  std::uint32_t const count = GetCrcCatalogEntryCount ();
  for (std::uint32_t index = 0; index < count; ++index)
    {
      if (GetCrcCatalogBitWidth (index) == 8)
        return index;
    }
  return CrcCatalogLegacyIndex ();
}
} // namespace

class Crc8Test : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    empty_data = {};
    single_byte_data = { 0x42 };
    known_data_1 = { 0x01, 0x02, 0x03, 0x04 };
    known_data_2 = { 0xDE, 0xAD, 0xBE, 0xEF };
    all_zero_data = std::vector<byte> (10, 0x00);
    large_data = std::vector<byte> (1024 * 1024, 0x55);
  }

  std::vector<byte> empty_data;
  std::vector<byte> single_byte_data;
  std::vector<byte> known_data_1;
  std::vector<byte> known_data_2;
  std::vector<byte> all_zero_data;
  std::vector<byte> large_data;
};

TEST_F (Crc8Test, GivenEmptyVector_WhenCalculateCrc8_ThenReturnsZero)
{
  EXPECT_EQ (Crc8MaximDow::calculate (empty_data), 0);
}

TEST_F (Crc8Test, GivenNullptrAndZeroSize_WhenCalculateCrc8Raw_ThenReturnsZero)
{
  EXPECT_EQ (Crc8MaximDow::calculate (nullptr, 0), 0);
}

TEST_F (Crc8Test,
        GivenNullptrAndNonZeroSize_WhenCalculateCrc8Raw_ThenReturnsZero)
{
  EXPECT_EQ (Crc8MaximDow::calculate (nullptr, 10), 0);
}

TEST_F (Crc8Test,
        GivenSingleByteVector_WhenCalculateCrc8_ThenReturnsCorrectCrc)
{
  EXPECT_EQ (Crc8MaximDow::calculate (single_byte_data), 0xFA);
}

TEST_F (Crc8Test,
        GivenSingleByteRaw_WhenCalculateCrc8Raw_ThenReturnsCorrectCrc)
{
  EXPECT_EQ (Crc8MaximDow::calculate (single_byte_data.data (),
                                      single_byte_data.size ()),
             0xFA);
}

TEST_F (Crc8Test,
        GivenKnownData1Vector_WhenCalculateCrc8_ThenReturnsExpectedCrc)
{
  EXPECT_EQ (Crc8MaximDow::calculate (known_data_1), 0xF4);
}

TEST_F (Crc8Test,
        GivenKnownData1Raw_WhenCalculateCrc8Raw_ThenReturnsExpectedCrc)
{
  EXPECT_EQ (
      Crc8MaximDow::calculate (known_data_1.data (), known_data_1.size ()),
      0xF4);
}

TEST_F (Crc8Test,
        GivenKnownData2Vector_WhenCalculateCrc8_ThenReturnsExpectedCrc)
{
  EXPECT_EQ (Crc8MaximDow::calculate (known_data_2), 0x84);
}

TEST_F (Crc8Test,
        GivenKnownData2Raw_WhenCalculateCrc8Raw_ThenReturnsExpectedCrc)
{
  EXPECT_EQ (
      Crc8MaximDow::calculate (known_data_2.data (), known_data_2.size ()),
      0x84);
}

TEST_F (Crc8Test, GivenAllZeroData_WhenCalculateCrc8_ThenReturnsZero)
{
  EXPECT_EQ (Crc8MaximDow::calculate (all_zero_data), 0x00);
}

TEST_F (Crc8Test, GivenAllZeroDataRaw_WhenCalculateCrc8Raw_ThenReturnsZero)
{
  EXPECT_EQ (
      Crc8MaximDow::calculate (all_zero_data.data (), all_zero_data.size ()),
      0x00);
}

TEST_F (Crc8Test,
        GivenMaximumInputSize_WhenCalculateCrc8_ThenCompletesWithoutError)
{
  byte crc = 0;
  EXPECT_NO_THROW (crc = Crc8MaximDow::calculate (large_data));
  EXPECT_NE (crc, 0);
}

TEST_F (
    Crc8Test,
    GivenMaximumInputSizeRaw_WhenCalculateCrc8Raw_ThenCompletesWithoutError)
{
  byte crc = 0;
  EXPECT_NO_THROW (
      crc = Crc8MaximDow::calculate (large_data.data (), large_data.size ()));
  EXPECT_NE (crc, 0);
}

TEST_F (Crc8Test, ThreadSafety_MultipleConcurrentCalculationsVector)
{
  int const num_threads = 8;
  std::vector<byte> data_to_process
      = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
  byte const expected_crc = Crc8MaximDow::calculate (data_to_process);

  std::vector<std::thread> threads;
  std::vector<byte> results (static_cast<std::size_t> (num_threads));

  for (int i = 0; i < num_threads; ++i)
    threads.emplace_back ([&, i] () {
      results[static_cast<std::size_t> (i)]
          = Crc8MaximDow::calculate (data_to_process);
    });

  for (auto &t : threads)
    t.join ();

  for (int i = 0; i < num_threads; ++i)
    EXPECT_EQ (results[static_cast<std::size_t> (i)], expected_crc);
}

TEST_F (Crc8Test, ThreadSafety_MultipleConcurrentCalculationsRaw)
{
  int const num_threads = 8;
  std::vector<byte> data_to_process
      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02 };
  byte const expected_crc = Crc8MaximDow::calculate (data_to_process.data (),
                                                     data_to_process.size ());

  std::vector<std::thread> threads;
  std::vector<byte> results (static_cast<std::size_t> (num_threads));

  for (int i = 0; i < num_threads; ++i)
    threads.emplace_back ([&, i] () {
      results[static_cast<std::size_t> (i)] = Crc8MaximDow::calculate (
          data_to_process.data (), data_to_process.size ());
    });

  for (auto &t : threads)
    t.join ();

  for (int i = 0; i < num_threads; ++i)
    EXPECT_EQ (results[static_cast<std::size_t> (i)], expected_crc);
}

TEST_F (Crc8Test, Perf_LargeDataVectorEncoding)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const N = 100;
  byte sink = 0;
  auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < N; ++i)
    sink = Crc8MaximDow::calculate (large_data);
  auto const dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  int const maxExpectedDurationMs = 5000;
  EXPECT_LT (dur.count (), maxExpectedDurationMs);
  EXPECT_NE (sink, static_cast<byte> (0));
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

TEST_F (Crc8Test, Perf_LargeDataRawEncoding)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const N = 100;
  byte sink = 0;
  auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < N; ++i)
    sink = Crc8MaximDow::calculate (large_data.data (), large_data.size ());
  auto const dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  int const maxExpectedDurationMs = 5000;
  EXPECT_LT (dur.count (), maxExpectedDurationMs);
  EXPECT_NE (sink, static_cast<byte> (0));
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

class Crc8SingleByteParamTest : public ::testing::TestWithParam<int>
{
};

TEST_P (Crc8SingleByteParamTest, VectorMatchesRawPointer)
{
  int const p = GetParam ();
  ASSERT_GE (p, 0);
  ASSERT_LE (p, 255);
  byte const b = static_cast<byte> (static_cast<unsigned char> (p));
  std::vector<byte> v = { b };
  EXPECT_EQ (Crc8MaximDow::calculate (v),
             Crc8MaximDow::calculate (&b, static_cast<std::size_t> (1)));
}

INSTANTIATE_TEST_SUITE_P (AllByteValues_0_255, Crc8SingleByteParamTest,
                          ::testing::Range (0, 256));

#if __cplusplus >= 202002L
TEST (Crc8Span, MatchesVector)
{
  std::vector<byte> data = { 0x01, 0x02, 0x03, 0x04 };
  std::span<byte const> sp (data.data (), data.size ());
  EXPECT_EQ (Crc8MaximDow::calculate (sp), Crc8MaximDow::calculate (data));
}

TEST (Crc8Span, EmptySpan_ReturnsZero)
{
  std::vector<byte> empty;
  std::span<byte const> sp (empty.data (), static_cast<std::size_t> (0));
  EXPECT_EQ (Crc8MaximDow::calculate (sp), 0);
}
#endif

TEST (CrcCatalog, EntryCountMatchesAllCrcSpecsTuple)
{
  EXPECT_EQ (
      GetCrcCatalogEntryCount (),
      static_cast<std::uint32_t> (std::tuple_size<all_crc_specs_t>::value));
}

TEST (CrcCatalog, AllSpecsMatchRevEngCheckOf123456789)
{
  expect_all_catalog_checks (
      std::make_index_sequence<std::tuple_size<all_crc_specs_t>::value>{});
}

TEST (CrcCatalog, NamedEnginesCoverEverySpec)
{
  EXPECT_EQ (std::tuple_size<all_crc_algorithms_t>::value,
             std::tuple_size<all_crc_specs_t>::value);
  expect_all_named_engines (
      std::make_index_sequence<std::tuple_size<all_crc_specs_t>::value>{});
}

TEST (CrcCatalog, RepresentativeSpecsMatchPublishedCheckValues)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (
      CrcParametric<crc3_gsm_spec_t>::calculate (data, kRevEngCheckSize),
      crc3_gsm_spec_t::kCatalogCheck);
  EXPECT_EQ (
      CrcParametric<crc8_maxim_dow_spec_t>::calculate (data, kRevEngCheckSize),
      crc8_maxim_dow_spec_t::kCatalogCheck);
  EXPECT_EQ (
      CrcParametric<crc16_modbus_spec_t>::calculate (data, kRevEngCheckSize),
      crc16_modbus_spec_t::kCatalogCheck);
  EXPECT_EQ (
      CrcParametric<crc32_iso_hdlc_spec_t>::calculate (data, kRevEngCheckSize),
      crc32_iso_hdlc_spec_t::kCatalogCheck);
  EXPECT_EQ (
      CrcParametric<crc64_ecma182_spec_t>::calculate (data, kRevEngCheckSize),
      crc64_ecma182_spec_t::kCatalogCheck);
}

TEST (CrcCatalog, ComputeCrcCatalogIndexZeroMatchesCrc3Gsm)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (ComputeCrcCatalog (0, data, kRevEngCheckSize),
             static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
}

TEST (CrcCatalog, ComputeCrcCatalogLastIndexMatchesLastSpec)
{
  using LastSpec =
      typename std::tuple_element<std::tuple_size<all_crc_specs_t>::value - 1U,
                                  all_crc_specs_t>::type;
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  std::uint32_t const last
      = GetCrcCatalogEntryCount () - static_cast<std::uint32_t> (1);
  EXPECT_EQ (ComputeCrcCatalog (last, data, kRevEngCheckSize),
             static_cast<std::uint64_t> (LastSpec::kCatalogCheck));
}

TEST (CrcCatalog, ComputeCrcCatalogRejectsNullAndOutOfRange)
{
  byte sample = 0x42;
  EXPECT_EQ (ComputeCrcCatalog (0, nullptr, 1), 0U);
  EXPECT_EQ (ComputeCrcCatalog (0, &sample, 0), 0U);
  EXPECT_EQ (ComputeCrcCatalog (GetCrcCatalogEntryCount (), &sample, 1), 0U);
}

TEST (CrcCatalog, GetCrcCatalogBitWidth_WhenFound_ThenPositiveWidth)
{
  EXPECT_GT (GetCrcCatalogBitWidth (0), 0);
  EXPECT_EQ (GetCrcCatalogBitWidth (CrcCatalogLegacyIndex ()), 8);
}

TEST (CrcCatalog, GetCrcCatalogBitWidth_WhenUnfound_ThenMinusOne)
{
  EXPECT_EQ (GetCrcCatalogBitWidth (GetCrcCatalogEntryCount ()), -1);
  EXPECT_EQ (GetCrcCatalogBitWidth (GetCrcCatalogEntryCount () + 1U), -1);
}

TEST (CrcCatalog, ComputeCrcCatalogVectorMatchesPointerForm)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  std::vector<std::uint8_t> const payload (data, data + kRevEngCheckSize);
  EXPECT_EQ (ComputeCrcCatalog (0, payload),
             ComputeCrcCatalog (0, data, kRevEngCheckSize));
  std::vector<std::uint8_t> const empty;
  EXPECT_EQ (ComputeCrcCatalog (0, empty), 0U);
}

TEST (CrcCatalog, RevEngParamsMatchParametricForMaximDow)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  crc_params_t const params = params_from_maxim_dow ();
  EXPECT_EQ (
      ComputeCrcWithRevEngParams (params, data, kRevEngCheckSize),
      static_cast<std::uint64_t> (crc8_maxim_dow_spec_t::kCatalogCheck));
  EXPECT_EQ (ComputeCrcWithRevEngParams (params, data, kRevEngCheckSize),
             static_cast<std::uint64_t> (
                 CrcParametric<crc8_maxim_dow_spec_t>::calculate (
                     data, kRevEngCheckSize)));
}

TEST (CrcCatalog, RevEngParamsRejectInvalidWidth)
{
  crc_params_t params = params_from_maxim_dow ();
  params.widthBits = 0;
  EXPECT_FALSE (ValidateCrcRevEngParams (params));
  byte sample = 0x01;
  EXPECT_EQ (ComputeCrcWithRevEngParams (params, &sample, 1), 0U);
  params.widthBits = 65;
  EXPECT_FALSE (ValidateCrcRevEngParams (params));
}

class CrcTransportTest : public ::testing::Test
{
protected:
  void
  TearDown () override
  {
    SetTransportCrcDefault ();
  }
};

TEST_F (CrcTransportTest, DefaultModeUsesCrc8MaximDow)
{
  SetTransportCrcDefault ();
  EXPECT_EQ (GetTransportCrcMode (), TransportCrcMode::Default);
  EXPECT_EQ (GetTransportCrcCatalogIndex (), CrcCatalogLegacyIndex ());
  std::vector<byte> data = { 0x01, 0x02, 0x03, 0x04 };
  EXPECT_EQ (ComputeTransportChecksum (data.data (), data.size ()),
             Crc8MaximDow::calculate (data));
}

TEST_F (CrcTransportTest, CatalogModeUsesFirstWidth8Entry)
{
  std::uint32_t const index = first_width8_catalog_index ();
  ASSERT_NE (index, CrcCatalogLegacyIndex ());
  ASSERT_TRUE (SetTransportCrcCatalogIndex (index));
  EXPECT_EQ (GetTransportCrcMode (), TransportCrcMode::Catalog);
  EXPECT_EQ (GetTransportCrcCatalogIndex (), index);

  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (
      ComputeTransportChecksum (data, kRevEngCheckSize),
      static_cast<byte> (ComputeCrcCatalog (index, data, kRevEngCheckSize)));
}

TEST_F (CrcTransportTest, CustomModeUsesRevEngParams)
{
  crc_params_t const params = params_from_maxim_dow ();
  ASSERT_TRUE (SetTransportCrcRevEngParams (params));
  EXPECT_EQ (GetTransportCrcMode (), TransportCrcMode::Custom);
  EXPECT_EQ (GetTransportCrcCatalogIndex (),
             CrcTransportUsesCustomSpecSentinel ());

  crc_params_t stored{};
  ASSERT_TRUE (TryGetTransportCrcRevEngParams (stored));
  EXPECT_EQ (stored.widthBits, params.widthBits);
  EXPECT_EQ (stored.poly, params.poly);

  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (ComputeTransportChecksum (data, kRevEngCheckSize),
             crc8_maxim_dow_spec_t::kCatalogCheck);
}

TEST_F (CrcTransportTest, CatalogIndexRejectsNon8BitWidth)
{
  EXPECT_FALSE (SetTransportCrcCatalogIndex (0));
  EXPECT_EQ (GetTransportCrcMode (), TransportCrcMode::Default);
}

TEST_F (CrcTransportTest, CatalogIndex_WhenOutOfRange_ThenUnfoundFalse)
{
  EXPECT_FALSE (SetTransportCrcCatalogIndex (GetCrcCatalogEntryCount ()));
  EXPECT_FALSE (SetTransportCrcCatalogIndex (GetCrcCatalogEntryCount () + 1U));
  EXPECT_EQ (GetTransportCrcMode (), TransportCrcMode::Default);
}

TEST_F (CrcTransportTest, CustomRejectsNon8BitWidth)
{
  crc_params_t params{};
  params.widthBits = crc16_modbus_spec_t::kWidth;
  params.poly = crc16_modbus_spec_t::kPoly;
  params.init = crc16_modbus_spec_t::kInit;
  params.refIn = crc16_modbus_spec_t::kRefIn;
  params.refOut = crc16_modbus_spec_t::kRefOut;
  params.xorOut = crc16_modbus_spec_t::kXorOut;
  EXPECT_FALSE (SetTransportCrcRevEngParams (params));
  EXPECT_EQ (GetTransportCrcMode (), TransportCrcMode::Default);
}

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

  auto const raw = CrcParametric<Spec>::calculate (kMsg, sizeof (kMsg));
  auto const vec = CrcParametric<Spec>::calculate (
      std::vector<std::uint8_t> (std::begin (kMsg), std::end (kMsg)));

  EXPECT_EQ (raw, Spec::kCatalogCheck)
      << "raw ptr CRC of \"123456789\" != kCatalogCheck";
  EXPECT_EQ (vec, Spec::kCatalogCheck)
      << "vector CRC of \"123456789\" != kCatalogCheck";
  EXPECT_EQ (raw, vec) << "raw ptr and vector overloads disagree";

  EXPECT_EQ (CrcParametric<Spec>::calculate (nullptr, 0U), V{ 0 });
  EXPECT_EQ (CrcParametric<Spec>::calculate (nullptr, 42U), V{ 0 });
  EXPECT_EQ (CrcParametric<Spec>::calculate (std::vector<std::uint8_t>{}),
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
          = CrcParametric<Spec>::calculate (data.data (), data.size ());
      auto const actualVec = CrcParametric<Spec>::calculate (data);

      ASSERT_EQ (actualRaw, expected)
          << "raw ptr mismatch: case " << i << " len=" << data.size ();
      ASSERT_EQ (actualVec, expected)
          << "vector mismatch: case " << i << " len=" << data.size ();

#if __cplusplus >= 202002L
      auto const actualSpan = CrcParametric<Spec>::calculate (
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
          = CrcParametric<Spec>::calculate (data.data (), data.size ());
      auto const vec = CrcParametric<Spec>::calculate (data);
      EXPECT_EQ (raw, ref);
      EXPECT_EQ (vec, ref);
#if __cplusplus >= 202002L
      EXPECT_EQ (CrcParametric<Spec>::calculate (
                     std::span<std::uint8_t const>{ data }),
                 ref);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
    }
}
