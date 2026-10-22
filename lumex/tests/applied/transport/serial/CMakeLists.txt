# lumex/tests/applied/transport/serial/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportSerialCxx<std>Tests runs LumexSerialStream.cxx11.tests.cpp; its
# real-port case runs only with LUMEX_TEST_SERIAL_PORT set.
lumex_add_standard_suites(TransportSerial
    MODULE transport
    LINK lumex::transport
    SOURCES ../LumexTempNode.hpp
)
