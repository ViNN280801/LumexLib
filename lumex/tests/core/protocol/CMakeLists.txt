# lumex/tests/core/protocol/CMakeLists.txt

# The tests follow the source tree (lumex/core/protocol): dsl/ for the table
# macros, the options, the entries and the collision guard, field/ for the
# field types, layout/ for the layouts and the encoders and decoders, error/
# for the error codes, registry/ for the parameter tables, index/ for the
# run-time index, describe and the session context, export/ for the Markdown
# and JSON exports, devices/ for the test tables of the researched devices
# (altered for NDA reasons). This directory keeps the test of the umbrella and the
# synthetic tables the directories use (LumexProtocolTestTables.hpp,
# LumexProtocolFieldTables.hpp). One suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake: C++11 and C++20).
lumex_add_standard_suites(Protocol
    MODULE protocol
    LINK lumex::protocol
)

add_subdirectory(devices)
add_subdirectory(dsl)
add_subdirectory(error)
add_subdirectory(export)
add_subdirectory(field)
add_subdirectory(index)
add_subdirectory(layout)
add_subdirectory(registry)
