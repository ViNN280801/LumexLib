# lumex/tests/core/expected/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexExpectedCxx11Tests  every *.cxx11.tests.cpp: Expected<T, E>,
#                            Expected<void, E>, Unexpected, BadExpectedAccess,
#                            the monadic operations, success () / failure ()
#                            and the type matrix over the C++11 types, and
#                            ExpectedWithOptional (both modules in one file)
#   LumexExpectedCxx17Tests  + SuccessFailure.cxx17.tests.cpp: the full type
#                            matrix with std::string_view, std::optional and
#                            std::variant; C++17 is also the GCC and Clang
#                            default, where LUMEX_CONSTEXPR_DTOR stays empty
#   LumexExpectedCxx20Tests  the same sources with the constexpr destructors
#                            and the concepts branch of the monadic operations
# ExpectedTestTypes.hpp and SuccessFailureTestSupport.hpp hold the error types
# and the type matrix the files of several standards share.
# ExpectedWithOptional.cxx11.tests.cpp includes both value-holding modules, so
# it is built only where the optional module is.
set(_expected_link lumex::expected)
set(_expected_exclude "")
if(TARGET lumex::optional)
    list(APPEND _expected_link lumex::optional)
else()
    set(_expected_exclude ExpectedWithOptional.cxx11.tests.cpp)
    message(STATUS "Skipping ExpectedWithOptional.cxx11.tests.cpp of LumexExpected*Tests: requires LUMEX_BUILD_OPTIONAL=ON")
endif()

lumex_add_standard_suites(Expected
    MODULE expected
    LINK ${_expected_link}
    SOURCES ExpectedTestTypes.hpp SuccessFailureTestSupport.hpp
    EXCLUDE ${_expected_exclude}
)
