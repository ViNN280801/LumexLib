# lumex/tests/core/utility/CMakeLists.txt

# The tests follow the source tree (lumex/core/utility): one directory per
# directory of the module that has tests, each with one suite per standard of
# the module (lumex/tests/LumexTestStandards.cmake): LumexUtility<Dir>Cxx11Tests,
# ...Cxx14Tests, ... ...Cxx26Tests (C++23 and C++26 only where the compiler has
# them). This directory keeps LumexUtilityMinMaxMacros.cxx11.tests.cpp, which
# includes the umbrella and belongs to no subdirectory: LumexUtilityCxx11Tests
# and so on.
lumex_add_standard_suites(Utility
    MODULE utility
    LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)

add_subdirectory(attr)
add_subdirectory(bit)
add_subdirectory(callback)
add_subdirectory(cast)
add_subdirectory(debug)
add_subdirectory(demangle)
add_subdirectory(dump)
add_subdirectory(macros)
add_subdirectory(mem)
add_subdirectory(numeric)
add_subdirectory(os)
add_subdirectory(process)
add_subdirectory(ranges)
add_subdirectory(sequence)
add_subdirectory(traits)
add_subdirectory(util)
