# lumex/tests/core/utility/sequence/CMakeLists.txt

# The tests of lumex/core/utility/sequence: integer_sequence, index_sequence,
# make_integer_sequence, make_index_sequence and index_sequence_for of
# LumexIndexSequence.hpp from C++11 (.cxx11: the class of this library below
# C++14, the aliases of the standard ones from it), and the identity of every
# name with the standard one from C++14 (.cxx14).
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# LumexUtilitySequenceCxx11Tests, ...Cxx14Tests, ...Cxx17Tests, ...Cxx20Tests
# (C++23 and C++26 only where the compiler has them), each built from the
# <Component>.cxx<std>.tests.cpp files of its standard and the lower ones; a
# directory has no suite below its lowest file. A test of a feature the
# toolchain lacks reports GTEST_SKIP.
lumex_add_standard_suites(UtilitySequence
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
