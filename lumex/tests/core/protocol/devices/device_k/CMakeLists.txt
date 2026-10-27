# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
#
# lumex/tests/core/protocol/devices/device_k/CMakeLists.txt
#
# LumexProtocolDeviceKCxx<std>Tests: the test table of device K (text with
# fields of fixed width in hexadecimal digits, a header with a length in
# front of every message, text state codes) on mangled reference frames with
# recomputed checks.
lumex_add_standard_suites(ProtocolDeviceK
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceKTable.hpp
)
