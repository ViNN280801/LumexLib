// CRC tests that compile from C++11 (lumex/core/crc/catalog): calculate_crc8,
// the runtime catalog and the transport mode; every suite of this directory
// (C++11, C++14, C++17, C++20) runs them. The library is compiled as C++11 and
// these tests prove that a consumer of any standard links it.
// LumexCrcCatalog.cxx17.tests.cpp adds the std::string_view overloads of the
// catalog.

#include <chrono>
#include <cstdint>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"

#include "lumex/tests/support/LumexPerfSkip.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
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
      = crc_parametric<Spec>::calculate (data, kRevEngCheckSize);
  EXPECT_EQ (actual, Spec::kCatalogCheck);
}

// C++11 has no std::index_sequence (C++14): a list of the numbers 0 .. N - 1.
template <std::size_t... I> struct index_list
{
};

template <std::size_t N, std::size_t... I>
struct make_index_list : make_index_list<N - 1, N - 1, I...>
{
};

template <std::size_t... I> struct make_index_list<0, I...>
{
  typedef index_list<I...> type;
};

template <std::size_t... I>
void
expect_all_catalog_checks (index_list<I...>)
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
  static_assert (std::is_same<Engine, crc_parametric<Spec>>::value,
                 "named engine must be CrcParametric of the paired spec");
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (Engine::calculate (data, kRevEngCheckSize), Spec::kCatalogCheck);
}

