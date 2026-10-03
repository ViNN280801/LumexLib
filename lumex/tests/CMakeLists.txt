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

# Two vendored GoogleTest trees (not FetchContent):
#   1.12.1 for C++11/14 tests, 1.18.0 for C++17+ tests.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/LumexGoogleTest.cmake")
include(GoogleTest)

lumex_add_subdirectory_if(LUMEX_BUILD_APPLIED applied)
lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)
lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)
