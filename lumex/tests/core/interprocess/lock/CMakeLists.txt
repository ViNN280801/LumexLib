# lumex/tests/core/interprocess/lock/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexInterprocessLockCxx<std>Tests runs LumexInterprocessLock.cxx11.tests.cpp.
# The lock files go into the working directory of the test, one directory per
# case and process (the death test: per standard).
lumex_add_standard_suites(InterprocessLock
    MODULE interprocess
    LINK lumex::interprocess
)
