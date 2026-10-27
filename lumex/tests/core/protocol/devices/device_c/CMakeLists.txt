# lumex/tests/core/protocol/devices/device_c/CMakeLists.txt

# LumexProtocolDeviceCCxx<std>Tests: the test table of device C (the
# process header of device A, three parameter tables, an accessory
# nested in a bus transaction).
#
# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
lumex_add_standard_suites(ProtocolDeviceC
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceC.hpp ../device_a/LumexProtocolDeviceA.hpp
            ../LumexProtocolDevicesSupport.hpp
)
