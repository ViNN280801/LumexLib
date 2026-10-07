# lumex/tests/core/crc/parametric/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCrcParametricCxx14Tests  LumexCrcParametric.cxx14.tests.cpp: parametric
#                                 CRC needs C++14 constexpr
#   LumexCrcParametricCxx17Tests  the same file at C++17
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
