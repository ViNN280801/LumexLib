# lumex/tests/core/utility/numeric/CMakeLists.txt

# The tests of lumex/core/utility/numeric: the safe numeric comparator and its
# three-way part (LumexSafeNumericComparator, LumexSafeThreeWayCompare) and the
# ordering classes (LumexOrdering), from C++11 (.cxx11) with the C++20 cases
# (.cxx20), which compare the classes and the results with the standard ones.
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityNumericCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without concepts, <span>,
# <ranges> or <compare>) reports GTEST_SKIP.
# LumexNumericSamples.hpp holds the value tables and the exact reference that
# the three-way tests of several standards share.
lumex_add_standard_suites(UtilityNumeric
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    SOURCES LumexNumericSamples.hpp
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
