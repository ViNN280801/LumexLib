# lumex/tests/core/exceptions/stacktrace/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexExceptionsStacktraceCxx11Tests builds LumexStacktrace.cxx11.tests.cpp,
# and the Cxx17 and Cxx20 suites run the same file. The stack trace tests
# search the captured text for function names, so the suites get the options
# of lumex_exceptions_test_options (the module's CMakeLists.txt).
lumex_add_standard_suites(ExceptionsStacktrace
    MODULE exceptions
    LINK ${LUMEX_EXCEPTIONS_TEST_LINK}
    DISCOVER_ARGS
        PROPERTIES RESOURCE_LOCK lumex_exception_crash_dir
    TARGETS_VAR _exceptions_suites
)
lumex_exceptions_test_options(${_exceptions_suites})
