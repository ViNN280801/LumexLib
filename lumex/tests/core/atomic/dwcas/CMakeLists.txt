# lumex/tests/core/atomic/dwcas/CMakeLists.txt
#
# The tests of lumex/core/atomic/dwcas: the 16-byte atomic word with a
# 128-bit compare-and-swap (x86-64 only). Module key atomic.dwcas of
# lumex/tests/LumexTestStandards.cmake; one suite per standard (C++11, 14, 17,
# 20) plus the variants that run the same sources on the other backends:
#   LumexDwcasCxx<std>Tests             the default backend of the compiler
#                                       (inline assembly with GCC and Clang,
#                                       the intrinsic with MSVC)
#   LumexDwcasBuiltinCxx<std>Tests      LUMEX_DWCAS_BACKEND=3: __atomic_*
#                                       builtins on a 128-bit integer (links
#                                       libatomic; the backend ThreadSanitizer
#                                       can see)
#   LumexDwcasMsvcWrapperCxx<std>Tests  LUMEX_DWCAS_BACKEND=2: the MSVC
#                                       wrapper. Where the compiler is not MSVC
#                                       the directory msvc_stand_in/ supplies
#                                       an <intrin.h> whose
#                                       _InterlockedCompareExchange128 is
#                                       written with lock cmpxchg16b, so the
#                                       order of the arguments and of the
#                                       halves in the wrapper is exercised.
# CTest names carry the prefix "atomic.dwcas." and the suffix of the suite
# (.cxx11, .builtin.cxx11, .msvc_wrapper.cxx11, ...).
# LUMEX_TEST_DWCAS_BACKEND_EXPECTED is the backend the suite must run on (the
# configuration test compares it with LUMEX_DWCAS_BACKEND).
# A ThreadSanitizer tree (LUMEX_USE_TSAN) runs every suite on the built-in
# backend, because the sanitizer does not see inline assembly and the stand-in
# is assembly too; the msvc_wrapper variant is then built on the built-in
# backend as well and so only repeats the others.
# A word lives in 16-byte aligned writable memory; the tests that fault on
# purpose (a misaligned word, a word on a read-only page) run in a child
# process.
set(_dwcas_sanitize_thread FALSE)
if(LUMEX_USE_TSAN)
    set(_dwcas_sanitize_thread TRUE)
endif()

if(_dwcas_sanitize_thread)
    set(_dwcas_default_definitions
        LUMEX_DWCAS_BACKEND=3 LUMEX_TEST_DWCAS_BACKEND_EXPECTED=3)
else()
    set(_dwcas_default_definitions "")
endif()

lumex_add_standard_suites(Dwcas
    MODULE atomic.dwcas
    LINK lumex::atomic
    SOURCES LumexDwcasTestSupport.hpp
    DEFINITIONS ${_dwcas_default_definitions}
    DISCOVER_ARGS PROPERTIES TIMEOUT 300 LABELS tsan
    TARGETS_VAR _dwcas_targets
    VARIANT builtin
        DEFINITIONS LUMEX_DWCAS_BACKEND=3 LUMEX_TEST_DWCAS_BACKEND_EXPECTED=3
    VARIANT msvc_wrapper
        DEFINITIONS LUMEX_DWCAS_BACKEND=2 LUMEX_TEST_DWCAS_BACKEND_EXPECTED=2
)

foreach(_target IN LISTS _dwcas_targets)
    if(_target MATCHES "Builtin" OR _dwcas_sanitize_thread)
        # GCC sends the 16-byte __atomic_* operations to libatomic; Clang
        # does so without -mcx16.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(${_target} PRIVATE atomic)
        endif()
    endif()
    if(_target MATCHES "MsvcWrapper" AND NOT MSVC AND NOT _dwcas_sanitize_thread)
        target_include_directories(${_target} BEFORE PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/msvc_stand_in)
    endif()
endforeach()
