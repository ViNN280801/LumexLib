# lumex/tests/core/atomic/smart_ptr/CMakeLists.txt
#
# The tests of lumex/core/atomic/smart_ptr: atomic_shared_ptr and
# atomic_weak_ptr, their configuration and the wait / notify built on them.
# One suite per standard of the module and per variant
# (lumex/tests/LumexTestStandards.cmake), every one built from the files of
# its standard and the lower ones:
#   LumexAtomicSmartPtrCxx11Tests          every *.cxx11.tests.cpp
#   LumexAtomicSmartPtrCxx17Tests          + LumexAtomicSmartPtrKinds.cxx17
#                                          (weak_from_this, shared_ptr<T[]>)
#   LumexAtomicSmartPtrCxx20Tests          + the *.cxx20.tests.cpp files
#                                          (make_shared<T[]>, std::latch,
#                                          std::jthread, the differential run
#                                          against std::atomic<std::shared_ptr
#                                          <T>>)
#   LumexAtomicSmartPtrLockBasedCxx20Tests C++20 with the lock-based cell
#                                          forced
#   LumexAtomicSmartPtrWaitTableCxx20Tests C++20 with the wait table forced
# LumexAtomicTestSupport.hpp of the module's directory holds what they share.
lumex_add_standard_suites(AtomicSmartPtr
    MODULE atomic
    LINK lumex::atomic
    SOURCES ../LumexAtomicTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300
    VARIANT lock_based DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED
    VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE
)
