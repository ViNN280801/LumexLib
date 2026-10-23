# lumex/tests/core/protocol/CMakeLists.txt

# The tests follow the source tree (lumex/core/protocol): dsl/ for the table
# macros, the options, the entries and the collision guard, field/ for the
# integer fields, error/ for the error codes. This directory keeps the test of
# the umbrella and the synthetic tables every directory uses
# (LumexProtocolTestTables.hpp). One suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake: C++11 and C++20).
lumex_add_standard_suites(Protocol
    MODULE protocol
    LINK lumex::protocol
)

add_subdirectory(dsl)
add_subdirectory(error)
add_subdirectory(field)
