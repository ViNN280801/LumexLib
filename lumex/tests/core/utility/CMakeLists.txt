# lumex/tests/core/utility/CMakeLists.txt

# The tests follow the source tree (lumex/core/utility): one directory per
# directory of the module that has tests, each with one suite per standard of
# the module (lumex/tests/LumexTestStandards.cmake): LumexUtility<Dir>Cxx11Tests,
# ...Cxx14Tests, ... ...Cxx26Tests (C++23 and C++26 only where the compiler has
# them). This directory keeps LumexUtilityMinMaxMacros.cxx11.tests.cpp, which
# includes the umbrella and belongs to no subdirectory: LumexUtilityCxx11Tests
# and so on.
# STANDARDS narrows the module's row to 11 17 20 for this directory. Nothing in
# its tests or in the sources they test differs at C++14, C++23 and C++26 from
# the next lower standard (the text of the directory's own files after
# preprocessing is the same, no __cplusplus threshold or LUMEX_HAS_* feature of
# those standards is used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(Utility
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
    STANDARDS 11 17 20
)

add_subdirectory(attr)
add_subdirectory(bit)
add_subdirectory(callback)
add_subdirectory(cast)
add_subdirectory(debug)
add_subdirectory(demangle)
add_subdirectory(dump)
add_subdirectory(functional)
add_subdirectory(macros)
add_subdirectory(mem)
add_subdirectory(numeric)
add_subdirectory(os)
add_subdirectory(process)
add_subdirectory(ranges)
add_subdirectory(sequence)
add_subdirectory(traits)
add_subdirectory(util)
