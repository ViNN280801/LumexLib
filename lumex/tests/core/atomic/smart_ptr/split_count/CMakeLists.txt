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
# object, the ledger policy, the gate policy and the torn-read policy, the
# allocation counters; LumexSplitCountScenarios.hpp the scenario templates
# (semantics table, stress shapes) and LumexSplitCountHold.hpp the threads held
# at a point of the protocol. LumexSplitCountAllocHooks.cxx11.tests.cpp
# replaces the global allocation functions (not under ThreadSanitizer).
# LumexSplitCountSim.hpp is the simulated-scheduler explorer (V1); its long runs
# are the Soak tests. LumexSplitCountSimAba.hpp is the ABA control of package V2
# (a toy stack that the explorer must catch).
# relacy/ holds the Relacy models of the memory orders (plain programs, CTest
# label relacy); it is added only when LUMEX_RELACY_DIR names a Relacy
# checkout, and then it adds nothing to the suites above.
set(_split_sanitize_thread FALSE)
if(LUMEX_USE_TSAN)
    set(_split_sanitize_thread TRUE)
endif()

if(_split_sanitize_thread)
    set(_split_default_definitions LUMEX_DWCAS_BACKEND=3)
else()
    set(_split_default_definitions "")
endif()

# The built-in backend needs __atomic_* on unsigned __int128 (GCC and Clang)
# and exists for ThreadSanitizer; MSVC has neither the type nor the builtins,
# so a tree that registered the variant would stop at the #error of
# LumexDwcasConfig.hpp, the same way the dwcas suites did. The table of
# lumex/tests/LumexTestStandards.cmake declares the variant under the same
# condition - lumex_add_standard_suites fails when the two disagree.
set(_split_builtin_variant "")
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(_split_builtin_variant VARIANT builtin DEFINITIONS LUMEX_DWCAS_BACKEND=3)
endif()

lumex_add_standard_suites(AtomicSplitCount
    MODULE atomic.smart_ptr.split_count
    LINK lumex::atomic
    SOURCES LumexSplitCountTestSupport.hpp LumexSplitCountScenarios.hpp
            LumexSplitCountHold.hpp LumexSplitCountSim.hpp
            LumexSplitCountSimScenarios.hpp LumexSplitCountSimAba.hpp
    DEFINITIONS ${_split_default_definitions}
    DISCOVER_ARGS PROPERTIES TIMEOUT 300 LABELS tsan
    SOAK_FILTER *Soak*
    TARGETS_VAR _split_targets
    ${_split_builtin_variant}
    VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE
    VARIANT no_split_count DEFINITIONS LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT
    VARIANT prefer_split_count DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FAMILY_PREFER_SPLIT_COUNT
)

foreach(_target IN LISTS _split_targets)
    if(_target MATCHES "Builtin" OR _split_sanitize_thread)
        # GCC sends the 16-byte __atomic_* operations to libatomic; Clang
        # does so unless the target has cx16.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(${_target} PRIVATE atomic)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"
               AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
            # Windows has no libatomic to link: the built-in backend must
            # lower its 16-byte builtins to the instruction (the library
            # documents -mcx16 in LumexDwcasBackendAsm.hpp), or clang emits
            # calls that nothing defines (lld-link: undefined symbol
            # __atomic_compare_exchange_16).
            target_compile_options(${_target} PRIVATE -mcx16)
        endif()
    endif()
endforeach()

# The Relacy models (optional, not part of the default run): see
# relacy/CMakeLists.txt. They are built when LUMEX_BUILD_RELACY is ON or when
# LUMEX_RELACY_DIR is given. Relacy itself is vendored in 3rdparty/relacy and
# used by default; -DLUMEX_RELACY_DIR=<path> to another checkout (a directory
# that holds relacy/relacy.hpp) replaces it. Nothing else here depends on it.
if(LUMEX_RELACY_DIR)
    set(_lumex_relacy_requested ON)
else()
    set(_lumex_relacy_requested ${LUMEX_BUILD_RELACY})
    get_filename_component(LUMEX_RELACY_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../3rdparty/relacy"
        ABSOLUTE)
endif()
if(_lumex_relacy_requested)
    if(EXISTS "${LUMEX_RELACY_DIR}/relacy/relacy.hpp")
        add_subdirectory(relacy)
    else()
        message(STATUS "LUMEX_RELACY_DIR has no relacy/relacy.hpp: the Relacy models are not built")
    endif()
endif()
