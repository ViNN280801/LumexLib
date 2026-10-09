# lumex/tests/core/atomic/CMakeLists.txt
#
# atomic_shared_ptr / atomic_weak_ptr work from C++11 (the lock-based
# implementation) and wrap std::atomic<std::shared_ptr<T>> where the standard
# library has it; wait () sleeps through std::atomic::wait from C++20 and on a
# striped mutex / condition variable table otherwise. The tests follow the
# source tree (lumex/core/atomic):
#   smart_ptr/  the smart pointers (LumexAtomicSmartPtr*, SharedPtr, WeakPtr)
#   sync/       the wait and the two-bit lock (LumexAtomicWait)
#   this one    LumexAtomicGlobalNames.cxx11.tests.cpp, the test of the module
#               umbrella, and LumexAtomicTestSupport.hpp, which both
#               directories use
# Every directory has one suite per standard of the module and per variant
# (lumex/tests/LumexTestStandards.cmake), each one built from the files of its
# standard and the lower ones; the suites of this directory are
#   LumexAtomicCxx11Tests          C++11 - lock-based, table
#   LumexAtomicCxx17Tests          C++17 - lock-based, table
#   LumexAtomicCxx20Tests          C++20 - std::atomic<std::shared_ptr<T>>
#                                  where the library has it (libstdc++ 12+,
#                                  MSVC), lock-based otherwise (libc++);
#                                  std::atomic::wait
#   LumexAtomicLockBasedCxx20Tests C++20 - lock-based (forced),
#                                  std::atomic::wait
#   LumexAtomicWaitTableCxx20Tests C++20 - as LumexAtomicCxx20Tests, with the
#                                  table (forced)
# and the same five, named LumexAtomicSmartPtr... and LumexAtomicSync..., in
# each subdirectory.
# LumexAtomicGlobalNames.cxx11.tests.cpp declares global atomic_shared_ptr
# and atomic_weak_ptr templates of its own: every suite stops building if the
# module declares either name at global scope.
# CTest names carry the prefix of the directory ("atomic.",
# "atomic.smart_ptr.", "atomic.sync.") and the suffix of the suite (.cxx11,
# ..., .lock_based.cxx20). A lost wake-up hangs a test; the in-test watchdog
# aborts it after 120 s and the CTest timeout is the second line of defence.
#
# The concurrency tests of this module (ABA through address reuse, Treiber
# stack and Michael-Scott queue over atomic_shared_ptr, linearizability, the
# livelock regression, the self-tests on deliberately broken engines) are
# described in smart_ptr/CMakeLists.txt. Every test of the module carries the
# CTest label `tsan`: the ThreadSanitizer run is a separate build tree
#   cmake -S . -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
#         -DCMAKE_CXX_COMPILER=/opt/llvm-23.1.0/bin/clang++ \
#         -DLUMEX_BUILD_TESTS=ON -DLUMEX_USE_TSAN=ON
#   ctest --test-dir build-tsan -L tsan -j4
# (Clang by default; GCC's ThreadSanitizer needs `setarch x86_64 -R ctest ...`
# with a kernel that randomizes mappings above its shadow range). The soak
# runs are `*Soak*` tests: left out of the default registration, registered
# under the label `soak` by -DLUMEX_BUILD_SOAK_TESTS=ON (`ctest -L soak`), or
# run directly with LUMEX_TEST_SOAK=1 and LUMEX_TEST_SOAK_SECONDS.
lumex_add_standard_suites(Atomic
    MODULE atomic
    LINK lumex::atomic
    SOURCES LumexAtomicTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300 LABELS tsan
    SOAK_FILTER *Soak*
    VARIANT lock_based DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED
    VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE
)

add_subdirectory(smart_ptr)
add_subdirectory(sync)
