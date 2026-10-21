# lumex/tests/applied/transport/log/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportLogCxx<std>Tests runs LumexTransportLog.cxx11.tests.cpp and
# LumexTransportLogMinLevel.cxx11.tests.cpp (a translation unit compiled with
# LUMEX_TRANSPORT_LOG_MIN_LEVEL 3).
lumex_add_standard_suites(TransportLog
    MODULE transport
    LINK lumex::transport
)
