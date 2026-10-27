# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
#
# lumex/tests/core/protocol/devices/device_i/CMakeLists.txt
#
# LumexProtocolDeviceICxx<std>Tests: the test table of device I (no address
# in the message, scaled counts and floating-point values, a parameter
# table, the same codes as devices E, F and G with other meanings) and of its
# protocol i_alt (the protocol before the out-of-band switch) on mangled
# reference frames.
lumex_add_standard_suites(ProtocolDeviceI
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceITable.hpp
)
