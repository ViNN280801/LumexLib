# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
#
# lumex/tests/core/protocol/devices/device_j/CMakeLists.txt
#
# LumexProtocolDeviceJCxx<std>Tests: the test table of device J, one logical
# protocol in two layers (j_text: fixed-width hexadecimal digits, j_hid: the
# bytes of a report), on mangled reference frames that both layers must
# agree on.
lumex_add_standard_suites(ProtocolDeviceJ
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceJTable.hpp
)
