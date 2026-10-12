# lumex/tests/core/atomic/sync/CMakeLists.txt
#
# The tests of lumex/core/atomic/sync: the address-keyed wait and the two-bit
# lock under the lock-based smart pointers. One suite per standard of the
# module and per variant (lumex/tests/LumexTestStandards.cmake), named
# LumexAtomicSyncCxx11Tests, ...Cxx17Tests, ...Cxx20Tests,
# LumexAtomicSyncLockBasedCxx11Tests, LumexAtomicSyncLockBasedCxx20Tests,
# LumexAtomicSyncWaitTableCxx20Tests and the two LumexAtomicSyncNoLockFree...
# suites;
# the wait table variant forces the striped table the C++20 suite would
# otherwise leave to std::atomic::wait.
# LumexAtomicTestSupport.hpp of the module's directory holds what the tests
# share with smart_ptr/.
lumex_add_standard_suites(AtomicSync
    MODULE atomic
    LINK lumex::atomic
    SOURCES ../LumexAtomicTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300 LABELS tsan
    SOAK_FILTER *Soak*
    VARIANT lock_based DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED
    VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE
    VARIANT no_lock_free DEFINITIONS LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE
    VARIANT lock_free DEFINITIONS
        LUMEX_ATOMIC_TEST_SHARED_ENGINE=atomic_shared_ptr_lock_free
        LUMEX_ATOMIC_TEST_WEAK_ENGINE=atomic_weak_ptr_lock_free
        LUMEX_ATOMIC_TEST_ENGINE_REQUIRES_LOCK_FREE
    VARIANT lock_free_deferred DEFINITIONS
        LUMEX_ATOMIC_TEST_SHARED_ENGINE=atomic_shared_ptr_lock_free
        LUMEX_ATOMIC_TEST_WEAK_ENGINE=atomic_weak_ptr_lock_free
        "LUMEX_ATOMIC_TEST_ENGINE_ARGS=,lumex::core::atomic::smart_ptr::reclaim::deferred"
        LUMEX_ATOMIC_TEST_ENGINE_DEFERS_DESTRUCTION=1
        LUMEX_ATOMIC_TEST_ENGINE_REQUIRES_LOCK_FREE
    VARIANT std_backed DEFINITIONS
        LUMEX_ATOMIC_TEST_SHARED_ENGINE=atomic_shared_ptr_std_backed
        LUMEX_ATOMIC_TEST_WEAK_ENGINE=atomic_weak_ptr_std_backed
        LUMEX_ATOMIC_TEST_ENGINE_REQUIRES_STD_BACKED
)
