# lumex/tests/applied/logger/logger/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexLoggerLoggerCxx11Tests, LumexLoggerLoggerCxx14Tests,
# LumexLoggerLoggerCxx17Tests and LumexLoggerLoggerCxx20Tests, all built from
# LumexLogger.cxx11.tests.cpp: the logger headers branch on C++14, C++17 and
# C++20, so every branch is compiled and run.
lumex_add_standard_suites(LoggerLogger
    MODULE logger
    LINK lumex::logger
)
