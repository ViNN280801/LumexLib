# Fixture of cmake.consumer_hazard_pointer_static_destruction (see main.cpp).
#
# One executable whose static objects use hazard pointers in their
# constructors and destructors, and whose threads use them from thread-local
# destructors. The domain is constant-initialized and never destroyed, so none
# of this depends on the order of static initialization or destruction.

cmake_minimum_required(VERSION 3.16)
project(LumexHazardPointerStaticDestruction LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT GENERATORS REFLECTION STRING STRING_VIEW TEMPORARY TIME UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_OPTIONAL ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_SPAN ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_HAZARD_POINTER ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

add_executable(hazard_pointer_static_destruction_consumer main.cpp early.cpp)
target_include_directories(hazard_pointer_static_destruction_consumer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(hazard_pointer_static_destruction_consumer PRIVATE
    lumex::hazard_pointer)
set_target_properties(hazard_pointer_static_destruction_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
