# lumex/tests/core/interprocess/CMakeLists.txt

# The tests follow the source tree (lumex/core/interprocess): lock/ for
# interprocess_lock. This directory keeps the tests of the umbrella and of the
# error kind: LumexInterprocess.cxx11.tests.cpp (LumexInterprocessCxx<std>Tests).
# Each directory has one suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake).
lumex_add_standard_suites(Interprocess
    MODULE interprocess
    LINK lumex::interprocess
)

add_subdirectory(lock)
