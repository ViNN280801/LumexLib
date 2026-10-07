# lumex/tests/core/utility/traits/CMakeLists.txt

# The tests of lumex/core/utility/traits: the type, range and stream traits:
# from C++11 (.cxx11), the stream trait variable templates (.cxx14),
# is_optional on std::optional (.cxx17), the concepts and the three-way
# comparison (.cxx20).
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityTraitsCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without concepts, <span>,
# <ranges> or <compare>) reports GTEST_SKIP.
# LumexStreamTraitsTestTypes.hpp holds the types the stream trait tests of
# several standards share.
lumex_add_standard_suites(UtilityTraits
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    SOURCES LumexStreamTraitsTestTypes.hpp
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
