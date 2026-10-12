# Fixture of cmake.consumer_atomic_lock_free_hooks (see main.cpp).
#
# Builds the hazard pointer engine and the lock-free atomic smart pointers
# into the test executable, the engine with LUMEX_HAZARD_POINTER_TEST_HOOKS and
# the cell with LUMEX_ATOMIC_SMART_PTR_TEST_HOOKS: threads are stalled at named
# places (a reader that announced a box, a load that protected a box, a
# compare-exchange that found the value equivalent) and the global allocator
# hands a freed block out again at once, so the ABA and use-after-reclaim
# interleavings of the box engine are deterministic. The library of the tree
# is not linked (HAZARD_POINTER is switched off, the way
# hazard_pointer_hooks does it) and the detection macro is defined here.

cmake_minimum_required(VERSION 3.16)
project(LumexAtomicLockFreeHooks LANGUAGES CXX)

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
set(LUMEX_BUILD_ATOMIC ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

find_package(Threads REQUIRED)

add_executable(atomic_lock_free_hooks_consumer
    main.cpp
    "${LUMEX_SOURCE_DIR}/lumex/core/hazard_pointer/engine/LumexHazardPointerDomain.cpp")
target_compile_definitions(atomic_lock_free_hooks_consumer PRIVATE
    LUMEX_HAZARD_POINTER_TEST_HOOKS
    LumexCore_hazard_pointer_EXPORTS
    LUMEX_ATOMIC_HAS_HAZARD_POINTER=1)
target_link_libraries(atomic_lock_free_hooks_consumer PRIVATE
    lumex::atomic lumex::utility lumex::span Threads::Threads)
set_target_properties(atomic_lock_free_hooks_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
