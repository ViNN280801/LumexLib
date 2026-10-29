# lumex/tests/applied/transport/testing/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportTestingCxx<std>Tests runs LumexVirtualClock.cxx11.tests.cpp
# (the virtual clock and its timers) and, from C++20,
# LumexVirtualClockAwaitable.cxx20.tests.cpp (a coroutine waits on it).
lumex_add_standard_suites(TransportTesting
    MODULE transport
    LINK lumex::transport
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