template <std::size_t... I>
void
expect_all_named_engines (index_list<I...>)
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
  std::uint32_t const count = get_crc_catalog_entry_count ();
  for (std::uint32_t index = 0; index < count; ++index)
    {
      if (get_crc_catalog_bit_width (index) == 8)
        return index;
    }
  return crc_catalog_legacy_index ();
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
    threads.emplace_back (
        [&, i] ()
          {
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
    threads.emplace_back (
        [&, i] ()
          {
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

TEST (CrcCatalog, EntryCountMatchesAllCrcSpecsTuple)
{
  EXPECT_EQ (
      get_crc_catalog_entry_count (),
      static_cast<std::uint32_t> (std::tuple_size<all_crc_specs_t>::value));
}

TEST (CrcCatalog, AllSpecsMatchRevEngCheckOf123456789)
{
  expect_all_catalog_checks (
      make_index_list<std::tuple_size<all_crc_specs_t>::value>::type ());
}

TEST (CrcCatalog, NamedEnginesCoverEverySpec)
{
  EXPECT_EQ (std::tuple_size<all_crc_algorithms_t>::value,
             std::tuple_size<all_crc_specs_t>::value);
  expect_all_named_engines (
      make_index_list<std::tuple_size<all_crc_specs_t>::value>::type ());
}

TEST (CrcCatalog, RepresentativeSpecsMatchPublishedCheckValues)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (
      crc_parametric<crc3_gsm_spec_t>::calculate (data, kRevEngCheckSize),
      crc3_gsm_spec_t::kCatalogCheck);
  EXPECT_EQ (crc_parametric<crc8_maxim_dow_spec_t>::calculate (
                 data, kRevEngCheckSize),
             crc8_maxim_dow_spec_t::kCatalogCheck);
  EXPECT_EQ (
      crc_parametric<crc16_modbus_spec_t>::calculate (data, kRevEngCheckSize),
      crc16_modbus_spec_t::kCatalogCheck);
  EXPECT_EQ (crc_parametric<crc32_iso_hdlc_spec_t>::calculate (
                 data, kRevEngCheckSize),
             crc32_iso_hdlc_spec_t::kCatalogCheck);
  EXPECT_EQ (
      crc_parametric<crc64_ecma182_spec_t>::calculate (data, kRevEngCheckSize),
      crc64_ecma182_spec_t::kCatalogCheck);
}

TEST (CrcCatalog, SpecConstantsBoundToReferencesLinkBeforeCxx17)
{
  // Binding a reference odr-uses the in-class static members; before C++17
  // that needs their namespace-scope definitions in LumexCrcCatalog.cpp.
  int const &width = crc16_modbus_spec_t::kWidth;
  std::uint64_t const &poly = crc16_modbus_spec_t::kPoly;
  bool const &reflect_in = crc16_modbus_spec_t::kRefIn;
  std::uint16_t const &check = crc16_modbus_spec_t::kCatalogCheck;
  std::uint8_t const &small_check = crc3_gsm_spec_t::kCatalogCheck;
  EXPECT_EQ (width, 16);
  EXPECT_EQ (poly, 0x8005U);
  EXPECT_TRUE (reflect_in);
  EXPECT_EQ (check, 0x4B37U);
  EXPECT_EQ (small_check, 0x4U);
}

TEST (CrcCatalog, ComputeCrcCatalogIndexZeroMatchesCrc3Gsm)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (compute_crc_catalog (0, data, kRevEngCheckSize),
             static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
}

TEST (CrcCatalog, ComputeCrcCatalogLastIndexMatchesLastSpec)
{
  using LastSpec =
      typename std::tuple_element<std::tuple_size<all_crc_specs_t>::value - 1U,
                                  all_crc_specs_t>::type;
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  std::uint32_t const last
      = get_crc_catalog_entry_count () - static_cast<std::uint32_t> (1);
  EXPECT_EQ (compute_crc_catalog (last, data, kRevEngCheckSize),
             static_cast<std::uint64_t> (LastSpec::kCatalogCheck));
}

TEST (CrcCatalog, ComputeCrcCatalogRejectsNullAndOutOfRange)
{
  byte sample = 0x42;
  EXPECT_EQ (compute_crc_catalog (0, nullptr, 1), 0U);
  EXPECT_EQ (compute_crc_catalog (0, &sample, 0), 0U);
  EXPECT_EQ (compute_crc_catalog (get_crc_catalog_entry_count (), &sample, 1),
             0U);
}

TEST (CrcCatalog, GetCrcCatalogBitWidth_WhenFound_ThenPositiveWidth)
{
  EXPECT_GT (get_crc_catalog_bit_width (0), 0);
  EXPECT_EQ (get_crc_catalog_bit_width (crc_catalog_legacy_index ()), 8);
}

TEST (CrcCatalog, GetCrcCatalogBitWidth_WhenUnfound_ThenMinusOne)
{
  EXPECT_EQ (get_crc_catalog_bit_width (get_crc_catalog_entry_count ()), -1);
  EXPECT_EQ (get_crc_catalog_bit_width (get_crc_catalog_entry_count () + 1U),
             -1);
}

TEST (CrcCatalog, ComputeCrcCatalogVectorMatchesPointerForm)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  std::vector<std::uint8_t> const payload (data, data + kRevEngCheckSize);
  EXPECT_EQ (compute_crc_catalog (0, payload),
             compute_crc_catalog (0, data, kRevEngCheckSize));
  std::vector<std::uint8_t> const empty;
  EXPECT_EQ (compute_crc_catalog (0, empty), 0U);
}

TEST (CrcCatalog, RevEngParamsMatchParametricForMaximDow)
{
  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  crc_params_t const params = params_from_maxim_dow ();
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (params, data, kRevEngCheckSize),
      static_cast<std::uint64_t> (crc8_maxim_dow_spec_t::kCatalogCheck));
  EXPECT_EQ (compute_crc_with_rev_eng_params (params, data, kRevEngCheckSize),
             static_cast<std::uint64_t> (
                 crc_parametric<crc8_maxim_dow_spec_t>::calculate (
                     data, kRevEngCheckSize)));
}

TEST (CrcCatalog, RevEngParamsRejectInvalidWidth)
{
  crc_params_t params = params_from_maxim_dow ();
  params.widthBits = 0;
  EXPECT_FALSE (validate_crc_rev_eng_params (params));
  byte sample = 0x01;
  EXPECT_EQ (compute_crc_with_rev_eng_params (params, &sample, 1), 0U);
  params.widthBits = 65;
  EXPECT_FALSE (validate_crc_rev_eng_params (params));
}

class CrcTransportTest : public ::testing::Test
{
protected:
  void
  TearDown () override
  {
    set_transport_crc_default ();
  }
};

TEST_F (CrcTransportTest, DefaultModeUsesCrc8MaximDow)
{
  set_transport_crc_default ();
  EXPECT_EQ (get_transport_crc_mode (), TransportCrcMode::Default);
  EXPECT_EQ (get_transport_crc_catalog_index (), crc_catalog_legacy_index ());
  std::vector<byte> data = { 0x01, 0x02, 0x03, 0x04 };
  EXPECT_EQ (compute_transport_checksum (data.data (), data.size ()),
             Crc8MaximDow::calculate (data));
}

TEST_F (CrcTransportTest, CatalogModeUsesFirstWidth8Entry)
{
  std::uint32_t const index = first_width8_catalog_index ();
  ASSERT_NE (index, crc_catalog_legacy_index ());
  ASSERT_TRUE (set_transport_crc_catalog_index (index));
  EXPECT_EQ (get_transport_crc_mode (), TransportCrcMode::Catalog);
  EXPECT_EQ (get_transport_crc_catalog_index (), index);

  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (
      compute_transport_checksum (data, kRevEngCheckSize),
      static_cast<byte> (compute_crc_catalog (index, data, kRevEngCheckSize)));
}

TEST_F (CrcTransportTest, CustomModeUsesRevEngParams)
{
  crc_params_t const params = params_from_maxim_dow ();
  ASSERT_TRUE (set_transport_crc_rev_eng_params (params));
  EXPECT_EQ (get_transport_crc_mode (), TransportCrcMode::Custom);
  EXPECT_EQ (get_transport_crc_catalog_index (),
             crc_transport_uses_custom_spec_sentinel ());

  crc_params_t stored{};
  ASSERT_TRUE (try_get_transport_crc_rev_eng_params (stored));
  EXPECT_EQ (stored.widthBits, params.widthBits);
  EXPECT_EQ (stored.poly, params.poly);

  byte const *data = reinterpret_cast<byte const *> (kRevEngCheckMessage);
  EXPECT_EQ (compute_transport_checksum (data, kRevEngCheckSize),
             crc8_maxim_dow_spec_t::kCatalogCheck);
}

TEST_F (CrcTransportTest, CatalogIndexRejectsNon8BitWidth)
{
  EXPECT_FALSE (set_transport_crc_catalog_index (0));
  EXPECT_EQ (get_transport_crc_mode (), TransportCrcMode::Default);
}

TEST_F (CrcTransportTest, CatalogIndex_WhenOutOfRange_ThenUnfoundFalse)
{
  EXPECT_FALSE (
      set_transport_crc_catalog_index (get_crc_catalog_entry_count ()));
  EXPECT_FALSE (
      set_transport_crc_catalog_index (get_crc_catalog_entry_count () + 1U));
  EXPECT_EQ (get_transport_crc_mode (), TransportCrcMode::Default);
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
  EXPECT_FALSE (set_transport_crc_rev_eng_params (params));
  EXPECT_EQ (get_transport_crc_mode (), TransportCrcMode::Default);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
