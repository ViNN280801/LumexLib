# lumex/tests/core/time/timer/CMakeLists.txt

# time has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexTimeTimerCxx11Tests builds every *.cxx11.tests.cpp of this directory.
# The measure_time tests read and set environment variables through
# lumex::environment, which lumex::time links.
lumex_add_standard_suites(TimeTimer
    MODULE time
    LINK
        lumex::time
)
