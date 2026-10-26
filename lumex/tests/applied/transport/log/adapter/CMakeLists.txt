# lumex/tests/applied/transport/log/adapter/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportLogAdapterCxx<std>Tests runs
# LumexTransportLoggerAdapter.cxx11.tests.cpp, the adapter from the transport
# log to the LumexLogger file logger. ../CMakeLists.txt adds this directory
# only when the logger is built (the adapter is compiled only then).
#
# The tests enable the logger through a trigger file of their own and write
# the log file next to the logger, which the logger's own re-read tests share:
# the same RESOURCE_LOCK runs them one at a time under ctest -j.
lumex_add_standard_suites(TransportLogAdapter
    MODULE transport
    LINK lumex::transport lumex::logger
    SOURCES ../../../logger/logger/LumexLoggerTriggerFile.hpp
    DISCOVER_ARGS PROPERTIES RESOURCE_LOCK lumex_logger_trigger_file
)
