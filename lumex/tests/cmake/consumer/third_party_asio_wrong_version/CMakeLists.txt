# Consumer fixture for cmake.consumer_third_party_asio_wrong_version.
#
# LUMEX_ASIO_DIR names an include directory whose asio/version.hpp is not
# 1.36.0. lumex_setup_asio () must stop the configure with a message that names
# the found version (the test expects "LumexLib needs Asio"): a second, different
# Asio next to DChannel's copy in one process is an ODR violation.

cmake_minimum_required(VERSION 3.16)
project(LumexThirdPartyAsioWrongVersion LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(_fake "${CMAKE_CURRENT_BINARY_DIR}/fake_asio")
file(WRITE "${_fake}/asio/version.hpp"
    "#define ASIO_VERSION 103500 // 1.35.0\n")
set(LUMEX_ASIO_DIR "${_fake}" CACHE PATH "" FORCE)

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT FRAMING PROTOCOL GENERATORS INTERPROCESS REFLECTION STRING STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

lumex_setup_asio()

# Not reached: the configure stops above.
add_executable(third_party_asio_wrong_version_consumer main.cpp)
