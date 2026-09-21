#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/serial/LumexSerialPort"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::applied::serial::port;
using namespace lumex::applied::serial::enumeration;
using namespace lumex::applied::serial::resolver;

// --- Platform-independent branches ---
// The "COM..." and "tty..." prefix checks, and the network-channel handling,
// all run before any platform-specific branch, so they behave identically on
// every OS.

TEST (LumexSerialPort, WindowsStyleComPortIsAlwaysRecognized)
{
  EXPECT_EQ (
      resolve_serial_port_path ("COM1", Constants::KSERIAL_PORT_CHANNEL_TYPE),
      R"(\\.\COM1)");
  EXPECT_EQ (
      resolve_serial_port_path ("COM12", Constants::KSERIAL_PORT_CHANNEL_TYPE),
      R"(\\.\COM12)");
}

TEST (LumexSerialPort, ShortComLikeNameIsNotTreatedAsComPort)
{
  // "length() >= 3" guard: "CO" is too short to be treated as a COM port.
  EXPECT_NE (
      resolve_serial_port_path ("CO", Constants::KSERIAL_PORT_CHANNEL_TYPE),
      R"(\\.\CO)");
}

TEST (LumexSerialPort, EmptyName_WhenUnfound_ThenNotMappedAsComOrTty)
{
  std::string const empty;
  std::string const resolved
      = resolve_serial_port_path (empty, Constants::KSERIAL_PORT_CHANNEL_TYPE);
#if defined(_WIN32)
  EXPECT_TRUE (resolved.empty ());
#elif defined(__APPLE__)
  EXPECT_EQ (resolved, "/dev/");
#else
  EXPECT_EQ (resolved, "/dev/serial/by-id/");
#endif
}

TEST (LumexSerialPort, TtyPrefixedNameIsAlwaysMappedUnderDev)
{
  EXPECT_EQ (resolve_serial_port_path ("ttyUSB0",
                                       Constants::KSERIAL_PORT_CHANNEL_TYPE),
             "/dev/ttyUSB0");
  EXPECT_EQ (resolve_serial_port_path ("ttyACM0",
                                       Constants::KSERIAL_PORT_CHANNEL_TYPE),
             "/dev/ttyACM0");
  EXPECT_EQ (
      resolve_serial_port_path ("ttyS0", Constants::KSERIAL_PORT_CHANNEL_TYPE),
      "/dev/ttyS0");
}

TEST (LumexSerialPort, NetworkChannelUsesIpAddressCallback)
{
  auto const cb = [] () { return std::string ("192.168.0.42"); };
  EXPECT_EQ (resolve_serial_port_path ("anything",
                                       Constants::KNET_TCP_CHANNEL_TYPE, cb),
             "192.168.0.42");
  EXPECT_EQ (resolve_serial_port_path ("anything",
                                       Constants::KNET_UDP_CHANNEL_TYPE, cb),
             "192.168.0.42");
}

TEST (LumexSerialPort, NetworkChannelWithoutCallbackReturnsNameAsIs)
{
  EXPECT_EQ (
      resolve_serial_port_path ("10.0.0.1", Constants::KNET_TCP_CHANNEL_TYPE),
      "10.0.0.1");
  EXPECT_EQ (
      resolve_serial_port_path ("10.0.0.1", Constants::KNET_UDP_CHANNEL_TYPE),
      "10.0.0.1");
}

TEST (LumexSerialPort, IsNoexcept)
{
  // noexcept(expr) checks the WHOLE call expression, including implicit
  // argument conversions - a string literal here would force a
  // (potentially-throwing) temporary std::string construction for the
  // by-const-ref parameter, making the expression non-noexcept regardless of
  // the callee's own specifier. Bind an existing std::string lvalue first so
  // only the call itself is measured.
  std::string const portName = "COM1";
  EXPECT_TRUE (noexcept (resolve_serial_port_path (
      portName, Constants::KSERIAL_PORT_CHANNEL_TYPE)));
}

TEST (LumexSerialPort, RepeatedCallsAreConsistent)
{
  // Purely functional, no shared state - calling it many times must always
  // agree with itself.
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ (resolve_serial_port_path ("COM3",
                                         Constants::KSERIAL_PORT_CHANNEL_TYPE),
               R"(\\.\COM3)");
}

