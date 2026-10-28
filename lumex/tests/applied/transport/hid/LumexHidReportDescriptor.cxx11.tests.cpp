// analyze_report_descriptor (lumex/applied/transport/hid): the reference
// cases (unnumbered, numbered, bit fields,
// 0x85 as a data byte, empty, truncated) and the sizes per report ID, Push
// and Pop, long items, four-byte items and the malformed descriptors. The
// descriptors are synthetic (vendor-defined usage page).

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFakeHidapi.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/hid/LumexHidReportDescriptor.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using lumex_fake_hidapi::bytes;
using tr::error::transport_errc;
using tr::hid::analyze_report_descriptor;
using tr::hid::report_layout_result_t;
using tr::hid::report_layout_t;

report_layout_result_t
analyze (bytes const &d)
{
  return analyze_report_descriptor (tr::const_bytes_t (d.data (), d.size ()));
}

void
expect_bad (bytes const &d)
{
  report_layout_result_t const result = analyze (d);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::bad_report_descriptor);
}
} // namespace

TEST (LumexHidReportDescriptorTest,
      GivenAnUnnumberedDescriptor_WhenAnalyzed_ThenNoReportIdsAndTheSizes)
{
  report_layout_result_t const result
      = analyze (lumex_fake_hidapi::unnumbered_descriptor (64, 64));
  ASSERT_TRUE (result.has_value ());
  report_layout_t const &layout = result.value ();
  EXPECT_FALSE (layout.numbered);
  ASSERT_EQ (layout.input_reports.size (), 1u);
  EXPECT_EQ (layout.input_reports[0].report_id, 0);
  EXPECT_EQ (layout.input_size (0), 64u);
  EXPECT_EQ (layout.output_size (0), 64u);
  EXPECT_TRUE (layout.has_output (0));
  EXPECT_EQ (layout.max_input_size (), 64u);
}

TEST (LumexHidReportDescriptorTest,
      GivenADescriptorWithAReportId_WhenAnalyzed_ThenNumbered)
{
  bytes const d = { 0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x01,
                    0x75, 0x08, 0x95, 0x20, 0x81, 0x02, 0x91, 0x02, 0xC0 };
  report_layout_result_t const result = analyze (d);
  ASSERT_TRUE (result.has_value ());
  EXPECT_TRUE (result.value ().numbered);
  EXPECT_EQ (result.value ().input_size (1), 32u);
  EXPECT_EQ (result.value ().output_size (1), 32u);
  EXPECT_EQ (result.value ().output_size (0), 0u);
}

TEST (LumexHidReportDescriptorTest,
      GivenBitFields_WhenAnalyzed_ThenTheSizeIsRoundedUpToBytes)
{
  bytes const d = { 0x75, 0x01, 0x95, 0x0A, 0x81, 0x02,   // 10 bits in
                    0x75, 0x04, 0x95, 0x01, 0x91, 0x02 }; // 4 bits out
  report_layout_result_t const result = analyze (d);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result.value ().input_size (0), 2u);
  EXPECT_EQ (result.value ().output_size (0), 1u);
}

TEST (LumexHidReportDescriptorTest,
      GivenA0x85DataByte_WhenAnalyzed_ThenItIsNoReportId)
{
  // 0x85 as the data byte of Logical Maximum is not a Report ID item.
  bytes const d = { 0x15, 0x00, 0x25, 0x85, 0x75, 0x08,
                    0x95, 0x08, 0x81, 0x02, 0x91, 0x02 };
  report_layout_result_t const result = analyze (d);
  ASSERT_TRUE (result.has_value ());
  EXPECT_FALSE (result.value ().numbered);
  EXPECT_EQ (result.value ().input_size (0), 8u);
}

TEST (LumexHidReportDescriptorTest,
      GivenAnEmptyDescriptor_WhenAnalyzed_ThenBadReportDescriptor)
{
  expect_bad (bytes ());
  report_layout_result_t const null_result
      = analyze_report_descriptor (tr::const_bytes_t ());
  EXPECT_FALSE (null_result.has_value ());
}

TEST (LumexHidReportDescriptorTest,
      GivenATruncatedItem_WhenAnalyzed_ThenBadReportDescriptor)
{
  // The last item announces one data byte and has none.
  expect_bad ({ 0x75, 0x08, 0x95 });
  // A four-byte item with three bytes.
  expect_bad ({ 0x97, 0x01, 0x00, 0x00 });
}

TEST (LumexHidReportDescriptorTest,
      GivenSeveralReportIds_WhenAnalyzed_ThenEachHasItsOwnSizes)
{
  report_layout_result_t const result
      = analyze (lumex_fake_hidapi::numbered_descriptor ());
  ASSERT_TRUE (result.has_value ());
  report_layout_t const &layout = result.value ();
  EXPECT_TRUE (layout.numbered);
  ASSERT_EQ (layout.input_reports.size (), 3u);
  ASSERT_EQ (layout.output_reports.size (), 2u);
  EXPECT_EQ (layout.input_size (1), 8u);
  EXPECT_EQ (layout.output_size (1), 8u);
  EXPECT_EQ (layout.input_size (2), 16u);
  EXPECT_EQ (layout.output_size (2), 4u);
  EXPECT_EQ (layout.input_size (3), 2u);
  EXPECT_FALSE (layout.has_output (3));
  EXPECT_FALSE (layout.has_output (0));
  EXPECT_EQ (layout.max_input_size (), 16u);
}

