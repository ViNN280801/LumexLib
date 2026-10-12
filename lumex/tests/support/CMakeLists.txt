# lumex/tests/support/CMakeLists.txt
#
# The header-only helpers that the concurrency tests share (core/atomic and
# core/hazard_pointer): configuration from the environment, the seeded
# schedule, the thread helpers, the reuse-address allocator, the object
# ledger, the history recorder with the linearizability checker and the
# subprocess runner. They depend on nothing of LumexLib, so the directory is
# registered whenever the tests are, and it is not a module of
# LumexTestStandards.cmake (cmake.wiring_standard_suites skips support/).
# One suite per standard the helpers are compiled at:
#   LumexTestSupportCxx11Tests, LumexTestSupportCxx17Tests,
#   LumexTestSupportCxx20Tests
# and the CTest names are support.<Suite>.<Test>.cxx<std>. A second
# executable, LumexTestSupportNoForkCxx11Tests, builds the subprocess runner
# for platforms without fork () (LUMEX_TEST_FORCE_NO_FORK) so that branch is
# tested on every platform.
foreach(_std IN ITEMS 11 17 20)
    set(_target LumexTestSupportCxx${_std}Tests)
    add_executable(${_target} LumexTestSupport.cxx11.tests.cpp)
    target_include_directories(${_target} PRIVATE ${LumexLib_SOURCE_DIR})
    target_link_libraries(${_target} PRIVATE Threads::Threads)
    lumex_test_use_gtest(${_target} CXX_STANDARD ${_std})
    lumex_gtest_discover_tests(${_target}
        TEST_SUFFIX .cxx${_std}
        PROPERTIES TIMEOUT 300)
endforeach()

add_executable(LumexTestSupportNoForkCxx11Tests
    LumexTestSupportNoFork.cxx11.tests.cpp)
target_include_directories(LumexTestSupportNoForkCxx11Tests PRIVATE
    ${LumexLib_SOURCE_DIR})
target_compile_definitions(LumexTestSupportNoForkCxx11Tests PRIVATE
    LUMEX_TEST_FORCE_NO_FORK)
target_link_libraries(LumexTestSupportNoForkCxx11Tests PRIVATE
    Threads::Threads)
lumex_test_use_gtest(LumexTestSupportNoForkCxx11Tests CXX_STANDARD 11)
lumex_gtest_discover_tests(LumexTestSupportNoForkCxx11Tests
    TEST_SUFFIX .nofork.cxx11
    PROPERTIES TIMEOUT 300)
