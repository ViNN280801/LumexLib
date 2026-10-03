# lumex/tests/applied/logging/CMakeLists.txt

# logging has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexLoggingCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(Logging
    MODULE logging
    LINK
        lumex::logging
)