TEST (LumexHidReportDescriptorTest,
      GivenPushAndPop_WhenAnalyzed_ThenTheGlobalStateIsRestored)
{
  bytes const d = { 0x75, 0x08, 0x85, 0x01, // size 8, ID 1
                    0xA4,                   // Push
                    0x85, 0x02, 0x95, 0x03, // ID 2, count 3
                    0x81, 0x02,             // 3 bytes in report 2
                    0xB4,                   // Pop: ID 1, count 0
                    0x95, 0x05, 0x81, 0x02, // 5 bytes in report 1
                    0x91, 0x02 };           // 5 bytes out of report 1
  report_layout_result_t const result = analyze (d);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result.value ().input_size (2), 3u);
  EXPECT_EQ (result.value ().input_size (1), 5u);
  EXPECT_EQ (result.value ().output_size (1), 5u);
  EXPECT_FALSE (result.value ().has_output (2));
}

TEST (LumexHidReportDescriptorTest,
      GivenAPopWithoutPush_WhenAnalyzed_ThenBadReportDescriptor)
{
  expect_bad ({ 0x75, 0x08, 0xB4, 0x95, 0x01, 0x81, 0x02 });
}

TEST (LumexHidReportDescriptorTest,
      GivenReportIdZero_WhenAnalyzed_ThenBadReportDescriptor)
{
  expect_bad ({ 0x85, 0x00, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02 });
}

TEST (LumexHidReportDescriptorTest,
      GivenAReportBeforeTheFirstReportId_WhenAnalyzed_ThenBadReportDescriptor)
{
  // The first input has no ID on the wire, the second has one.
  expect_bad ({ 0x75, 0x08, 0x95, 0x01, 0x81, 0x02, 0x85, 0x01, 0x81, 0x02 });
}

TEST (LumexHidReportDescriptorTest,
      GivenALongItem_WhenAnalyzed_ThenItIsSkippedWhole)
{
  // [0xFE][2 data bytes][long tag][data: 0x85 0x01, no Report ID].
  bytes const d
      = { 0xFE, 0x02, 0xF0, 0x85, 0x01, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02 };
  report_layout_result_t const result = analyze (d);
  ASSERT_TRUE (result.has_value ());
  EXPECT_FALSE (result.value ().numbered);
  EXPECT_EQ (result.value ().input_size (0), 4u);
  // A long item longer than the bytes left.
  expect_bad ({ 0x75, 0x08, 0xFE, 0x05, 0xF0, 0x00 });
  expect_bad ({ 0xFE, 0x00 });
}

TEST (LumexHidReportDescriptorTest,
      GivenTwoAndFourByteItems_WhenAnalyzed_ThenTheValuesAreLittleEndian)
{
  bytes const d = { 0x75, 0x08,                   // Report Size (8)
                    0x96, 0x00, 0x01,             // Report Count (256)
                    0x81, 0x02,                   // 256 bytes in
                    0x97, 0x02, 0x00, 0x00, 0x00, // Report Count (2), 4 bytes
                    0x91, 0x02 };                 // 2 bytes out
  report_layout_result_t const result = analyze (d);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result.value ().input_size (0), 256u);
  EXPECT_EQ (result.value ().output_size (0), 2u);
}

TEST (LumexHidReportDescriptorTest,
      GivenFeatureReports_WhenAnalyzed_ThenTheyCountAsNeitherInputNorOutput)
{
  bytes const d = { 0x75, 0x08, 0x95, 0x02, 0x81, 0x02, // 2 bytes in
                    0x95, 0x10, 0xB1, 0x02 };           // 16 bytes feature
  report_layout_result_t const result = analyze (d);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result.value ().input_size (0), 2u);
  EXPECT_TRUE (result.value ().output_reports.empty ());
}

TEST (LumexHidReportDescriptorTest,
      GivenAReportAbove64KiB_WhenAnalyzed_ThenBadReportDescriptor)
{
  // 8 bits x 65536 is the largest report; one bit more is refused.
  bytes const largest
      = { 0x75, 0x08, 0x97, 0x00, 0x00, 0x01, 0x00, 0x81, 0x02 };
  ASSERT_TRUE (analyze (largest).has_value ());
  EXPECT_EQ (analyze (largest).value ().input_size (0), 65536u);
  bytes too_large = largest;
  too_large.push_back (0x75);
  too_large.push_back (0x01);
  too_large.push_back (0x95);
  too_large.push_back (0x01);
  too_large.push_back (0x81);
  too_large.push_back (0x02);
  expect_bad (too_large);
}
