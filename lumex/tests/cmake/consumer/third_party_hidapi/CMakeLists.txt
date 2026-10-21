# Consumer fixture for cmake.consumer_third_party_hidapi.
#
# The vendored hidapi 0.15.0 (cmake/LumexHidapi.cmake): the consumer calls the
# lumex_setup_hidapi macro from file scope (it enables C) and runs an
# enumeration of all HID devices; no device is needed and hid_exit is never
# called. On Linux without libudev (hidraw backend) the case is skipped.

cmake_minimum_required(VERSION 3.16)
project(LumexThirdPartyHidapiConsumer LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

if(NOT WIN32 AND NOT APPLE AND NOT LUMEX_HIDAPI_BACKEND STREQUAL "libusb")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FIXTURE_UDEV QUIET libudev)
    endif()
    if(NOT FIXTURE_UDEV_FOUND)
        message("LUMEX_CONSUMER_SKIP: libudev is not installed (hidraw backend of hidapi)")
        return()
    endif()
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT GENERATORS REFLECTION STRING STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

lumex_setup_hidapi()

add_executable(third_party_hidapi_consumer main.cpp)
target_link_libraries(third_party_hidapi_consumer PRIVATE lumex::3rdparty_hidapi)
set_target_properties(third_party_hidapi_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
