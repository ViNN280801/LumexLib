# lumex/tests/applied/transport/statistics/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportStatisticsCxx<std>Tests runs the *.cxx11.tests.cpp files: the
# counters themselves, and the counting of byte_stream (mock and TCP on
# loopback) and message_transport (mock).
lumex_add_standard_suites(TransportStatistics
    MODULE transport
    LINK lumex::transport
)
