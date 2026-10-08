# lumex/tests/CMakeLists.txt

# CMake-file tests do not need GoogleTest. Register them even when every
# library group is OFF so -DLUMEX_BUILD_TESTS=ON still has a suite.
add_subdirectory(cmake)

# Include order of every source under lumex/ (quality-gates section 7). The
# checker runs on Python 3.7 and later; without an interpreter the test is
# not registered.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME lint.include_order
        COMMAND ${Python3_EXECUTABLE}
            ${LumexLib_SOURCE_DIR}/Scripts/CodeTools/check_include_order.py
            --check --dir ${LumexLib_SOURCE_DIR}/lumex
    )
    set_tests_properties(lint.include_order PROPERTIES LABELS "lint")
else()
    message(STATUS "Skipping lint.include_order: no Python 3 interpreter")
endif()

# Every public header and module umbrella under lumex/ has a Doxygen @file
# block: without one Doxygen documents none of the file's free functions,
# macros or typedefs. Same Python 3.7+ interpreter as the include order.
if(Python3_Interpreter_FOUND)
    add_test(NAME lint.file_blocks
        COMMAND ${Python3_EXECUTABLE}
            ${LumexLib_SOURCE_DIR}/Scripts/CodeTools/check_file_blocks.py
            --check --dir ${LumexLib_SOURCE_DIR}/lumex
    )
    set_tests_properties(lint.file_blocks PROPERTIES LABELS "lint")
else()
    message(STATUS "Skipping lint.file_blocks: no Python 3 interpreter")
endif()

# No public header under lumex/ writes `using namespace` at file scope:
# there the directive puts the names of the library into the global
# namespace of every consumer. Same Python 3.7+ interpreter as above.
if(Python3_Interpreter_FOUND)
    add_test(NAME lint.header_using_directives
        COMMAND ${Python3_EXECUTABLE}
            ${LumexLib_SOURCE_DIR}/Scripts/CodeTools/check_header_using.py
            --check --dir ${LumexLib_SOURCE_DIR}/lumex
    )
    set_tests_properties(lint.header_using_directives PROPERTIES
        LABELS "lint")
else()
    message(STATUS
        "Skipping lint.header_using_directives: no Python 3 interpreter")
endif()

# No `#pragma warning` or `#pragma comment` outside _MSC_VER: GCC and MinGW
# report them as unknown pragmas, which stops a -Werror build. Same Python
# 3.7+ interpreter as above.
if(Python3_Interpreter_FOUND)
    add_test(NAME lint.msvc_pragmas
        COMMAND ${Python3_EXECUTABLE}
            ${LumexLib_SOURCE_DIR}/Scripts/CodeTools/check_msvc_pragmas.py
            --check --dir ${LumexLib_SOURCE_DIR}/lumex
    )
    set_tests_properties(lint.msvc_pragmas PROPERTIES LABELS "lint")
else()
    message(STATUS "Skipping lint.msvc_pragmas: no Python 3 interpreter")
endif()

# Every public source file under lumex/ starts with the MIT notice in a plain
# /* comment: a Doxygen /** comment would copy the license onto every page.
if(Python3_Interpreter_FOUND)
    add_test(NAME lint.license_headers
        COMMAND ${Python3_EXECUTABLE}
            ${LumexLib_SOURCE_DIR}/Scripts/CodeTools/check_license_headers.py
            --check --dir ${LumexLib_SOURCE_DIR}/lumex
    )
    set_tests_properties(lint.license_headers PROPERTIES LABELS "lint")
else()
    message(STATUS "Skipping lint.license_headers: no Python 3 interpreter")
endif()

# Every public header under lumex/ compiles on its own: a translation unit with
# the single #include of it, at C++11, with the compiler and the C++ flags of
# this build (the C++ standard library too: -stdlib= of the vendored GoogleTest
# target is the one the Lumex targets use). A header that fails alone relies on
# a neighbour to have included something first. Four parallel compiles; about
# 10 seconds on GCC 13.2. Exit code 77 (cl or clang-cl outside a developer
# shell) is reported as skipped.
if(Python3_Interpreter_FOUND)
    add_test(NAME lint.headers_standalone
        COMMAND ${Python3_EXECUTABLE}
            ${LumexLib_SOURCE_DIR}/Scripts/CodeTools/check_headers_standalone.py
            --check --dir ${LumexLib_SOURCE_DIR}/lumex
            --compiler ${CMAKE_CXX_COMPILER} --std 11 --jobs 4
            "--cxx-flags=${CMAKE_CXX_FLAGS} $<FILTER:$<TARGET_PROPERTY:lumex_gtest_1_12,COMPILE_OPTIONS>,INCLUDE,^-stdlib=>"
    )
    set_tests_properties(lint.headers_standalone PROPERTIES
        LABELS "lint"
        PROCESSORS 4
        TIMEOUT 900
        SKIP_RETURN_CODE 77)
else()
    message(STATUS "Skipping lint.headers_standalone: no Python 3 interpreter")
endif()

# Two vendored GoogleTest trees (not FetchContent):
#   1.12.1 for C++11/14 tests, 1.18.0 for C++17+ tests.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/LumexGoogleTest.cmake")
include(GoogleTest)

# The C++ standards of every test module (one suite per module and standard),
# read by lumex_add_standard_suites.
include("${CMAKE_CURRENT_LIST_DIR}/LumexTestStandards.cmake")

lumex_add_subdirectory_if(LUMEX_BUILD_APPLIED applied)
lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)
lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)
