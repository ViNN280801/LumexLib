# Shared by sanitizer_* cmake -P cases. Macro so
# lumex_warn_sanitizers_without_debug_info PARENT_SCOPE lands here.

macro(lumex_run_sanitizer_build_type_check expect_weak)
    include("${LUMEX_SOURCE_DIR}/cmake/LumexSanitizerBuildType.cmake")
    lumex_warn_sanitizers_without_debug_info()
    if(${expect_weak})
        if(NOT LUMEX_SANITIZER_WEAK_DIAGNOSTICS)
            message(FATAL_ERROR
                "expected LUMEX_SANITIZER_WEAK_DIAGNOSTICS TRUE "
                "(ASAN=${LUMEX_USE_ASAN} UBSAN=${LUMEX_USE_UBSAN} "
                "TSAN=${LUMEX_USE_TSAN} "
                "BUILD_TYPE=${CMAKE_BUILD_TYPE} "
                "CONFIGS=${CMAKE_CONFIGURATION_TYPES})")
        endif()
    else()
        if(LUMEX_SANITIZER_WEAK_DIAGNOSTICS)
            message(FATAL_ERROR
                "expected LUMEX_SANITIZER_WEAK_DIAGNOSTICS FALSE "
                "(ASAN=${LUMEX_USE_ASAN} UBSAN=${LUMEX_USE_UBSAN} "
                "TSAN=${LUMEX_USE_TSAN} "
                "BUILD_TYPE=${CMAKE_BUILD_TYPE} "
                "CONFIGS=${CMAKE_CONFIGURATION_TYPES})")
        endif()
    endif()
endmacro()
