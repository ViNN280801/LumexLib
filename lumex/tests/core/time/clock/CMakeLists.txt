# lumex/tests/core/time/clock/CMakeLists.txt

# time has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexTimeClockCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(TimeClock
    MODULE time
    LINK
        lumex::time
)
