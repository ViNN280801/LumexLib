# lumex/tests/core/protocol/field/CMakeLists.txt

# LumexProtocolFieldCxx<std>Tests: every field type of the module (integers
# of every width and order, scaled, floating and sign-magnitude fields, text
# fields, bits, masks, splits, enumerations, arrays, counts, tails, unions,
# nested messages), their value types, records, limits and error paths.
lumex_add_standard_suites(ProtocolField
    MODULE protocol
    LINK lumex::protocol
    SOURCES ../LumexProtocolFieldTables.hpp
)
