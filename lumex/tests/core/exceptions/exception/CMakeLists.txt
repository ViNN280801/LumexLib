# lumex/tests/core/exceptions/exception/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexExceptionsExceptionCxx11Tests  the .cxx11.tests.cpp files:
#                                       LumexException,
#                                       LumexExceptionHeader and
#                                       LumexExceptionStringView (the string
#                                       view constructor, an inline wrapper
#                                       over the exported std::string one:
#                                       lumex_string_view below C++17,
#                                       std::string_view from C++17)
#   LumexExceptionsExceptionCxx17Tests  + LumexException.cxx17.tests.cpp:
#                                       std::string_view arguments
#   LumexExceptionsExceptionCxx20Tests  the same files at C++20
# LumexExceptionTestFixtures.hpp holds the fixture the files of every standard
# share. The exception suites share bin/crashes with the other directories of
# the module; the lock of the module's CMakeLists.txt covers them.
lumex_add_standard_suites(ExceptionsException
    MODULE exceptions
    LINK ${LUMEX_EXCEPTIONS_TEST_LINK}
    SOURCES LumexExceptionTestFixtures.hpp
    DISCOVER_ARGS
        PROPERTIES RESOURCE_LOCK lumex_exception_crash_dir
    TARGETS_VAR _exceptions_suites
)
lumex_exceptions_test_options(${_exceptions_suites})
