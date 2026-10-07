# lumex/tests/core/exceptions/crash/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexExceptionsCrashCxx11Tests builds WindowsSEHTranslator.cxx11.tests.cpp,
# and the Cxx17 and Cxx20 suites run the same file.
lumex_add_standard_suites(ExceptionsCrash
    MODULE exceptions
    LINK ${LUMEX_EXCEPTIONS_TEST_LINK}
    DISCOVER_ARGS
        PROPERTIES RESOURCE_LOCK lumex_exception_crash_dir
    TARGETS_VAR _exceptions_suites
)
lumex_exceptions_test_options(${_exceptions_suites})
