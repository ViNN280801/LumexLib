# Compile-check fixture for cmake.path_compile_checks.
#
# lumex::path converts to std::string only explicitly (MAJOR 2.0.0.0): the
# implicit operator made `std_path = lumex_path;` ambiguous on POSIX and, with
# libstdc++ 8, every direct initialization of a std::filesystem::path from a
# lumex path. check.cpp holds
#   LUMEX_PATH_GOOD_CASE        compiles: the explicit conversions, what still
#                               converts to a path implicitly and, from C++17,
#                               the copy initialization, the direct
#                               initialization, the list initialization and the
#                               assignment of a std::filesystem::path from a
#                               lumex path
#   LUMEX_PATH_BAD_CASE 1-7     fail to compile with a conversion error: a copy
#                               initialization, an argument by reference and by
#                               value, an assignment, a return statement, `+=`
#                               and a conditional expression, all of them
#                               needing the implicit conversion to std::string
# The checks run at C++11, C++17 and C++20. Everything runs at configure time
# (try_compile of a static library: the translation unit is compiled, not
# linked, so the library of filesystem is not needed).

cmake_minimum_required(VERSION 3.16)
project(LumexPathCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

# The compiler's messages are matched in English below.
set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_standards 14 17 20)
else()
    set(_werror -Werror)
    set(_standards 11 17 20)
endif()
set(_last_case 7)

function(_lumex_path_try result_var tag standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${_source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
        COMPILE_DEFINITIONS ${_werror} ${ARGN}
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_VARIABLE _output)
    set(${result_var} ${_ok} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${_output}" PARENT_SCOPE)
endfunction()

set(_checked 0)
foreach(_std IN LISTS _standards)
    set(_tag "cxx${_std}")
    _lumex_path_try(_ok good_${_tag} ${_std} -DLUMEX_PATH_GOOD_CASE)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_PATH_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()

    foreach(_case RANGE 1 ${_last_case})
        _lumex_path_try(_ok bad_${_case}_${_tag} ${_std}
                        -DLUMEX_PATH_BAD_CASE=${_case})
        if(_ok)
            message(FATAL_ERROR
                "LUMEX_PATH_BAD_CASE=${_case} (${_tag}) compiled; a lumex "
                "path converts to std::string implicitly again")
        endif()
        # A case must fail for its own reason, not because check.cpp has a
        # typo: the error must be a conversion error (GCC, Clang and MSVC
        # spell it differently), not the #error of an unknown case or a
        # missing name.
        if(_ok_OUTPUT MATCHES "is not one of the cases")
            message(FATAL_ERROR
                "LUMEX_PATH_BAD_CASE=${_case} is not defined in check.cpp")
        endif()
        if(_ok_OUTPUT MATCHES "not declared in this scope|undeclared identifier|use of undeclared|C2065")
            message(FATAL_ERROR
                "LUMEX_PATH_BAD_CASE=${_case} (${_tag}) fails on a missing "
                "name, not on the conversion\n${_ok_OUTPUT}")
        endif()
        if(NOT _ok_OUTPUT MATCHES
           "conversion from|cannot convert|could not convert|invalid initialization|no known conversion|no match for|no viable|no matching function|incompatible operand|different types|C2440|C2679|C2664|C2678|C2446|C2088")
            message(FATAL_ERROR
                "LUMEX_PATH_BAD_CASE=${_case} (${_tag}) fails, but not on a "
                "conversion\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "path compile checks: ${_checked} rejected cases as expected")
