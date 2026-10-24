// hid_selector_t: which HID interfaces a selector names (each field alone and
// together, the path, the selector with no field), find_hid_device and
// describe. The devices are synthetic (no real product IDs).

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/LumexHidSelectors.hpp"

namespace
{
namespace hid = lumex::applied::transport::hid;

std::uint16_t const KVENDOR = 0x1234;
std::uint16_t const KPRODUCT = 0x5678;

hid::hid_device_info_t
interface_of (std::string const &path, std::uint16_t vendor_id,
              std::uint16_t product_id, std::string const &serial,
              int interface_number, std::uint16_t usage_page,
              std::uint16_t usage)
{
  hid::hid_device_info_t info;
  info.path = path;
  info.vendor_id = vendor_id;
  info.product_id = product_id;
  info.serial_number = serial;
  info.interface_number = interface_number;
  info.usage_page = usage_page;
  info.usage = usage;
  return info;
}

hid::hid_device_info_t
the_device ()
{
  return interface_of ("path-a", KVENDOR, KPRODUCT, "Sn01", 1, 0xFF00, 0x0001);
}

hid::hid_selector_t
by_ids (std::uint16_t vendor_id, std::uint16_t product_id)
{
  hid::hid_selector_t selector;
  selector.vendor_id = vendor_id;
  selector.product_id = product_id;
  return selector;
}
} // namespace

TEST (LumexHidSelectorsTest,
      GivenASelectorByIds_WhenMatched_ThenBothIdsMustFit)
{
  EXPECT_TRUE (hid::matches (by_ids (KVENDOR, KPRODUCT), the_device ()));
  EXPECT_FALSE (hid::matches (by_ids (KVENDOR + 1, KPRODUCT), the_device ()));
  EXPECT_FALSE (hid::matches (by_ids (KVENDOR, KPRODUCT + 1), the_device ()));
}

TEST (LumexHidSelectorsTest,
      GivenAVendorOnlySelector_WhenMatched_ThenAnyProductFits)
{
  EXPECT_TRUE (hid::matches (by_ids (KVENDOR, 0), the_device ()));
  EXPECT_FALSE (hid::matches (by_ids (KVENDOR + 1, 0), the_device ()));
}

TEST (LumexHidSelectorsTest,
      GivenAProductOnlySelector_WhenMatched_ThenAnyVendorFits)
{
  EXPECT_TRUE (hid::matches (by_ids (0, KPRODUCT), the_device ()));
  EXPECT_FALSE (hid::matches (by_ids (0, KPRODUCT + 1), the_device ()));
}

TEST (LumexHidSelectorsTest,
      GivenASerialNumber_WhenMatched_ThenItIsComparedWithoutCase)
{
  hid::hid_selector_t selector = by_ids (KVENDOR, KPRODUCT);
  selector.serial_number = "sN01";
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  selector.serial_number = "Sn02";
  EXPECT_FALSE (hid::matches (selector, the_device ()));
  selector.serial_number = "Sn0";
  EXPECT_FALSE (hid::matches (selector, the_device ()));
}

TEST (LumexHidSelectorsTest, GivenASerialNumberOnly_WhenMatched_ThenAnyIdsFit)
{
  hid::hid_selector_t selector;
  selector.serial_number = "Sn01";
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  hid::hid_device_info_t other = the_device ();
  other.serial_number.clear ();
  EXPECT_FALSE (hid::matches (selector, other));
}

TEST (LumexHidSelectorsTest,
      GivenAnInterfaceNumber_WhenMatched_ThenOnlyThatInterfaceFits)
{
  hid::hid_selector_t selector = by_ids (KVENDOR, KPRODUCT);
  selector.interface_number = 1;
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  selector.interface_number = 0;
  EXPECT_FALSE (hid::matches (selector, the_device ()));
}

TEST (LumexHidSelectorsTest,
      GivenInterfaceZero_WhenTheDeviceHasAnUnknownInterface_ThenItDoesNotFit)
{
  hid::hid_selector_t selector = by_ids (KVENDOR, KPRODUCT);
  selector.interface_number = 0;
  hid::hid_device_info_t unknown = the_device ();
  unknown.interface_number = -1;
  EXPECT_FALSE (hid::matches (selector, unknown));
  selector.interface_number = -1;
  EXPECT_TRUE (hid::matches (selector, unknown));
}

