# Consumer fixture for cmake.consumer_third_party_reuse.
#
# A consumer that already defines lumex_3rdparty_asio (for example to share one
# Asio tree with its own code) keeps it: lumex_setup_asio () must reuse the
# existing target instead of defining a second one. The marker definition below
# reaches main.cpp only through the consumer's own target. A second call of each
# setup function must be a no-op.

cmake_minimum_required(VERSION 3.16)
project(LumexThirdPartyReuseConsumer LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

find_package(Threads REQUIRED)
add_library(lumex_3rdparty_asio INTERFACE)
add_library(lumex::3rdparty_asio ALIAS lumex_3rdparty_asio)
target_include_directories(lumex_3rdparty_asio SYSTEM INTERFACE
    "${LUMEX_SOURCE_DIR}/3rdparty/asio/include")
target_compile_definitions(lumex_3rdparty_asio INTERFACE
    ASIO_STANDALONE ASIO_NO_DEPRECATED FIXTURE_CONSUMER_OWNS_ASIO=1)
target_link_libraries(lumex_3rdparty_asio INTERFACE Threads::Threads)
if(WIN32)
    target_link_libraries(lumex_3rdparty_asio INTERFACE ws2_32 mswsock)
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT FRAMING GENERATORS REFLECTION STRING STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

lumex_setup_asio()
lumex_setup_asio()
lumex_setup_boost_context()
lumex_setup_boost_context()

add_executable(third_party_reuse_consumer main.cpp)
target_link_libraries(third_party_reuse_consumer PRIVATE
    lumex::3rdparty_asio lumex::3rdparty_boost_context)
set_target_properties(third_party_reuse_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(WIN32)
    target_compile_definitions(third_party_reuse_consumer PRIVATE _WIN32_WINNT=0x0A00)
endif()
