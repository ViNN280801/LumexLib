# lumex/tests/applied/logging/log/CMakeLists.txt

# logging has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexLoggingLogCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(LoggingLog
    MODULE logging
    LINK
        lumex::logging
)
