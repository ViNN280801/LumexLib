# lumex/tests/core/utility/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake)
# for every component of the directory: LumexUtilityCxx11Tests,
# LumexUtilityCxx14Tests, ... LumexUtilityCxx26Tests (C++23 and C++26 only
# where the compiler has them). The suite of a standard compiles the
# <Component>.cxx<std>.tests.cpp files of that standard and of every lower
# one:
#   .cxx11  every component that works from C++11: Attributes, CallbackSlot,
#           CheckOS, CoreDumpGenerator, Debug, Demangle, ExceptionMacros,
#           Macros (and the MinGW simulation), Process, RangeTraits,
#           SafeNumericComparator, StreamTraits, TypeTraits, TypeTraitsTopics,
#           Utilities, and Bit's count_leading_zeros
#   .cxx14  the stream trait variable templates
#   .cxx17  is_optional on std::optional
#   .cxx20  Bit's byte_swap, Cast, MemRead and Ranges (C++20 modules), the
#           concepts, the three-way comparison and LUMEX_DEFINE_ENUM_TRAITS
#   .cxx23  std::byteswap and std::ranges::contains
# A test of a feature the toolchain lacks (GCC 8 accepts -std=c++2a without
# concepts, <span>, <ranges> or <compare>) reports GTEST_SKIP.
# LumexStreamTraitsTestTypes.hpp holds the types the stream trait tests of
# several standards share. LumexDebug.hpp's captureStackTrace () uses the
# DbgHelp API on Windows.
lumex_add_standard_suites(Utility
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    SOURCES LumexStreamTraitsTestTypes.hpp
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
