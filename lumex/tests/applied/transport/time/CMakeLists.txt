# lumex/tests/applied/transport/time/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportTimeCxx<std>Tests runs LumexTimerSource.cxx11.tests.cpp (the
# steady timer source and the timer front).
lumex_add_standard_suites(TransportTime
    MODULE transport
    LINK lumex::transport
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
