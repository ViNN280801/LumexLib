# Consumer fixture for cmake.consumer_third_party_coexistence (Linux).
#
# A process may hold two copies of Asio and hidapi: one inside another shared
# library (with its own embedded Asio and a static hidapi, default visibility)
# and one in the executable that links the LumexLib transport. On ELF their global symbols interpose, so the
# copies must be identical (transport specification F3). This fixture builds
# the same shape from the vendored trees: a shared library "other_library" with
# its own copy of both, and an executable that uses lumex::3rdparty_asio and
# lumex::3rdparty_hidapi; both run an io_context and enumerate HID devices in
# one process.

cmake_minimum_required(VERSION 3.16)
project(LumexThirdPartyCoexistenceConsumer LANGUAGES C CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message("LUMEX_CONSUMER_SKIP: the symbol interposition case is ELF/Linux only")
    return()
endif()
if(LUMEX_HIDAPI_BACKEND STREQUAL "libusb")
    message("LUMEX_CONSUMER_SKIP: the fixture models a library that uses hidraw")
    return()
endif()
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FIXTURE_UDEV QUIET IMPORTED_TARGET libudev)
endif()
if(NOT FIXTURE_UDEV_FOUND)
    message("LUMEX_CONSUMER_SKIP: libudev is not installed (hidraw backend of hidapi)")
    return()
endif()

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
lumex_setup_hidapi()

# The other shared library: its own Asio (same tree, same macros:
# ASIO_STANDALONE, ASIO_NO_DEPRECATED) and its own static hidapi compiled
# into it, all with default visibility.
find_package(Threads REQUIRED)
add_library(other_library SHARED
    other_library.cpp
    "${LUMEX_SOURCE_DIR}/3rdparty/hidapi/linux/hid.c")
target_include_directories(other_library SYSTEM PRIVATE
    "${LUMEX_SOURCE_DIR}/3rdparty/asio/include"
    "${LUMEX_SOURCE_DIR}/3rdparty/hidapi/hidapi")
target_compile_definitions(other_library PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_link_libraries(other_library PRIVATE Threads::Threads PkgConfig::FIXTURE_UDEV)
set_target_properties(other_library PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    C_VISIBILITY_PRESET default CXX_VISIBILITY_PRESET default)
target_compile_options(other_library PRIVATE -w)

add_executable(third_party_coexistence_consumer main.cpp)
target_link_libraries(third_party_coexistence_consumer PRIVATE
    other_library lumex::3rdparty_asio lumex::3rdparty_hidapi)
set_target_properties(third_party_coexistence_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
