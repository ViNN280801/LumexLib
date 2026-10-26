# lumex/tests/core/protocol/devices/device_b/CMakeLists.txt

# LumexProtocolDeviceBCxx<std>Tests: the test table of device B (two
# generations: b2 on the family table of device A, b1 on its process
# header).
#
# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
lumex_add_standard_suites(ProtocolDeviceB
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceB.hpp ../device_a/LumexProtocolDeviceA.hpp
            ../LumexProtocolDevicesSupport.hpp
)
