# Consumer fixture for LumexCMake.consumer_reflection_umbrella_*.
#
# Embeds LumexLib with only the modules lumex::reflection needs and builds one
# executable that includes the reflection umbrella and links lumex::reflection
# alone, without any nlohmann. It must build with LUMEX_WITH_FIELD_REFLECTION
# both OFF and ON: the option may not leak field reflection (and its
# <nlohmann/json.hpp> include) into consumers.

cmake_minimum_required(VERSION 3.16)
project(LumexReflectionConsumerSmoke LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        GENERATORS OPTIONAL STRING STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_REFLECTION ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

add_executable(reflection_umbrella_consumer main.cpp)
target_link_libraries(reflection_umbrella_consumer PRIVATE lumex::reflection)
set_target_properties(reflection_umbrella_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)
