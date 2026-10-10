# lumex/tests/core/atomic/smart_ptr/split_count/CMakeLists.txt
#
# The tests of lumex/core/atomic/smart_ptr/split_count: the split-count
# engine of atomic_shared_ptr_lock_free_split_count and
# atomic_weak_ptr_lock_free_split_count (x86-64 only, over
# lumex::core::smart_ptr). Module key atomic.smart_ptr.split_count of
# lumex/tests/LumexTestStandards.cmake; one suite per standard (C++11, 17,
# 20) plus the variants that run the same sources with other definitions:
#   LumexAtomicSplitCountCxx<std>Tests               the default backend of
#                                                    the compiler
#   LumexAtomicSplitCountBuiltinCxx<std>Tests        LUMEX_DWCAS_BACKEND=3:
#                                                    __atomic_* builtins on a
#                                                    128-bit integer (links
#                                                    libatomic); the only
#                                                    backend ThreadSanitizer
#                                                    can see, so the
#                                                    protocol's memory orders
#                                                    are checked there
#   LumexAtomicSplitCountWaitTableCxx20Tests         C++20 with the wait
#                                                    table forced
#   LumexAtomicSplitCountNoSplitCountCxx<std>Tests   the build without the
#                                                    engine
#                                                    (LUMEX_ATOMIC_SMART_PTR_
#                                                    DISABLE_SPLIT_COUNT)
# CTest names carry the prefix "atomic.smart_ptr.split_count." and the suffix
# of the suite (.cxx11, .builtin.cxx11, ...). A ThreadSanitizer tree
# (LUMEX_USE_TSAN) runs every suite on the built-in backend, because the
# sanitizer does not see inline assembly (only the tests select the backend).
# LumexSplitCountTestSupport.hpp holds what the files share: the counting
# object, the ledger policy, the gate policy and the torn-read policy.
set(_split_sanitize_thread FALSE)
if(LUMEX_USE_TSAN)
    set(_split_sanitize_thread TRUE)
endif()

if(_split_sanitize_thread)
    set(_split_default_definitions LUMEX_DWCAS_BACKEND=3)
else()
    set(_split_default_definitions "")
endif()

lumex_add_standard_suites(AtomicSplitCount
    MODULE atomic.smart_ptr.split_count
    LINK lumex::atomic
    SOURCES LumexSplitCountTestSupport.hpp
    DEFINITIONS ${_split_default_definitions}
    DISCOVER_ARGS PROPERTIES TIMEOUT 300 LABELS tsan
    SOAK_FILTER *Soak*
    TARGETS_VAR _split_targets
    VARIANT builtin DEFINITIONS LUMEX_DWCAS_BACKEND=3
    VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE
    VARIANT no_split_count DEFINITIONS LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT
)

foreach(_target IN LISTS _split_targets)
    if(_target MATCHES "Builtin" OR _split_sanitize_thread)
        # GCC sends the 16-byte __atomic_* operations to libatomic; Clang
        # does so without -mcx16.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(${_target} PRIVATE atomic)
        endif()
    endif()
endforeach()
