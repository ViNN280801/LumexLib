# lumex/tests/core/protocol/devices/device_d/CMakeLists.txt

# LumexProtocolDeviceDCxx<std>Tests: the test table of device D (a
# layer of the family of device A).
#
# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
lumex_add_standard_suites(ProtocolDeviceD
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceD.hpp ../device_a/LumexProtocolDeviceA.hpp
            ../LumexProtocolDevicesSupport.hpp
)
