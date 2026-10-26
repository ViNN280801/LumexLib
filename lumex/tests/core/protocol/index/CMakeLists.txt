# lumex/tests/core/protocol/index/CMakeLists.txt

# LumexProtocolIndexCxx<std>Tests: the run-time index (rows, descriptors,
# lookup by key, name and frame, layers, key readers, duplicate keys), describe
# for every kind of field and layout, and the session context.
# LumexProtocolIndexTables.hpp (the synthetic widget) and
# LumexProtocolFieldTables.hpp (every field kind) are shared with the other
# directories of the module.
lumex_add_standard_suites(ProtocolIndex
    MODULE protocol
    LINK lumex::protocol
    SOURCES ../LumexProtocolIndexTables.hpp ../LumexProtocolFieldTables.hpp
)
