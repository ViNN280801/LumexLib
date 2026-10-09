# Fixture of cmake.consumer_contracts_two_modules (see main.cpp).
#
# A shared library with hidden visibility and an executable, both linking the
# shared contracts library: the violation handler must be one per process (a
# header-only slot would be one copy per DLL on Windows), so the handler that
# the executable installs receives the violations of the library and the
# handler the library installs is the one the executable reads back.

cmake_minimum_required(VERSION 3.16)
project(LumexContractsTwoModules LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT GENERATORS HAZARD_POINTER REFLECTION STRING STRING_VIEW TEMPORARY
        TIME UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt CONTRACTS MATH OPTIONAL SPAN UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
# The assertions of the fixture are observed, whatever the default is.
set(LUMEX_CONTRACTS_SEMANTIC OBSERVE CACHE STRING "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(contracts_module SHARED module.cpp)
target_include_directories(contracts_module PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(contracts_module PRIVATE lumex::contracts)
target_compile_definitions(contracts_module PRIVATE CONTRACTS_FIXTURE_BUILDING)

add_executable(contracts_two_modules_consumer main.cpp)
target_include_directories(contracts_two_modules_consumer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(contracts_two_modules_consumer PRIVATE
    contracts_module lumex::contracts)
