# lumex/tests/applied/transport/net/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportNetCxx<std>Tests runs LumexNetTransports.cxx11.tests.cpp (TCP
# and UDP on loopback against plain Asio peers).
lumex_add_standard_suites(TransportNet
    MODULE transport
    LINK lumex::transport
)
