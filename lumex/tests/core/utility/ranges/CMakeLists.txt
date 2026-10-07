# lumex/tests/core/utility/ranges/CMakeLists.txt

# The tests of lumex/core/utility/ranges: iterator_range from C++11 (.cxx11),
# the C++20 range helpers (.cxx20) and std::ranges::contains (.cxx23).
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityRangesCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without concepts, <span>,
# <ranges> or <compare>) reports GTEST_SKIP.
lumex_add_standard_suites(UtilityRanges
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
