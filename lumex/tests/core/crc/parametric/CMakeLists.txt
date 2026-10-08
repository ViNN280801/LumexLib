# lumex/tests/core/crc/parametric/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCrcParametricCxx11Tests  LumexCrcParametric.cxx11.tests.cpp and
#                                 LumexCrcParametricSpan.cxx11.tests.cpp: the
#                                 table is built on first use
#   LumexCrcParametricCxx14Tests  the same files at C++14: the table is built
#                                 at compile time
#   LumexCrcParametricCxx17Tests  the same files at C++17
#   LumexCrcParametricCxx20Tests  + LumexCrcParametric.cxx20.tests.cpp: the
#                                 std::span overload, and the span checks of
#                                 the typed tests
# LumexCrcTestHelpers.hpp holds the reference CRC the tests compare against.
lumex_add_standard_suites(CrcParametric
    MODULE crc
    LINK lumex::crc
    SOURCES LumexCrcTestHelpers.hpp
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
