# lumex/tests/applied/transport/detail/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportDetailCxx<std>Tests runs
# LumexPermissionDiagnostics.cxx11.tests.cpp.
lumex_add_standard_suites(TransportDetail
    MODULE transport
    LINK lumex::transport
    SOURCES ../LumexTempNode.hpp
)
