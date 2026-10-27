# Fixture of cmake.consumer_hazard_pointer_throwing_new_only (see main.cpp).
#
# An executable that replaces only the throwing operator new and operator
# delete (with a block header, so a block that did not come from its operator
# new is detected) and links the hazard pointer library of the tree: the
# engine's own storage must not depend on how the program's allocator treats
# the nothrow forms, which a libstdc++ before GCC 9 does not route to a
# replaced operator new.

cmake_minimum_required(VERSION 3.16)
project(LumexHazardPointerThrowingNewOnly LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT FRAMING PROTOCOL GENERATORS INTERPROCESS REFLECTION STRING STRING_VIEW TEMPORARY TIME UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_OPTIONAL ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_SPAN ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_HAZARD_POINTER ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

find_package(Threads REQUIRED)

add_executable(hazard_pointer_throwing_new_only_consumer main.cpp)
target_link_libraries(hazard_pointer_throwing_new_only_consumer PRIVATE
    lumex::hazard_pointer Threads::Threads)
set_target_properties(hazard_pointer_throwing_new_only_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
