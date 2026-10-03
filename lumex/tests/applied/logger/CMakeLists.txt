# lumex/tests/applied/logger/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexLoggerCxx11Tests, LumexLoggerCxx14Tests, LumexLoggerCxx17Tests and
# LumexLoggerCxx20Tests, all built from LumexLogger.cxx11.tests.cpp: the
# logger headers branch on C++14, C++17 and C++20, so every branch is
# compiled and run.
lumex_add_standard_suites(Logger
    MODULE logger
    LINK lumex::logger
)
