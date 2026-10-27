# lumex/tests/core/protocol/dsl/CMakeLists.txt

# LumexProtocolDslCxx<std>Tests: the table macros (DEFINE, EXTEND, LAYOUT,
# the client methods), the row options, the run-time entries, the 100-row list
# and the 40-field layout, the collision guard, the type lists and the keys.
lumex_add_standard_suites(ProtocolDsl
    MODULE protocol
    LINK lumex::protocol
    SOURCES ../LumexProtocolTestTables.hpp
)