TEST (LumexHidSelectorsTest,
      GivenAUsagePage_WhenMatched_ThenOnlyThatCollectionFits)
{
  hid::hid_selector_t selector = by_ids (KVENDOR, KPRODUCT);
  selector.usage_page = 0xFF00;
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  selector.usage_page = 0x0001;
  EXPECT_FALSE (hid::matches (selector, the_device ()));
}

TEST (LumexHidSelectorsTest, GivenAUsage_WhenMatched_ThenOnlyThatUsageFits)
{
  hid::hid_selector_t selector = by_ids (KVENDOR, KPRODUCT);
  selector.usage_page = 0xFF00;
  selector.usage = 0x0001;
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  selector.usage = 0x0002;
  EXPECT_FALSE (hid::matches (selector, the_device ()));
}

TEST (LumexHidSelectorsTest, GivenAPath_WhenMatched_ThenTheRestIsNotLookedAt)
{
  hid::hid_selector_t selector = by_ids (KVENDOR + 7, KPRODUCT + 7);
  selector.path = "path-a";
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  selector.path = "path-b";
  EXPECT_FALSE (hid::matches (selector, the_device ()));
  selector.path = "PATH-A";
  EXPECT_FALSE (hid::matches (selector, the_device ()));
}

TEST (LumexHidSelectorsTest,
      GivenASelectorWithNoField_WhenMatched_ThenNothingFits)
{
  EXPECT_FALSE (hid::matches (hid::hid_selector_t (), the_device ()));
  EXPECT_FALSE (
      hid::matches (hid::hid_selector_t (), hid::hid_device_info_t ()));
}

TEST (LumexHidSelectorsTest,
      GivenAUsageOnlySelector_WhenMatched_ThenItIsASelector)
{
  hid::hid_selector_t selector;
  selector.usage_page = 0xFF00;
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  selector.usage_page = 0;
  selector.usage = 1;
  EXPECT_TRUE (hid::matches (selector, the_device ()));
  selector.interface_number = 0;
  selector.usage = 0;
  EXPECT_FALSE (hid::matches (selector, the_device ()));
}

TEST (LumexHidSelectorsTest,
      GivenSeveralDevices_WhenFound_ThenTheFirstMatchPathIsReturned)
{
  std::vector<hid::hid_device_info_t> devices;
  devices.push_back (interface_of ("p0", 1, 2, "", 0, 0, 0));
  devices.push_back (
      interface_of ("p1", KVENDOR, KPRODUCT, "A", 0, 0xFF00, 1));
  devices.push_back (
      interface_of ("p2", KVENDOR, KPRODUCT, "A", 1, 0xFF00, 1));
  hid::hid_selector_t selector = by_ids (KVENDOR, KPRODUCT);
  EXPECT_EQ (hid::find_hid_device (selector, devices), "p1");
  selector.interface_number = 1;
  EXPECT_EQ (hid::find_hid_device (selector, devices), "p2");
  selector.interface_number = 5;
  EXPECT_EQ (hid::find_hid_device (selector, devices), "");
  EXPECT_EQ (hid::find_hid_device (by_ids (KVENDOR, KPRODUCT),
                                   std::vector<hid::hid_device_info_t> ()),
             "");
}

TEST (LumexHidSelectorsTest,
      GivenASelectorByPath_WhenFound_ThenItsPathIsReturnedAsItIs)
{
  hid::hid_selector_t selector;
  selector.path = "elsewhere";
  EXPECT_EQ (
      hid::find_hid_device (selector, std::vector<hid::hid_device_info_t> ()),
      "elsewhere");
}

TEST (LumexHidSelectorsTest,
      GivenSelectors_WhenDescribed_ThenTheFieldsSetAreNamed)
{
  hid::hid_selector_t selector = by_ids (0x16E0, 0x0001);
  EXPECT_EQ (hid::describe (selector), "HID VID 16E0 PID 0001");
  selector.serial_number = "S9";
  selector.interface_number = 2;
  selector.usage_page = 0xFF00;
  selector.usage = 3;
  EXPECT_EQ (hid::describe (selector),
             "HID VID 16E0 PID 0001 SN S9 IF 2 UP FF00 U 0003");
  selector.path = "node";
  EXPECT_EQ (hid::describe (selector), "node");
}
