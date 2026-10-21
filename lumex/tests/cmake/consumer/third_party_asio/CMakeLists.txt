# Consumer fixture for cmake.consumer_third_party_asio_cxx11 / _cxx20.
#
# The vendored standalone Asio (cmake/LumexAsio.cmake): the consumer embeds
# LumexLib with the smallest module set, calls lumex_setup_asio () and builds
# a program that runs an io_context, a timer and a TCP round trip over the
# loopback, at C++11 or C++20 (FIXTURE_CXX_STANDARD). With LUMEX_ASIO_DIR the
# fixture checks the override path instead of the vendored default.

cmake_minimum_required(VERSION 3.16)
project(LumexThirdPartyAsioConsumer LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT FIXTURE_CXX_STANDARD)
    set(FIXTURE_CXX_STANDARD 11)
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

add_executable(third_party_asio_consumer main.cpp)
target_link_libraries(third_party_asio_consumer PRIVATE lumex::3rdparty_asio)
set_target_properties(third_party_asio_consumer PROPERTIES
    CXX_STANDARD ${FIXTURE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
if(WIN32)
    # Windows 10, so Asio does not ask for _WIN32_WINNT.
    target_compile_definitions(third_party_asio_consumer PRIVATE _WIN32_WINNT=0x0A00)
endif()
