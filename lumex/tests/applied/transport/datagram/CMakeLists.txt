# lumex/tests/applied/transport/datagram/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportDatagramCxx<std>Tests runs LumexMessageTransport.cxx11.tests.cpp
# (message_transport over the mock).
lumex_add_standard_suites(TransportDatagram
    MODULE transport
    LINK lumex::transport
)
