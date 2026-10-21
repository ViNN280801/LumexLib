# Fixture of cmake.consumer_framing_two_modules (see main.cpp).
#
# A shared library with hidden visibility and an executable, both linking the
# shared framing library: an error code made in the library must compare equal
# to the framing_errc enumerator in the executable. A std::error_category is
# compared by address, so a header-only category (one copy per DLL on
# Windows) would make them differ.

cmake_minimum_required(VERSION 3.16)
project(LumexFramingTwoModules LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        ATOMIC BASE64 CIRCULAR_BUFFER ENVIRONMENT EXCEPTIONS FILESYSTEM FMT
        GENERATORS HAZARD_POINTER REFLECTION SMART_PTR STRING TEMPORARY TIME
        UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt
        CONTRACTS CRC EXPECTED FRAMING MATH OPTIONAL SPAN STRING_VIEW UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(framing_module SHARED module.cpp)
target_include_directories(framing_module PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(framing_module PRIVATE lumex::framing)
target_compile_definitions(framing_module PRIVATE FRAMING_FIXTURE_BUILDING)

add_executable(framing_two_modules_consumer main.cpp)
target_include_directories(framing_two_modules_consumer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(framing_two_modules_consumer PRIVATE
    framing_module lumex::framing)
