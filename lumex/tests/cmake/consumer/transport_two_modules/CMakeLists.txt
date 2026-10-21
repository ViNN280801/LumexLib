# Fixture of cmake.consumer_transport_two_modules (see main.cpp).
#
# An executable and a shared library with hidden visibility that both link
# the static transport library, with the other modules shared (serial is a
# DLL that exports its bounded open through LUMEX_SERIAL_API). The library
# runs a TCP stream on the executor of the executable's io_runner (one
# io_context for both binaries); the executable matches the code it returns
# with is_transport_error and sees that each binary has its own log slot.

cmake_minimum_required(VERSION 3.16)
project(LumexTransportTwoModules LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt HARDWARE JSON LOGGER LOGGING RESOURCE_MONITOR SETTINGS)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_TRANSPORT_WITH_SPAWN OFF CACHE BOOL "" FORCE)
set(LUMEX_TRANSPORT_WITH_TESTING OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_TRANSPORT ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

get_target_property(_serial_type LumexApplied_serial TYPE)
if(NOT _serial_type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR "serial is ${_serial_type}, the fixture needs a DLL")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(transport_module SHARED module.cpp)
target_include_directories(transport_module PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(transport_module PRIVATE lumex::transport)
target_compile_definitions(transport_module PRIVATE TRANSPORT_FIXTURE_BUILDING)

add_executable(transport_two_modules_consumer main.cpp)
target_include_directories(transport_two_modules_consumer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(transport_two_modules_consumer PRIVATE
    transport_module lumex::transport)
