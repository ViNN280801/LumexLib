# lumex/tests/core/crc/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCrcCxx14Tests  LumexCrc.cxx14.tests.cpp: parametric CRC needs C++14
#                       constexpr, the catalogue checks
#                       std::make_index_sequence
#   LumexCrcCxx17Tests  + LumexCrc.cxx17.tests.cpp: the std::string_view
#                       overloads of the catalogue
#   LumexCrcCxx20Tests  + LumexCrc.cxx20.tests.cpp: the std::span overload
# LumexCrcTestHelpers.hpp holds the reference CRC the tests compare against.
lumex_add_standard_suites(Crc
    MODULE crc
    LINK lumex::crc
    SOURCES LumexCrcTestHelpers.hpp
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
