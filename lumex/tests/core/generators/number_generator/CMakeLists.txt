# lumex/tests/core/generators/number_generator/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexNumberGeneratorTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexNumberGenerator.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::number_generator
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)

# LumexNumberGenerator.tests.cpp is NOT a GoogleTest binary (no <gtest/gtest.h>, no TEST() macros -
# just a hand-rolled main() with its own RUN_TEST()/TEST_ASSERT() macros). Linking lumex::gtest_main_cxx11
# above never actually pulls in gtest's own main() (this TU's own main() already satisfies the symbol),
# but gtest_discover_tests() still tries to enumerate tests by running it with --gtest_list_tests, which
# this binary does not understand - it just runs its whole ad-hoc suite regardless of argv, which the
# build-time discovery step then kills as a false "Process terminated due to timeout" once it runs long
# enough. add_test() instead just registers it to run under ctest directly, with no enumeration step.
add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})
