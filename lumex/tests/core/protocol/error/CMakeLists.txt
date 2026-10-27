# lumex/tests/core/protocol/error/CMakeLists.txt

# LumexProtocolErrorCxx<std>Tests: the error codes and their category.
lumex_add_standard_suites(ProtocolError
    MODULE protocol
    LINK lumex::protocol
)
