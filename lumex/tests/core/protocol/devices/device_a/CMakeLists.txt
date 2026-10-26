# lumex/tests/core/protocol/devices/device_a/CMakeLists.txt

# LumexProtocolDeviceACxx<std>Tests: the test table of device A (the common
# application layer of a device family and its message formats).
#
# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
lumex_add_standard_suites(ProtocolDeviceA
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceA.hpp ../LumexProtocolDevicesSupport.hpp
)
