# lumex/tests/core/atomic/CMakeLists.txt
#
# atomic_shared_ptr / atomic_weak_ptr work from C++11 (the lock-based
# implementation) and wrap std::atomic<std::shared_ptr<T>> where the standard
# library has it; wait () sleeps through std::atomic::wait from C++20 and on a
# striped mutex / condition variable table otherwise. Every combination gets
# its own suite, built from the same sources:
#   LumexAtomicTests               C++11 - lock-based, table
#   LumexAtomicCxx17Tests          C++17 - lock-based, table
#   LumexAtomicCxx20Tests          C++20 - std::atomic<std::shared_ptr<T>> where
#                                  the library has it (libstdc++ 12+, MSVC),
#                                  lock-based otherwise (libc++);
#                                  std::atomic::wait
#   LumexAtomicLockBasedCxx20Tests C++20 - lock-based (forced), std::atomic::wait
#   LumexAtomicWaitTableCxx20Tests C++20 - as LumexAtomicCxx20Tests, with the
#                                  table (forced)
# LumexAtomicGlobalNames.tests.cpp declares global atomic_shared_ptr and
# atomic_weak_ptr templates of its own: every suite stops building if the
# module declares either name at global scope.
# CTest names carry the directory prefix "atomic." and the suffix of the
# suite. A lost wake-up hangs a test; the in-test watchdog aborts it after
# 120 s and the CTest timeout below is the second line of defence.

set(LUMEX_ATOMIC_TEST_SOURCES
    LumexAtomicWait.tests.cpp
    LumexAtomicSharedPtr.tests.cpp
    LumexAtomicWeakPtr.tests.cpp
    LumexAtomicSmartPtrKinds.tests.cpp
    LumexAtomicSmartPtrLifetime.tests.cpp
    LumexAtomicSmartPtrWaitNotify.tests.cpp
    LumexAtomicSmartPtrConcurrency.tests.cpp
    LumexAtomicSmartPtrConfig.tests.cpp
    LumexAtomicSmartPtrPerf.tests.cpp
    LumexAtomicGlobalNames.tests.cpp
)

# name;standard;CTest suffix;forcing definition
foreach(_atomic_suite IN ITEMS
        "LumexAtomicTests;11;;"
        "LumexAtomicCxx17Tests;17;.cxx17;"
        "LumexAtomicCxx20Tests;20;.cxx20;"
        "LumexAtomicLockBasedCxx20Tests;20;.lock_based.cxx20;LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED"
        "LumexAtomicWaitTableCxx20Tests;20;.wait_table.cxx20;LUMEX_ATOMIC_WAIT_FORCE_TABLE")
    list(GET _atomic_suite 0 _atomic_name)
    list(GET _atomic_suite 1 _atomic_std)
    list(GET _atomic_suite 2 _atomic_suffix)
    list(GET _atomic_suite 3 _atomic_force)

    add_executable(${_atomic_name} ${LUMEX_ATOMIC_TEST_SOURCES})

    target_link_libraries(
        ${_atomic_name}
        PRIVATE
        lumex::atomic
    )

    if(_atomic_force)
        target_compile_definitions(${_atomic_name} PRIVATE ${_atomic_force})
    endif()

    lumex_test_use_gtest(${_atomic_name} CXX_STANDARD ${_atomic_std})
    if(_atomic_suffix)
        lumex_gtest_discover_tests(${_atomic_name}
            TEST_SUFFIX "${_atomic_suffix}"
            PROPERTIES TIMEOUT 300)
    else()
        lumex_gtest_discover_tests(${_atomic_name}
            PROPERTIES TIMEOUT 300)
    endif()
endforeach()
