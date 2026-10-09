# lumex/tests/core/utility/bit/CMakeLists.txt

# The tests of lumex/core/utility/bit: count_leading_zeros, byte_swap (the
# form without bit_cast) and is_little_endian from C++11 (.cxx11), the C++20
# byte_swap (.cxx20), std::byteswap at C++23 (.cxx23).
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilityBitCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks (GCC 8 accepts -std=c++2a without concepts, <span>,
# <ranges> or <compare>) reports GTEST_SKIP.
# STANDARDS narrows the module's row to 11 17 20 23 for this directory. Nothing
# in its tests or in the sources they test differs at C++14 and C++26 from the
# next lower standard (the text of the directory's own files after
# preprocessing is the same, no __cplusplus threshold or LUMEX_HAS_* feature of
# those standards is used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(UtilityBit
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
    STANDARDS 11 17 20 23
)
