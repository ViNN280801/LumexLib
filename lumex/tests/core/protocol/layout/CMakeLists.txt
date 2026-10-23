# lumex/tests/core/protocol/layout/CMakeLists.txt

# LumexProtocolLayoutCxx<std>Tests: the three layouts (binary with the
# alignment policy, fixed-width text, tokenized text), the encode and decode
# functions of a protocol row, the same logical command in several layouts,
# and the property tests (random and damaged input never read past its end).
lumex_add_standard_suites(ProtocolLayout
    MODULE protocol
    LINK lumex::protocol
    SOURCES ../LumexProtocolFieldTables.hpp
)
