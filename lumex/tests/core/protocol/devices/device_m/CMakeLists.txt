# lumex/tests/core/protocol/devices/device_m/CMakeLists.txt

# LumexProtocolDeviceMCxx<std>Tests: the test table of device M (m1:
# tokenized text; m2: binary frames through the header codec of
# core/framing).
#
# The protocol below was intentionally altered (names, codes, constants,
# strings) for NDA reasons; it does not describe any real device.
lumex_add_standard_suites(ProtocolDeviceM
    MODULE protocol
    LINK lumex::protocol lumex::framing
    SOURCES LumexProtocolDeviceM.hpp ../LumexProtocolDevicesSupport.hpp
)
