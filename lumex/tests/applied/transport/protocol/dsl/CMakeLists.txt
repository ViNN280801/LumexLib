# lumex/tests/applied/transport/protocol/dsl/CMakeLists.txt

# LumexTransportProtocolDslCxx<std>Tests: the dialogue vocabulary over the
# synthetic pump table (LumexDialogueTestTables.hpp): the reply kinds as the
# index describes and exports them, the row options in the run-time entries,
# the traits of a row and of a protocol, the protocol options of a header.
# The vocabulary is header-only over core/protocol and does not change
# between C++11 and C++20 but for the GoogleTest version: C++11 and C++20.
lumex_add_standard_suites(TransportProtocolDsl
    MODULE transport
    STANDARDS 11 20
    LINK lumex::protocol
    SOURCES LumexDialogueTestTables.hpp
)
