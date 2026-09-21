# cmake/LumexSanitizerBuildType.cmake
#
# Warn when any LUMEX_USE_*SAN is ON together with Release or MinSizeRel.
# Sanitizer reports need debug info and mostly-unoptimized frames;
# RelWithDebInfo is the minimum useful config, Debug is the recommended one.
#
# Independent of CMakeRoutines so LumexCMake.* can include this file from
# cmake -P without project() or a compiler.

include_guard(GLOBAL)

function(lumex_sanitizer_build_type_is_weak out_var)
  set(_types "")
  if(CMAKE_BUILD_TYPE)
    list(APPEND _types "${CMAKE_BUILD_TYPE}")
  endif()
  if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND _types ${CMAKE_CONFIGURATION_TYPES})
  endif()

  set(_weak FALSE)
  foreach(_t IN LISTS _types)
    string(TOUPPER "${_t}" _u)
    if(_u STREQUAL "RELEASE" OR _u STREQUAL "MINSIZEREL")
      set(_weak TRUE)
    endif()
  endforeach()

  set(${out_var} "${_weak}" PARENT_SCOPE)
endfunction()

function(lumex_warn_sanitizers_without_debug_info)
  set(LUMEX_SANITIZER_WEAK_DIAGNOSTICS FALSE PARENT_SCOPE)

  if(NOT LUMEX_USE_ASAN AND NOT LUMEX_USE_UBSAN AND NOT LUMEX_USE_TSAN)
    return()
  endif()

  lumex_sanitizer_build_type_is_weak(_weak)
  if(NOT _weak)
    return()
  endif()

  set(LUMEX_SANITIZER_WEAK_DIAGNOSTICS TRUE PARENT_SCOPE)
  message(WARNING
    "A sanitizer is ON (ASAN=${LUMEX_USE_ASAN} UBSAN=${LUMEX_USE_UBSAN} "
    "TSAN=${LUMEX_USE_TSAN}) together with Release or MinSizeRel. "
    "Sanitizer reports need debug info and mostly-unoptimized code; "
    "Release/MinSizeRel will not give adequate diagnostics. "
    "Switch at least to RelWithDebInfo; Debug is the recommended configuration.")
endfunction()

# ---------------------------------------------------------------------------
# clang-cl + ASan cannot use the debug CRT
# ---------------------------------------------------------------------------
# clang-cl rejects `/MDd` (and `/MTd`) together with `-fsanitize=address`:
#   clang-cl: error: invalid argument '-MDd' not allowed with '-fsanitize=address'
#   clang-cl: note: AddressSanitizer doesn't support linking with debug runtime
#                 libraries yet
# Debug is this project's recommended sanitizer configuration, so instead of
# failing the build the runtime library is forced to the release DLL CRT
# (MultiThreadedDLL -> /MD) for that combination only. cl.exe ASan builds with
# /MDd, clang-cl UBSan-only builds, and non-sanitized trees keep the
# configuration default (MultiThreadedDebugDLL -> /MDd in Debug).
#
# CMAKE_MSVC_RUNTIME_LIBRARY must be set before the targets are created: the
# MSVC_RUNTIME_LIBRARY target property is initialized from that variable at
# target-creation time. The root CMakeLists.txt therefore calls
# lumex_apply_asan_runtime_library() before add_subdirectory(lumex/...).

function(lumex_asan_requires_release_crt out_var)
    set(_needs FALSE)

    if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND LUMEX_USE_ASAN)
        set(_runtime "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        if(_runtime)
            # MultiThreadedDebug and MultiThreadedDebugDLL are the hard
            # conflict. Release values (MultiThreaded, MultiThreadedDLL) and
            # generator expressions are left alone.
            if(_runtime MATCHES "Debug")
                set(_needs TRUE)
            endif()
        else()
            set(_types "")
            if(CMAKE_BUILD_TYPE)
                list(APPEND _types "${CMAKE_BUILD_TYPE}")
            endif()
            if(CMAKE_CONFIGURATION_TYPES)
                list(APPEND _types ${CMAKE_CONFIGURATION_TYPES})
            endif()
            foreach(_type IN LISTS _types)
                string(TOUPPER "${_type}" _upper)
                if(_upper STREQUAL "DEBUG")
                    set(_needs TRUE)
                endif()
            endforeach()
        endif()
    endif()

    set(${out_var} "${_needs}" PARENT_SCOPE)
endfunction()

function(lumex_apply_asan_runtime_library)
    lumex_asan_requires_release_crt(_needs)
    if(NOT _needs)
        return()
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" PARENT_SCOPE)
    message(STATUS
        "LumexSanitizerBuildType: clang-cl + LUMEX_USE_ASAN cannot use the "
        "debug CRT; forcing CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL (/MD). "
        "cl.exe ASan and UBSan-only trees keep the configuration default.")
endfunction()
