# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
#
# lumex/tests/core/protocol/devices/device_f/CMakeLists.txt
#
# LumexProtocolDeviceFCxx<std>Tests: the test table of device F (a state
# message with three layouts chosen by a session value and by a field,
# hundredths of a degree, the identity reply with swapped versions) on
# mangled reference frames.
lumex_add_standard_suites(ProtocolDeviceF
    MODULE protocol
    LINK lumex::protocol
    SOURCES LumexProtocolDeviceFTable.hpp
)
