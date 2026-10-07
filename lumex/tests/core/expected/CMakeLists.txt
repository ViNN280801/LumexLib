# lumex/tests/core/expected/CMakeLists.txt

# The tests follow the source tree (lumex/core/expected): error/ tests
# unexpected and bad_expected_access, result/ tests expected<T, E>,
# expected<void, E> and success () / failure (). Each directory has one suite
# per standard of the module (lumex/tests/LumexTestStandards.cmake).
# ExpectedTestTypes.hpp holds the error types both directories use; a test
# source that belongs to no directory of lumex/core/expected is a
# cmake.wiring_standard_suites failure, so a new file goes into error/ or
# result/.
add_subdirectory(error)
add_subdirectory(result)
