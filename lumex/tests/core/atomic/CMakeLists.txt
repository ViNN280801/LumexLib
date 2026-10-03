# lumex/tests/core/atomic/CMakeLists.txt
#
# atomic_shared_ptr / atomic_weak_ptr work from C++11 (the lock-based
# implementation) and wrap std::atomic<std::shared_ptr<T>> where the standard
# library has it; wait () sleeps through std::atomic::wait from C++20 and on a
# striped mutex / condition variable table otherwise. One suite per standard
# of the module and per variant (lumex/tests/LumexTestStandards.cmake), every
# one built from the files of its standard and the lower ones:
#   LumexAtomicCxx11Tests          C++11 - lock-based, table; every
#                                  *.cxx11.tests.cpp
#   LumexAtomicCxx17Tests          C++17 - lock-based, table;
#                                  + LumexAtomicSmartPtrKinds.cxx17.tests.cpp
#                                  (weak_from_this, shared_ptr<T[]>)
#   LumexAtomicCxx20Tests          C++20 - std::atomic<std::shared_ptr<T>>
#                                  where the library has it (libstdc++ 12+,
#                                  MSVC), lock-based otherwise (libc++);
#                                  std::atomic::wait; + the *.cxx20.tests.cpp
#                                  files (make_shared<T[]>, std::latch,
#                                  std::jthread, the differential run against
#                                  std::atomic<std::shared_ptr<T>>)
#   LumexAtomicLockBasedCxx20Tests C++20 - lock-based (forced),
#                                  std::atomic::wait
#   LumexAtomicWaitTableCxx20Tests C++20 - as LumexAtomicCxx20Tests, with the
#                                  table (forced)
# LumexAtomicGlobalNames.cxx11.tests.cpp declares global atomic_shared_ptr
# and atomic_weak_ptr templates of its own: every suite stops building if the
# module declares either name at global scope.
# CTest names carry the directory prefix "atomic." and the suffix of the
# suite (.cxx11, ..., .lock_based.cxx20). A lost wake-up hangs a test; the
# in-test watchdog aborts it after 120 s and the CTest timeout is the second
# line of defence.
lumex_add_standard_suites(Atomic
    MODULE atomic
    LINK lumex::atomic
    SOURCES LumexAtomicTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300
    VARIANT lock_based DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED
    VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE
)
