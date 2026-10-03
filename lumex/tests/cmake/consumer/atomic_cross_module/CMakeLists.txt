# Fixture of cmake.consumer_atomic_cross_module (see main.cpp).
#
# Builds two shared libraries with hidden visibility and an executable at
# C++11, where the lock-based atomic smart pointers sleep on the striped table
# of lumex/core/atomic/sync/LumexAtomicWait.hpp. Windows DLLs cannot share
# that table (documented); the case reports itself as skipped there.

cmake_minimum_required(VERSION 3.16)
project(LumexAtomicCrossModule LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

if(WIN32)
    message("LUMEX_CONSUMER_SKIP: Windows DLLs keep one waiting table each")
    return()
endif()

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

foreach(_lib IN ITEMS sleeper waker)
    add_library(atomic_cross_${_lib} SHARED ${_lib}.cpp)
    target_include_directories(atomic_cross_${_lib} PRIVATE
        "${LUMEX_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(atomic_cross_${_lib} PRIVATE Threads::Threads)
endforeach()

add_executable(atomic_cross_module_consumer main.cpp)
target_include_directories(atomic_cross_module_consumer PRIVATE
    "${LUMEX_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(atomic_cross_module_consumer PRIVATE
    atomic_cross_sleeper atomic_cross_waker Threads::Threads)
