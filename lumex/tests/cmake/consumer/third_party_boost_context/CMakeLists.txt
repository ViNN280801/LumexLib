# Consumer fixture for cmake.consumer_third_party_boost_context.
#
# The vendored Boost.Context (cmake/LumexBoostContext.cmake: winfib on Windows,
# ucontext elsewhere) under asio::spawn: two stackful coroutines take turns
# through a steady_timer at C++11. Under LUMEX_USE_ASAN the ucontext backend
# gets BOOST_USE_ASAN.

cmake_minimum_required(VERSION 3.16)
project(LumexThirdPartyBoostContextConsumer LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
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

add_executable(third_party_boost_context_consumer main.cpp)
target_link_libraries(third_party_boost_context_consumer PRIVATE
    lumex::3rdparty_asio lumex::3rdparty_boost_context)
set_target_properties(third_party_boost_context_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(WIN32)
    target_compile_definitions(third_party_boost_context_consumer PRIVATE _WIN32_WINNT=0x0A00)
endif()
if(COMMAND lumex_apply_sanitizers)
    lumex_apply_sanitizers(third_party_boost_context_consumer)
endif()
