# lumex/tests/core/utility/mem/CMakeLists.txt

# The tests of lumex/core/utility/mem: the memory readers of LumexMemRead.hpp,
# which work from C++11 (the result is std::optional from C++17 and the
# optional of this library before it): pointer and size, a source object, the
# span of this library and the constraints of the overloads (.cxx11), the
# result type and std::byte from C++17 (.cxx17), std::span (.cxx20).
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityMemCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without concepts, <span>,
# <ranges> or <compare>) reports GTEST_SKIP.
lumex_add_standard_suites(UtilityMem
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
