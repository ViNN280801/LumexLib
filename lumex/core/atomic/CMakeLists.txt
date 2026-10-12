# lumex/core/atomic/CMakeLists.txt
#
# Header-only. atomic_shared_ptr / atomic_weak_ptr use std::thread,
# std::mutex and std::condition_variable before C++20; before glibc 2.34
# those need libpthread, so Threads::Threads is an INTERFACE dependency.
#
# The lock-free engine (atomic_shared_ptr_lock_free) is built on the compiled
# library lumex::hazard_pointer. The edge is soft: when that target exists,
# lumex::atomic links it and defines LUMEX_ATOMIC_HAS_HAZARD_POINTER (the one
# input of the detection in LumexAtomicSmartPtrConfig.hpp); without it
# (LUMEX_BUILD_HAZARD_POINTER=OFF) the module is lock-based only. There is no
# lumex_require_module line on purpose, and core/CMakeLists.txt adds
# hazard_pointer before atomic so the target exists here.

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

if(TARGET lumex::hazard_pointer)
    target_link_libraries(${LUMEX_ATOMIC_NAME} INTERFACE lumex::hazard_pointer)
    target_compile_definitions(${LUMEX_ATOMIC_NAME} INTERFACE
        LUMEX_ATOMIC_HAS_HAZARD_POINTER=1)
endif()

if(TARGET lumex::smart_ptr)
    target_link_libraries(${LUMEX_ATOMIC_NAME} INTERFACE lumex::smart_ptr)
    target_compile_definitions(${LUMEX_ATOMIC_NAME} INTERFACE
        LUMEX_ATOMIC_HAS_SMART_PTR=1)
endif()

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
