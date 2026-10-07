# lumex/tests/core/expected/result/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexExpectedResultCxx11Tests  every *.cxx11.tests.cpp: expected<T, E>,
#                                  expected<void, E>, the monadic operations,
#                                  success () / failure () and the type matrix
#                                  over the C++11 types, and
#                                  ExpectedWithOptional (both modules in one
#                                  file)
#   LumexExpectedResultCxx17Tests  + SuccessFailure.cxx17.tests.cpp: the full
#                                  type matrix with std::string_view,
#                                  std::optional and std::variant; C++17 is
#                                  also the GCC and Clang default, where
#                                  LUMEX_CONSTEXPR_DTOR stays empty
#   LumexExpectedResultCxx20Tests  the same sources with the constexpr
#                                  destructors and the concepts branch of the
#                                  monadic operations
# SuccessFailureTestSupport.hpp holds the type matrix the files of several
# standards share; ExpectedTestTypes.hpp of the module's directory the error
# types shared with error/; ExpectedParitySupport.hpp the tracked and move-only
# types and the detection traits of the std::expected parity tests of both
# directories.
# ExpectedWithOptional.cxx11.tests.cpp includes both value-holding modules, so
# it is built only where the optional module is.
set(_expected_link lumex::expected)
set(_expected_exclude "")
if(TARGET lumex::optional)
    list(APPEND _expected_link lumex::optional)
else()
    set(_expected_exclude ExpectedWithOptional.cxx11.tests.cpp)
    message(STATUS "Skipping ExpectedWithOptional.cxx11.tests.cpp of LumexExpectedResult*Tests: requires LUMEX_BUILD_OPTIONAL=ON")
endif()

lumex_add_standard_suites(ExpectedResult
    MODULE expected
    LINK ${_expected_link}
    SOURCES ../ExpectedTestTypes.hpp ../ExpectedParitySupport.hpp
    SuccessFailureTestSupport.hpp
    EXCLUDE ${_expected_exclude}
)
