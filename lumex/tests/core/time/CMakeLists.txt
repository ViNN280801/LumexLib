# lumex/tests/core/time/CMakeLists.txt

# time has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexTimeCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(Time
    MODULE time
    LINK
        lumex::time
)
