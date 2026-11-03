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

# The device simulator (testing/simulator) is compiled only when the
# transport compiles the dialogue engine, that is with the protocol and the
# framing modules (lumex/applied/transport/CMakeLists.txt), and so are its
# tests.
lumex_effective_on(_transport_tests_with_simulator_protocol LUMEX_BUILD_PROTOCOL)
lumex_effective_on(_transport_tests_with_simulator_framing LUMEX_BUILD_FRAMING)
if(_transport_tests_with_simulator_protocol AND _transport_tests_with_simulator_framing)
    add_subdirectory(simulator)
endif()
