# clang-cl + ASan must not keep the debug CRT: clang-cl fails every compile
# with "invalid argument '-MDd' not allowed with '-fsanitize=address'".
# CMakeRoutines' sanitizer helpers only add -fsanitize=..., so the runtime
# library switch lives in cmake/LumexSanitizerBuildType.cmake and is applied
# by the root CMakeLists.txt before any target exists.

include("${LUMEX_SOURCE_DIR}/cmake/LumexSanitizerBuildType.cmake")

function(_expect_needs label expect)
    lumex_asan_requires_release_crt(_got)
    if(NOT "${_got}" STREQUAL "${expect}")
        message(FATAL_ERROR
            "${label}: lumex_asan_requires_release_crt returned '${_got}', "
            "expected '${expect}' (MSVC=${MSVC} ID=${CMAKE_CXX_COMPILER_ID} "
            "ASAN=${LUMEX_USE_ASAN} BUILD_TYPE=${CMAKE_BUILD_TYPE} "
            "CONFIGS=${CMAKE_CONFIGURATION_TYPES} "
            "RUNTIME=${CMAKE_MSVC_RUNTIME_LIBRARY})")
    endif()
endfunction()

set(MSVC TRUE)
set(CMAKE_CXX_COMPILER_ID "Clang")
set(LUMEX_USE_ASAN ON)
set(LUMEX_USE_UBSAN ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "")

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CONFIGURATION_TYPES "")
_expect_needs("clang-cl ASan Debug" TRUE)

# macOS/Linux Clang has no MSVC runtime library concept.
set(MSVC FALSE)
_expect_needs("clang ASan Debug without MSVC frontend" FALSE)
set(MSVC TRUE)

# cl.exe ASan builds fine with /MDd; only the Clang frontend rejects it.
set(CMAKE_CXX_COMPILER_ID "MSVC")
_expect_needs("cl.exe ASan Debug" FALSE)
set(CMAKE_CXX_COMPILER_ID "Clang")

# clang-cl with UBSan only keeps the configuration default.
set(LUMEX_USE_ASAN OFF)
_expect_needs("clang-cl UBSan-only Debug" FALSE)
set(LUMEX_USE_ASAN ON)

# Release-family configurations already use the release CRT.
set(CMAKE_BUILD_TYPE RelWithDebInfo)
_expect_needs("clang-cl ASan RelWithDebInfo" FALSE)
set(CMAKE_BUILD_TYPE Release)
_expect_needs("clang-cl ASan Release" FALSE)
set(CMAKE_BUILD_TYPE MinSizeRel)
_expect_needs("clang-cl ASan MinSizeRel" FALSE)

# Multi-config generators: only a Debug entry conflicts.
set(CMAKE_BUILD_TYPE "")
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
_expect_needs("clang-cl ASan multi-config containing Debug" TRUE)
set(CMAKE_CONFIGURATION_TYPES "Release;MinSizeRel")
_expect_needs("clang-cl ASan multi-config without Debug" FALSE)

# An explicit release runtime is respected; an explicit debug one is still a
# hard clang-cl conflict and gets overridden.
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
_expect_needs("clang-cl ASan Debug with explicit MultiThreadedDLL" FALSE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
_expect_needs("clang-cl ASan Debug with explicit MultiThreadedDebugDLL" TRUE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
_expect_needs("clang-cl ASan Debug with explicit static MultiThreaded" FALSE)

# The apply function writes the release DLL CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
lumex_apply_asan_runtime_library()
if(NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
    message(FATAL_ERROR
        "lumex_apply_asan_runtime_library did not force MultiThreadedDLL "
        "(got '${CMAKE_MSVC_RUNTIME_LIBRARY}')")
endif()

# A non-conflicting tree must stay untouched.
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
lumex_apply_asan_runtime_library()
if(NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
    message(FATAL_ERROR
        "lumex_apply_asan_runtime_library touched a Release-family tree "
        "(got '${CMAKE_MSVC_RUNTIME_LIBRARY}')")
endif()

# The switch has to be wired in before any target is created.
file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
string(FIND "${_root}" "lumex_apply_asan_runtime_library()" _call)
if(_call EQUAL -1)
    message(FATAL_ERROR
        "CMakeLists.txt does not call lumex_apply_asan_runtime_library()")
endif()
string(FIND "${_root}" "add_subdirectory(lumex/examples)" _subdirs)
if(_subdirs EQUAL -1)
    message(FATAL_ERROR
        "CMakeLists.txt does not add_subdirectory(lumex/examples)")
endif()
if(_call GREATER _subdirs)
    message(FATAL_ERROR
        "lumex_apply_asan_runtime_library() must run before the "
        "add_subdirectory calls that create targets")
endif()
