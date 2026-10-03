# lumex/core/atomic/CMakeLists.txt
#
# Header-only. atomic_shared_ptr / atomic_weak_ptr use std::thread,
# std::mutex and std::condition_variable before C++20; before glibc 2.34
# those need libpthread, so Threads::Threads is an INTERFACE dependency.

set(LUMEX_ATOMIC_NAME LumexCore_atomic)
set(EXPORT_TO LumexCoreTargets)

find_package(Threads REQUIRED)

add_library(${LUMEX_ATOMIC_NAME} INTERFACE)
add_library(lumex::atomic ALIAS ${LUMEX_ATOMIC_NAME})

target_include_directories(${LUMEX_ATOMIC_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_ATOMIC_NAME} INTERFACE
    Threads::Threads
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_ATOMIC_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/atomic
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "LumexAtomic"
    )
endif()
