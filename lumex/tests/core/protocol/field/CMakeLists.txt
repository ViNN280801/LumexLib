# lumex/tests/core/protocol/field/CMakeLists.txt

# LumexProtocolFieldCxx<std>Tests: the integer fields, the scale modifier and
# the field traits.
lumex_add_standard_suites(ProtocolField
    MODULE protocol
    LINK lumex::protocol
)
