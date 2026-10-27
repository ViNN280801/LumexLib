# lumex/tests/core/protocol/devices/device_h/CMakeLists.txt

# LumexProtocolDeviceHCxx<std>Tests: the test table of device H (a
# layer of the family of device A).
#
# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
lumex_add_standard_suites(ProtocolDeviceH
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceH.hpp ../device_a/LumexProtocolDeviceA.hpp
            ../LumexProtocolDevicesSupport.hpp
)
