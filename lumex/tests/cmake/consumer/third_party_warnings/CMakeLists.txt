# Consumer fixture for cmake.consumer_third_party_warnings.
#
# The third-party code of the transport layer must not produce a single warning
# in a consumer that is strict about its own code: one translation unit
# includes Asio (with spawn and Boost.Context) and hidapi through the
# lumex::3rdparty_* targets and is compiled with /W4 /WX (cl.exe; clang-cl
# maps /W4 to -WCL4) or -Wall -Wextra -Wpedantic -Werror (GCC, Clang). On Linux
# without libudev hidapi is left out of the unit (FIXTURE_NO_HIDAPI).

cmake_minimum_required(VERSION 3.16)
project(LumexThirdPartyWarningsConsumer LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(_with_hidapi ON)
if(NOT WIN32 AND NOT APPLE AND NOT LUMEX_HIDAPI_BACKEND STREQUAL "libusb")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FIXTURE_UDEV QUIET libudev)
    endif()
    if(NOT FIXTURE_UDEV_FOUND)
        set(_with_hidapi OFF)
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

lumex_setup_asio()
lumex_setup_boost_context()
if(_with_hidapi)
    lumex_setup_hidapi()
endif()

add_executable(third_party_warnings_consumer main.cpp)
target_link_libraries(third_party_warnings_consumer PRIVATE
    lumex::3rdparty_asio lumex::3rdparty_boost_context)
if(_with_hidapi)
    target_link_libraries(third_party_warnings_consumer PRIVATE lumex::3rdparty_hidapi)
else()
    target_compile_definitions(third_party_warnings_consumer PRIVATE FIXTURE_NO_HIDAPI)
endif()
set_target_properties(third_party_warnings_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(WIN32)
    target_compile_definitions(third_party_warnings_consumer PRIVATE _WIN32_WINNT=0x0A00)
endif()
if(MSVC)
    target_compile_options(third_party_warnings_consumer PRIVATE /W4 /WX)
else()
    target_compile_options(third_party_warnings_consumer PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
