# lumex/tests/applied/transport/log/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportLogCxx<std>Tests runs LumexTransportLog.cxx11.tests.cpp and
# LumexTransportLogMinLevel.cxx11.tests.cpp (a translation unit compiled with
# LUMEX_TRANSPORT_LOG_MIN_LEVEL 3).
lumex_add_standard_suites(TransportLog
    MODULE transport
    LINK lumex::transport
)

# The adapter to the logger module is compiled only with the logger
# (lumex/applied/transport/CMakeLists.txt), and so are its tests.
lumex_effective_on(_transport_log_with_logger LUMEX_BUILD_LOGGER)
if(_transport_log_with_logger)
    add_subdirectory(adapter)
endif()
