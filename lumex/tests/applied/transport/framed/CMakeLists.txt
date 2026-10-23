# lumex/tests/applied/transport/framed/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportFramedCxx<std>Tests runs the *.cxx11.tests.cpp files: every
# codec of core/framing through framed_stream over the mock stream, the frame
# queue and the statistics, the completion token styles, cancellation, and
# framed_datagram over the mock datagram transport. ../CMakeLists.txt adds
# this directory only when the framing module is built (the adapters are
# compiled only then).
lumex_add_standard_suites(TransportFramed
    MODULE transport
    LINK lumex::transport lumex::framing
    SOURCES LumexFramedTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
