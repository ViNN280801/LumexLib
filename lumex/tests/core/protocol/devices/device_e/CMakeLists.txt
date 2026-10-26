# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
#
# lumex/tests/core/protocol/devices/device_e/CMakeLists.txt
#
# LumexProtocolDeviceECxx<std>Tests: the test table of device E (a family
# layer, a mode message chosen by its first byte, a parameter table addressed
# by a pair, a tail of the newer firmware) on mangled reference frames.
lumex_add_standard_suites(ProtocolDeviceE
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceETable.hpp
)
