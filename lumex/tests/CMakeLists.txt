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
