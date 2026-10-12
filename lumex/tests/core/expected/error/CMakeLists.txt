# lumex/tests/core/expected/error/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexExpectedErrorCxx11Tests  every *.cxx11.tests.cpp: unexpected and
#                                 bad_expected_access
#   LumexExpectedErrorCxx14Tests  the same files at C++14
#   LumexExpectedErrorCxx17Tests  the same files at C++17, also the GCC and
#                                 Clang default
#   LumexExpectedErrorCxx20Tests  the same files with the constexpr
#                                 destructors
#   LumexExpectedErrorCxx23Tests  the same files at C++23
# ExpectedTestTypes.hpp of the module's directory holds the error types the
# files share with result/; ExpectedParitySupport.hpp the tracked and move-only
# types and the detection traits of the std::expected parity tests of both
# directories.
# STANDARDS narrows the module's row to 11 14 17 20 for this directory. Nothing
# in its tests or in the sources they test differs at C++23 from the next lower
# standard (the text of the directory's own files after preprocessing is the
# same, no __cplusplus threshold or LUMEX_HAS_* feature of those standards is
# used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(ExpectedError
    MODULE expected
    LINK lumex::expected
    SOURCES ../ExpectedTestTypes.hpp ../ExpectedParitySupport.hpp
    STANDARDS 11 14 17 20
)
