# Consumer fixture for LumexCMake.consumer_cplusplus_macro.
#
# LumexLib headers select code with `#if __cplusplus`. MSVC reports
# 199711L there unless /Zc:__cplusplus is on, so the library targets carry
# that flag as an INTERFACE compile option. This consumer embeds LumexLib,
# links lumex::utility and sets no compiler flag of its own: the headers must
# still see the real standard.

cmake_minimum_required(VERSION 3.16)
project(LumexCplusplusMacroConsumer LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT GENERATORS OPTIONAL REFLECTION STRING STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

add_executable(cplusplus_macro_consumer main.cpp)
target_link_libraries(cplusplus_macro_consumer PRIVATE lumex::utility)
set_target_properties(cplusplus_macro_consumer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
