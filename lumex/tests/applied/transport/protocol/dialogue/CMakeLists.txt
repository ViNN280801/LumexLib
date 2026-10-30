# lumex/tests/applied/transport/protocol/dialogue/CMakeLists.txt

# LumexTransportProtocolDialogueCxx<std>Tests: the dialogue engine over a
# mock stream framed with SLIP, on a virtual clock, with the synthetic pump
# table of the vocabulary tests (../dsl/LumexDialogueTestTables.hpp): the
# correlations, the queue and the in-flight limit, the timeouts and the
# retries, the late replies, the counters, cancel, close, destruction and
# per-exchange cancellation. ../CMakeLists.txt adds this directory only when
# the transport compiles the engine (protocol and framing modules). C++11
# and C++20, as the vocabulary.
lumex_add_standard_suites(TransportProtocolDialogue
    MODULE transport
    STANDARDS 11 20
    LINK lumex::transport lumex::protocol lumex::framing
    SOURCES LumexDialogueTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
