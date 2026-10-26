# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
#
# lumex/tests/core/protocol/devices/device_g/CMakeLists.txt
#
# LumexProtocolDeviceGCxx<std>Tests: the test table of device G (interleaved
# bit masks, an identity reply chosen by its length, a statistics message
# chosen by its first byte) and of its older protocol g_old (single-character
# keys) on mangled reference frames.
lumex_add_standard_suites(ProtocolDeviceG
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceGTable.hpp
)