// --- Platform-specific branches ---
// These mirror the #if guards inside LumexSerialPort.cpp exactly, so each test
// only compiles (and therefore only needs to hold) on the platform where its
// code path is actually reachable.

#if defined(_WIN32)

TEST (LumexSerialPort, WindowsFallbackReturnsNameUnchanged)
{
  // Neither "COM*" nor "tty*" nor a network channel: Windows returns the name
  // as-is.
  EXPECT_EQ (resolve_serial_port_path ("SomeOtherName",
                                       Constants::KSERIAL_PORT_CHANNEL_TYPE),
             "SomeOtherName");
}

#elif defined(__APPLE__)

TEST (LumexSerialPort, MacFallbackPrependsDev)
{
  EXPECT_EQ (resolve_serial_port_path ("cu.usbserial-1420",
                                       Constants::KSERIAL_PORT_CHANNEL_TYPE),
             "/dev/cu.usbserial-1420");
}

#else // Linux / other Unix

TEST (LumexSerialPort, LinuxByIdFallbackPrependsFullPath)
{
  EXPECT_EQ (resolve_serial_port_path ("usb-FTDI_FT232R-if00-port0",
                                       Constants::KSERIAL_PORT_CHANNEL_TYPE),
             "/dev/serial/by-id/usb-FTDI_FT232R-if00-port0");
}

TEST (LumexSerialPort, BareUsbMarkerFallsBackToTtyUsb0AndWarns)
{
  std::vector<std::string> warnings;
  auto const warn
      = [&warnings] (std::string const &msg) { warnings.push_back (msg); };

  EXPECT_EQ (resolve_serial_port_path (
                 "USB", Constants::KSERIAL_PORT_CHANNEL_TYPE, nullptr, warn),
             "/dev/ttyUSB0");
  ASSERT_EQ (warnings.size (), 1U);
  EXPECT_NE (warnings[0].find ("USB"), std::string::npos);
  EXPECT_NE (warnings[0].find ("/dev/ttyUSB0"), std::string::npos);
}

TEST (LumexSerialPort, NumberedUsbMarkerFallsBackToMatchingTtyUsbIndex)
{
  std::vector<std::string> warnings;
  auto const warn
      = [&warnings] (std::string const &msg) { warnings.push_back (msg); };

  EXPECT_EQ (resolve_serial_port_path (
                 "USB42", Constants::KSERIAL_PORT_CHANNEL_TYPE, nullptr, warn),
             "/dev/ttyUSB42");
  ASSERT_EQ (warnings.size (), 1U);
  EXPECT_NE (warnings[0].find ("USB42"), std::string::npos);
}

TEST (LumexSerialPort, UsbMarkerWithoutWarningCallbackStillResolves)
{
  // logWarningCallback is optional - resolution must still succeed without
  // one.
  EXPECT_EQ (
      resolve_serial_port_path ("USB7", Constants::KSERIAL_PORT_CHANNEL_TYPE),
      "/dev/ttyUSB7");
}

TEST (LumexSerialPort, AlreadyValidTtyUsbNameIsNotTreatedAsBareMarker)
{
  // "ttyUSB0" starts with "USB" only after the "tty" prefix, but it is caught
  // by the earlier tty-prefix branch, so the bare-marker fallback (and its
  // warning) must never fire for it.
  std::vector<std::string> warnings;
  auto const warn
      = [&warnings] (std::string const &msg) { warnings.push_back (msg); };
  EXPECT_EQ (resolve_serial_port_path ("ttyUSB3",
                                       Constants::KSERIAL_PORT_CHANNEL_TYPE,
                                       nullptr, warn),
             "/dev/ttyUSB3");
  EXPECT_TRUE (warnings.empty ());
}

TEST (LumexSerialPort, AlreadyValidUsbByIdNameIsNotTreatedAsBareMarker)
{
  std::vector<std::string> warnings;
  auto const warn
      = [&warnings] (std::string const &msg) { warnings.push_back (msg); };
  EXPECT_EQ (resolve_serial_port_path ("usb-FTDI_FT232R-if00-port0",
                                       Constants::KSERIAL_PORT_CHANNEL_TYPE,
                                       nullptr, warn),
             "/dev/serial/by-id/usb-FTDI_FT232R-if00-port0");
  EXPECT_TRUE (warnings.empty ());
}

#endif
