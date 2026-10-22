# lumex/tests/applied/logger/logger/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexLoggerLoggerCxx11Tests, LumexLoggerLoggerCxx14Tests,
# LumexLoggerLoggerCxx17Tests and LumexLoggerLoggerCxx20Tests, all built from
# LumexLogger.cxx11.tests.cpp and LumexLoggerReread.cxx11.tests.cpp: the
# logger headers branch on C++14, C++17 and C++20, so every branch is
# compiled and run.
#
# The re-read tests enable the singleton through a trigger file of their own
# and write the log file next to the logger, which every suite (and the tests
# of the transport adapter) shares: RESOURCE_LOCK lumex_logger_trigger_file
# runs those tests one at a time under ctest -j.
lumex_add_standard_suites(LoggerLogger
    MODULE logger
    LINK lumex::logger
    SOURCES LumexLoggerTriggerFile.hpp
    DISCOVER_ARGS PROPERTIES RESOURCE_LOCK lumex_logger_trigger_file
)
