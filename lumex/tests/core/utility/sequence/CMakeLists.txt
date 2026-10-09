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
# STANDARDS narrows the module's row to 11 14 17 20 for this directory. Nothing
# in its tests or in the sources they test differs at C++23 and C++26 from the
# next lower standard (the text of the directory's own files after
# preprocessing is the same, no __cplusplus threshold or LUMEX_HAS_* feature of
# those standards is used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(UtilitySequence
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
    STANDARDS 11 14 17 20
)
