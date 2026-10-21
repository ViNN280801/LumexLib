# lumex/tests/applied/transport/error/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportErrorCxx<std>Tests runs LumexTransportErrc.cxx11.tests.cpp.
lumex_add_standard_suites(TransportError
    MODULE transport
    LINK lumex::transport
)
