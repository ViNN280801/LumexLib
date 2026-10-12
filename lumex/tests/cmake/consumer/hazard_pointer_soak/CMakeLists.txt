# Fixture of cmake.consumer_hazard_pointer_soak and
# cmake.consumer_hazard_pointer_tsan (see main.cpp).
#
# The long runs of the hazard pointer stress tests, outside the default test
# run: the ABA structures (stack, queue, list) over the immediate-reuse
# allocator, at 1, 2, 4, 8 and more than the cores' worth of threads, with a
# new printed seed in every round, for LUMEX_HP_SOAK_SECONDS seconds. With
# -DSANITIZE=thread the whole tree (the library included) is built with
# ThreadSanitizer. Registered only with LUMEX_BUILD_SOAK_TESTS.

cmake_minimum_required(VERSION 3.16)
project(LumexHazardPointerSoak LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

if(SANITIZE STREQUAL "thread")
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR WIN32)
        message("LUMEX_CONSUMER_SKIP: ThreadSanitizer needs GCC or Clang on Linux")
        return()
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g -O1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
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

add_executable(hazard_pointer_soak_consumer main.cpp)
target_include_directories(hazard_pointer_soak_consumer PRIVATE "${LUMEX_SOURCE_DIR}")
target_link_libraries(hazard_pointer_soak_consumer PRIVATE lumex::hazard_pointer)
set_target_properties(hazard_pointer_soak_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
