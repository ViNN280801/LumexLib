# lumex/tests/core/protocol/devices/device_l/CMakeLists.txt

# LumexProtocolDeviceLCxx<std>Tests: the test table of device L (a register
# table over Modbus RTU with the check high byte first; the frames go
# through the codec of core/framing).
#
# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
lumex_add_standard_suites(ProtocolDeviceL
    MODULE protocol
    LINK lumex::protocol lumex::framing
    SOURCES LumexProtocolDeviceL.hpp ../LumexProtocolDevicesSupport.hpp
)
