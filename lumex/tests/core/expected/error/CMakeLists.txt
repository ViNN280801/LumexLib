# lumex/tests/core/expected/error/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexExpectedErrorCxx11Tests  every *.cxx11.tests.cpp: unexpected and
#                                 bad_expected_access
#   LumexExpectedErrorCxx17Tests  the same files at C++17, also the GCC and
#                                 Clang default
#   LumexExpectedErrorCxx20Tests  the same files with the constexpr
#                                 destructors
# ExpectedTestTypes.hpp of the module's directory holds the error types the
# files share with result/.
lumex_add_standard_suites(ExpectedError
    MODULE expected
    LINK lumex::expected
    SOURCES ../ExpectedTestTypes.hpp
)
