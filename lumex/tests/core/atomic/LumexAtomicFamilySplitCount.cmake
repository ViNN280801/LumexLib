# lumex/tests/core/atomic/LumexAtomicFamilySplitCount.cmake
#
# What the directories of the atomic tests share for the split_count_engine
# variant (the split-count engine over lumex::smart_ptr, run by the family
# suites of atomic/, atomic/smart_ptr/ and atomic/sync/).
#
# The engine's 128-bit compare-and-swap is inline assembly by default, and
# ThreadSanitizer does not see inline assembly: every access made through it
# is reported as a data race that is not one (ctl_base::anchor () against
# ctl_base::ctl_base ()). A ThreadSanitizer tree (LUMEX_USE_TSAN) therefore
# runs the variant on the built-in backend (LUMEX_DWCAS_BACKEND=3, __atomic_*
# on a 128-bit integer), the same way the dedicated suites of split_count/ and
# of dwcas/ do. Use:
#   include (${CMAKE_CURRENT_LIST_DIR}/.../LumexAtomicFamilySplitCount.cmake)
#   lumex_add_standard_suites (... TARGETS_VAR _suites
#       VARIANT split_count_engine DEFINITIONS ...
#           ${LUMEX_ATOMIC_FAMILY_SPLIT_COUNT_DEFINITIONS})
#   lumex_atomic_family_split_count_link (${_suites})
set(LUMEX_ATOMIC_FAMILY_SPLIT_COUNT_DEFINITIONS "")
if(LUMEX_USE_TSAN)
    set(LUMEX_ATOMIC_FAMILY_SPLIT_COUNT_DEFINITIONS LUMEX_DWCAS_BACKEND=3)
endif()

# Links what the built-in backend needs to the split_count_engine targets of
# the given list (the others are left alone), in a ThreadSanitizer tree only.
function(lumex_atomic_family_split_count_link)
    if(NOT LUMEX_USE_TSAN)
        return()
    endif()
    foreach(_target IN LISTS ARGN)
        if(NOT _target MATCHES "SplitCountEngine")
            continue()
        endif()
        # GCC sends the 16-byte __atomic_* operations to libatomic; Clang
        # does so unless the target has cx16.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(${_target} PRIVATE atomic)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"
               AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
            target_compile_options(${_target} PRIVATE -mcx16)
        endif()
    endforeach()
endfunction()
