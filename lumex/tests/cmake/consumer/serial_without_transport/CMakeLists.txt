# Fixture of cmake.consumer_serial_without_transport (see main.cpp).
#
# The dependency runs one way (transport requires serial, never the reverse):
# the serial module builds, links and runs with the transport module off, and
# without Asio anywhere in the build.

cmake_minimum_required(VERSION 3.16)
project(LumexSerialWithoutTransport LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        HARDWARE JSON LOGGER LOGGING RESOURCE_MONITOR SETTINGS TRANSPORT)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

if(TARGET lumex_3rdparty_asio OR TARGET lumex::transport)
    message(FATAL_ERROR "transport or Asio is configured although LUMEX_BUILD_TRANSPORT is OFF")
endif()

add_executable(serial_without_transport_consumer main.cpp)
target_link_libraries(serial_without_transport_consumer PRIVATE lumex::serial)