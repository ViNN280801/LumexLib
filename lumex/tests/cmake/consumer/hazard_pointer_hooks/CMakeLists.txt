# Fixture of cmake.consumer_hazard_pointer_hooks (see main.cpp).
#
# Builds the engine of core/hazard_pointer a second time, into the test
# executable, with LUMEX_HAZARD_POINTER_TEST_HOOKS: the engine and the reader
# then call a handler at named points, and main.cpp stalls threads there to
# make the Dekker race of the protocol deterministic (a reader that announced
# and waits, a pass that read the slots and waits). The library of the tree is
# not linked, so the two engines never meet.

cmake_minimum_required(VERSION 3.16)
project(LumexHazardPointerHooks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT GENERATORS HAZARD_POINTER REFLECTION STRING STRING_VIEW TEMPORARY
        TIME UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_OPTIONAL ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_SPAN ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

find_package(Threads REQUIRED)

add_executable(hazard_pointer_hooks_consumer
    main.cpp
    "${LUMEX_SOURCE_DIR}/lumex/core/hazard_pointer/engine/LumexHazardPointerDomain.cpp")
target_compile_definitions(hazard_pointer_hooks_consumer PRIVATE
    LUMEX_HAZARD_POINTER_TEST_HOOKS
    LumexCore_hazard_pointer_EXPORTS)
target_link_libraries(hazard_pointer_hooks_consumer PRIVATE
    lumex::utility lumex::span Threads::Threads)
set_target_properties(hazard_pointer_hooks_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
