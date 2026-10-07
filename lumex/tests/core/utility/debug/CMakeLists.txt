# lumex/tests/core/utility/debug/CMakeLists.txt

# The tests of lumex/core/utility/debug: capture_stack_trace and the debug
# helpers, from C++11; LumexDebug.hpp uses the DbgHelp API on Windows.
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityDebugCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without concepts, <span>,
# <ranges> or <compare>) reports GTEST_SKIP.
lumex_add_standard_suites(UtilityDebug
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
