# lumex/tests/core/utility/dump/CMakeLists.txt

# The tests of lumex/core/utility/dump: core_dump_generator, from C++11.
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityDumpCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without concepts, <span>,
# <ranges> or <compare>) reports GTEST_SKIP.
# LumexCoreDumpTestTypes.hpp holds the argument types and the detectors of the
# string-taking members that the tests of several standards share.
lumex_add_standard_suites(UtilityDump
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    SOURCES LumexCoreDumpTestTypes.hpp
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
