# lumex/tests/applied/transport/testing/simulator/CMakeLists.txt

# LumexTransportTestingSimulatorCxx<std>Tests: the device simulator
# (LumexDeviceSimulator.hpp) on the mocks, on a virtual clock. The synthetic
# lab table of LumexSimulatorTestTables.hpp gives one row of every reply
# kind, a stream, rows accepted in one state, a registry with generic get and
# set rows and rows limited to a firmware (LumexDeviceSimulator.cxx11,
# LumexDeviceSimulatorLifetime.cxx11, LumexDeviceSimulatorBehaviors.cxx11);
# LumexDeviceSimulatorDevice<A..M>.cxx11 send the sample request of every row
# of every layer of the de-identified device tables
# (lumex/tests/core/protocol/devices) through the dialogue engine, one
# translation unit per device to keep the compile memory of the largest
# table in bounds. ../CMakeLists.txt adds this directory only when the
# transport compiles the engine (protocol and framing modules). C++11 and
# C++20, as the dialogue tests.
lumex_add_standard_suites(TransportTestingSimulator
    MODULE transport
    STANDARDS 11 20
    LINK lumex::transport lumex::protocol lumex::framing
    SOURCES
        LumexSimulatorDeviceSupport.hpp
        LumexSimulatorTestSupport.hpp
        LumexSimulatorTestTables.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
