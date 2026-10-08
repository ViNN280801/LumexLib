# lumex/tests/core/utility/functional/CMakeLists.txt

# The tests of lumex/core/utility/functional: the function objects identity and
# less of LumexFunctional.hpp from C++11 (.cxx11: the classes of this library
# below C++14 and C++20, the standard ones from there, same behavior), the
# heterogeneous lookup and std::less<void> from C++14 (.cxx14), std::identity
# and the agreement with std::ranges::less at C++20 (.cxx20).
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityFunctionalCxx11Tests, ...Cxx14Tests, ...Cxx17Tests,
# ...Cxx20Tests (C++23 and C++26 only where the compiler has them), each built
# from the <Component>.cxx<std>.tests.cpp files of its standard and the lower
# ones; a directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without <ranges>) reports
# GTEST_SKIP.
lumex_add_standard_suites(UtilityFunctional
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
