# Fixture of cmake.consumer_unicode_warnings (see main.cpp).
#
# The UTF-8 validation is header-only, so the library build never compiles it
# with a strict consumer's flags. One translation unit calls every overload and
# every vector routine that the build has, and is compiled at C++11 and C++20,
# in the mode with the vector routines and in the mode of
# LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD, for the validation header alone and
# for the unicode umbrella, with: /W4 /WX (cl.exe), -Weverything without the
# C++98 compatibility groups and /WX (clang-cl and Clang), and the warning set
# of the Lumex targets with -Werror and -Wuseless-cast, -Wcast-align and
# -Wcast-qual (GCC).

cmake_minimum_required(VERSION 3.16)
project(LumexUnicodeWarnings LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS FILESYSTEM
        FMT FRAMING PROTOCOL UNITS GENERATORS INTERPROCESS REFLECTION STRING
        STRING_VIEW TEMPORARY TIME EXPECTED)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_UNICODE ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(_strict /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(_strict -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-c++98-compat-local-type-template-args -Wno-pre-c++14-compat
        -Wno-pre-c++17-compat -Wno-padded)
    if(MSVC)
        list(APPEND _strict /WX)
    else()
        list(APPEND _strict -Werror)
    endif()
else()
    set(_strict -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
        -Wshadow -Wold-style-cast -Wcast-qual -Wcast-align -Wuseless-cast -Wundef
        -Wswitch-default -Wswitch-enum -Wnon-virtual-dtor -Woverloaded-virtual
        -Werror)
endif()

function(add_case target std)
    add_executable(${target} main.cpp)
    target_link_libraries(${target} PRIVATE lumex::unicode)
    target_compile_options(${target} PRIVATE ${_strict})
    target_compile_definitions(${target} PRIVATE ${ARGN})
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD ${std} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endfunction()

# The umbrella also includes utf/LumexUtf.hpp, whose utf32_decoder casts a
# std::uint32_t expression to std::uint32_t at two places (GCC -Wuseless-cast).
# That is not the header under test here: GCC compiles the umbrella cases
# without -Wuseless-cast until it is fixed, and the cases of the validation
# header alone keep it.
function(add_umbrella_case target std)
    add_case(${target} ${std} LUMEX_FIXTURE_UNICODE_UMBRELLA ${ARGN})
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -Wno-useless-cast)
    endif()
endfunction()

foreach(_std 11 20)
    add_case(unicode_warnings_validate_cxx${_std} ${_std})
    add_case(unicode_warnings_validate_no_simd_cxx${_std} ${_std}
        LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD)
    add_umbrella_case(unicode_warnings_umbrella_cxx${_std} ${_std})
    add_umbrella_case(unicode_warnings_umbrella_no_simd_cxx${_std} ${_std}
        LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD)
endforeach()

# run_consumer.cmake runs <CASE>_consumer: the C++11 build with the vector
# routines.
add_case(unicode_warnings_consumer 11)
