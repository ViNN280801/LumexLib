# lumex/tests/core/protocol/registry/CMakeLists.txt

# LumexProtocolRegistryCxx<std>Tests: the registry table macro, the checks of
# a write (access, limits, raw value), availability by firmware and model, the
# typed generic commands over every kind of key, and verify_registry.
# LumexProtocolIndexTables.hpp holds the synthetic lamp and widget tables.
lumex_add_standard_suites(ProtocolRegistry
    MODULE protocol
    LINK lumex::protocol
    SOURCES ../LumexProtocolIndexTables.hpp
)
