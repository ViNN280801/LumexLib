# lumex/tests/core/atomic/smart_ptr/CMakeLists.txt
#
# The tests of lumex/core/atomic/smart_ptr: atomic_shared_ptr and
# atomic_weak_ptr, their configuration and the wait / notify built on them.
# One suite per standard of the module and per variant
# (lumex/tests/LumexTestStandards.cmake), every one built from the files of
# its standard and the lower ones:
#   LumexAtomicSmartPtrCxx11Tests          every *.cxx11.tests.cpp, the common
#                                          names (the lock-free engine where
#                                          lumex::hazard_pointer is linked)
#   LumexAtomicSmartPtrCxx17Tests          + LumexAtomicSmartPtrKinds.cxx17
#                                          (weak_from_this, shared_ptr<T[]>)
#   LumexAtomicSmartPtrCxx20Tests          + the *.cxx20.tests.cpp files
#                                          (make_shared<T[]>, std::latch,
#                                          std::jthread, the differential run
#                                          against std::atomic<std::shared_ptr
#                                          <T>>)
#   LumexAtomicSmartPtrLockBasedCxx11Tests the lock-based engine forced, at
#   LumexAtomicSmartPtrLockBasedCxx20Tests C++11 and C++20
#   LumexAtomicSmartPtrWaitTableCxx20Tests C++20 with the wait table forced
#   LumexAtomicSmartPtrNoLockFreeCxx11Tests the build without the lock-free
#   LumexAtomicSmartPtrNoLockFreeCxx20Tests engine (the branch where only the
#                                          lock-based engine exists)
# LumexAtomicTestSupport.hpp of the module's directory holds what they share.
# The split-count engine has its own module key and its own directory,
# split_count/ (added at the end of this file).
#
# The suites are engine-parametrized. The tests reach the atomic smart pointer
# through two alias templates of LumexAtomicTestSupport.hpp, whose targets are
# the macros LUMEX_ATOMIC_TEST_SHARED_ENGINE and LUMEX_ATOMIC_TEST_WEAK_ENGINE
# (default: the common names). A new engine is one more variant, with no change
# of any test body:
#   VARIANT lock_free DEFINITIONS
#       LUMEX_ATOMIC_TEST_SHARED_ENGINE=atomic_shared_ptr_lock_free
#       LUMEX_ATOMIC_TEST_WEAK_ENGINE=atomic_weak_ptr_lock_free
#       LUMEX_ATOMIC_TEST_ENGINE_DEFERS_DESTRUCTION=1
#       "LUMEX_ATOMIC_TEST_ENGINE_QUIESCE()=...clean_up ()"
# plus the line `lumex_test_standards_declare_variant (atomic lock_free 11 20)`
# in LumexTestStandards.cmake (the same for std_backed at 20).
#
# The concurrency tests are
#   Aba               ABA through the reuse of the address of a freed node and
#                     of a control block, the A -> B -> A rule, aliasing
#                     identity, weak-to-shared promotion against the last
#                     release, expired () races
#   Linearizability   Wing and Gong search on short recorded histories, the
#                     version-chain counter, the exchange permutation,
#                     monotone loads, the walk of the read-modify-write
#                     operations, the precision of the strong compare-exchange
#   Structures        a Treiber stack and a Michael-Scott queue of shared
#                     nodes, and the forced ABA interleaving on a raw-pointer
#                     stack that the checks must catch
#   Lifecycle         release of replaced values, reference-count balance and
#                     self-assignment races, thread churn, deleters that call
#                     the atomic, a parked deleter, static storage at exit
#   Progress          loaders against compare-exchange swappers, time-boxed
#   WaitStress        token rings and many waiters (lost wake-ups)
#   Falsification     the checkers run on deliberately broken engines
#                     (LumexAtomicTestEngines.hpp) and must report them
#   Soak              long runs (LUMEX_TEST_SOAK), see SOAK_FILTER below
# Thread counts (LUMEX_TEST_THREADS, default 1, 2, 4, 8 and above the cores),
# schedule kinds, seeds (LUMEX_TEST_SEED), work (LUMEX_TEST_SCALE) and the
# generic helpers come from lumex/tests/support/.
lumex_add_standard_suites(AtomicSmartPtr
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

add_subdirectory(split_count)
