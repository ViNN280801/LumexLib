# Consumer fixture for cmake.consumer_expected_no_exceptions_*.
#
# expected built without exceptions: value () of an object that holds an error
# calls std::terminate, and the operations that restore the old contents when
# a constructor throws (emplace, assignment, swap) have no try block.
#
# Two ways to switch the exceptions off, one executable each case:
#   -DEXPECTED_NO_EXCEPTIONS_MODE=compiler  -fno-exceptions (GCC, Clang): the
#                                           real -fno-exceptions build; the
#                                           library detects it
#   -DEXPECTED_NO_EXCEPTIONS_MODE=macro     exceptions stay on and the program
#                                           defines LUMEX_EXPECTED_HAS_EXCEPTIONS
#                                           as 0 (the only way on MSVC here)
# -DEXPECTED_NO_EXCEPTIONS_STD=<11|17|20> sets the language standard.
# main.cpp runs the checks (a failure exits with 1) and, on POSIX, forks one
# child for each call that must terminate.

cmake_minimum_required(VERSION 3.16)
project(LumexExpectedNoExceptions LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT EXPECTED_NO_EXCEPTIONS_MODE)
    set(EXPECTED_NO_EXCEPTIONS_MODE compiler)
endif()
if(NOT EXPECTED_NO_EXCEPTIONS_STD)
    set(EXPECTED_NO_EXCEPTIONS_STD 11)
endif()
if(MSVC AND EXPECTED_NO_EXCEPTIONS_MODE STREQUAL "compiler")
    message("LUMEX_CONSUMER_SKIP: MSVC has no -fno-exceptions; the macro mode covers it")
    return()
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS FILESYSTEM
        FMT FRAMING PROTOCOL GENERATORS INTERPROCESS REFLECTION STRING STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_EXPECTED ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

add_executable(expected_no_exceptions_consumer main.cpp)
target_link_libraries(expected_no_exceptions_consumer PRIVATE lumex::expected)
set_target_properties(expected_no_exceptions_consumer PROPERTIES
    CXX_STANDARD ${EXPECTED_NO_EXCEPTIONS_STD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
if(EXPECTED_NO_EXCEPTIONS_MODE STREQUAL "compiler")
    target_compile_options(expected_no_exceptions_consumer PRIVATE -fno-exceptions)
    target_compile_definitions(expected_no_exceptions_consumer PRIVATE
        EXPECTED_NO_EXCEPTIONS_COMPILER=1)
elseif(EXPECTED_NO_EXCEPTIONS_MODE STREQUAL "macro")
    target_compile_definitions(expected_no_exceptions_consumer PRIVATE
        LUMEX_EXPECTED_HAS_EXCEPTIONS=0 EXPECTED_NO_EXCEPTIONS_MACRO=1)
else()
    message(FATAL_ERROR "EXPECTED_NO_EXCEPTIONS_MODE: compiler or macro")
endif()
