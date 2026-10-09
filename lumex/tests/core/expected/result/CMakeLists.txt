# lumex/tests/core/expected/result/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexExpectedResultCxx11Tests  every *.cxx11.tests.cpp: expected<T, E>,
#                                  expected<void, E>, the monadic operations,
#                                  the special member functions against the
#                                  standard (ExpectedMembers), the order of
#                                  construction and destruction (ExpectedReinit),
#                                  constant expressions of C++11
#                                  (ExpectedConstexpr), success () / failure ()
#                                  and the type matrix over the C++11 types,
#                                  and ExpectedWithOptional (both modules in one
#                                  file)
#   LumexExpectedResultCxx14Tests  + ExpectedConstexpr.cxx14.tests.cpp: the
#                                  observers and monadic operations of an
#                                  object that is not const in a constant
#                                  expression
#   LumexExpectedResultCxx17Tests  + SuccessFailure.cxx17.tests.cpp: the full
#                                  type matrix with std::string_view,
#                                  std::optional and std::variant, and
#                                  ExpectedConstexpr.cxx17.tests.cpp (lambdas);
#                                  C++17 is also the GCC and Clang default,
#                                  where LUMEX_CONSTEXPR_DTOR stays empty
#   LumexExpectedResultCxx20Tests  the same sources with the constexpr
#                                  destructors and the concepts branch of the
#                                  monadic operations, and
#                                  ExpectedConstexpr.cxx20.tests.cpp (the
#                                  operations that change the active
#                                  alternative of the union)
#   LumexExpectedResultCxx23Tests  + ExpectedStdDifferential.cxx23.tests.cpp:
#                                  the comparison with the std::expected of the
#                                  standard library (one skipped test where it
#                                  has none)
# SuccessFailureTestSupport.hpp holds the type matrix the files of several
# standards share; ExpectedTestTypes.hpp of the module's directory the error
# types shared with error/; ExpectedParitySupport.hpp the tracked and move-only
# types and the detection traits of the std::expected parity tests of both
# directories; ExpectedMembersSupport.hpp the type zoo of the tests of the
# special member functions and of the differential test; ExpectedProbe.hpp the
# probe type that logs its operations.
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
    ../ExpectedMembersSupport.hpp ../ExpectedProbe.hpp
    SuccessFailureTestSupport.hpp
    EXCLUDE ${_expected_exclude}
)
