# Root CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Must be set before project() enables RC. clang-cl + Ninja otherwise
# uses `cmake -E cmake_llvm_rc` (clang-cl -E, then rc.exe). That wrapper
# hangs here once version .rc files are dirty: clang-cl is invoked with
# `-clang:-MD -clang:-MF` on an .rc and never finishes. LibraryVersioning
# writes .rc files that do not include windows.h, so rc.exe can compile
# them without a C preprocessor.
if(WIN32)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> /nologo /fo<OBJECT> <SOURCE>")
endif()

# TRUE when LumexLib is the top-level project, FALSE when a parent project
# embeds it with add_subdirectory(). CMake 3.21 has PROJECT_IS_TOP_LEVEL,
# but the minimum here is 3.16, and the value is needed before project().
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LUMEX_IS_TOP_LEVEL TRUE)
else()
    set(LUMEX_IS_TOP_LEVEL FALSE)
endif()

# Prepend, not append: an embedding parent may already have another
# CMakeRoutines checkout on CMAKE_MODULE_PATH, and every relative
# include(core/...) below must resolve to this library's own copy.
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/CMakeRoutines")

# Must run before project(): compiler detection has not happened yet.
# No-op on non-Windows hosts. Skips INCLUDE/LIB injection for gcc/clang++.
# clang-cl is an MSVC frontend, so the routine FATALS when vswhere /
# vcvarsall.bat are missing. LLVM-only trees already pass
# CMAKE_CXX_COMPILER; do not force a VS install on those machines.
# When embedded, the parent already ran compiler detection and owns the
# toolchain, so the routine would FATAL; skip it.
if(NOT LUMEX_IS_TOP_LEVEL)
    message(STATUS
        "MsvcVcvarsConfig: skipped (LumexLib is embedded; "
        "the parent project owns compiler selection)")
else()
    include(core/MsvcVcvarsConfig)
    if(CMAKE_HOST_WIN32)
        set(_lumex_cxx "${CMAKE_CXX_COMPILER}")
        if(NOT _lumex_cxx AND DEFINED ENV{CXX} AND NOT "$ENV{CXX}" STREQUAL "")
            set(_lumex_cxx "$ENV{CXX}")
        endif()
        set(_lumex_vswhere
            "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe")
        if(NOT EXISTS "${_lumex_vswhere}")
            set(_lumex_vswhere
                "$ENV{ProgramFiles}/Microsoft Visual Studio/Installer/vswhere.exe")
        endif()
        set(_lumex_have_vcvars FALSE)
        if(EXISTS "${_lumex_vswhere}"
                OR MSVC_VCVARSALL
                OR MSVC_VS_INSTALLATION_PATH)
            set(_lumex_have_vcvars TRUE)
        endif()
        if(_lumex_have_vcvars OR NOT _lumex_cxx)
            configure_msvc_vcvars()
        else()
            message(STATUS
                "MsvcVcvarsConfig: skipped (no vswhere/vcvarsall; "
                "compiler already '${_lumex_cxx}')")
        endif()
        unset(_lumex_cxx)
        unset(_lumex_vswhere)
        unset(_lumex_have_vcvars)
    else()
        configure_msvc_vcvars()
    endif()
endif()

project(LumexLib
    VERSION 1.0.0.0
    LANGUAGES CXX
)
# The resource compiler only exists on Windows (version .rc files of the
# shared libraries, cmake/LibraryVersioning.cmake). Asking for RC everywhere
# made configure fail on Linux ("No CMAKE_RC_COMPILER could be found").
# Pinned by LumexCMake.wiring_rc_windows_only.
if(WIN32)
    enable_language(RC)
endif()

if(ARCHITECTURE)
    message(STATUS "Building for target architecture: ${ARCHITECTURE}")
endif()

# --- C++ Standard ---
# Deliberately NOT setting a project-wide CMAKE_CXX_STANDARD default. A previous
# attempt to pin one globally broke base64: Decoder/Validator select which
# overload signature exists via `#if __cplusplus`, so a library .cpp compiled
# at a different standard than its tests fails to link. Several *headers*
# (LumexBit/Cast/MemRead/Ranges, ...) do require C++20 unconditionally,
# but every one of them is header-only and only ever reached from a target that already pins
# its own standard explicitly (test executables via `lumex_test_use_gtest(<tgt> CXX_STANDARD N)`).
# Several older modules (e.g. base64's Decoder/Validator) instead use `#if __cplusplus` to
# select which overload SIGNATURE is declared/defined at all - not just to gate extra
# C++17/20-only additions - so their library .cpp files must keep compiling at whatever
# standard the consuming test target expects (historically the toolchain's own bare default),
# or the two sides disagree on the declared API shape and fail to link.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Options ---
include(cmake/LumexOptions.cmake)
include(cmake/LumexModules.cmake)
include(cmake/LumexNlohmannJson.cmake)
lumex_check_module_dependencies()

string(TOUPPER "${LUMEX_OPTIMIZATION_LEVEL}" _LUMEX_OPTIMIZATION_LEVEL)
if(_LUMEX_OPTIMIZATION_LEVEL STREQUAL "MAXIMUM")
    message(FATAL_ERROR
        "LUMEX_OPTIMIZATION_LEVEL=Maximum enables /fp:fast and -ffast-math. "
        "Lumex IEEE APIs (IsNanInf, CheckedNarrowCast, stringify inf/nan) require IEEE 754. "
        "Use Portable (default), Standard, Aggressive, or MinSize.")
endif()

if(LUMEX_BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
    message(STATUS "Building as shared libraries")
else()
    message(STATUS "Building as static libraries")
endif()

if(BUILD_SHARED_LIBS OR (CMAKE_POSITION_INDEPENDENT_CODE AND NOT WIN32))
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Per-target flags come from CMakeRoutines (cmake/LumexBuild.cmake).
# Do not re-enable global IPO: Portable forbids LTO.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF)

include(cmake/LumexBuild.cmake)

# RULE_LAUNCH_* must be set before targets compile (Ninja/Makefiles).
configure_build_timing(ENABLE ${LUMEX_BUILD_TIMING})

if(WIN32)
    configure_windows_version(VERSION TENELEVEN)
endif()

# Intended default when the user did not pass -DCMAKE_BUILD_TYPE.
# On CMake 4.x this check often does nothing: project() (above) already
# initializes CMAKE_BUILD_TYPE from the toolchain, commonly Debug for
# MSVC-like compilers. Pass -DCMAKE_BUILD_TYPE=Release explicitly for a
# Release tree. Do not move this block before project() without a
# deliberate decision - that would change the implicit default.
# Embedded: the build type belongs to the parent; never FORCE its cache.
if(LUMEX_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Optimization level: ${LUMEX_OPTIMIZATION_LEVEL}")
message(STATUS
    "Sanitizers: ASAN=${LUMEX_USE_ASAN} UBSAN=${LUMEX_USE_UBSAN} TSAN=${LUMEX_USE_TSAN}")
message(STATUS
    "Static analysis: CLANG_TIDY=${LUMEX_USE_CLANG_TIDY} CPPCHECK=${LUMEX_USE_CPPCHECK} "
    "PROFILE=${LUMEX_STATIC_ANALYSIS_PROFILE}")
message(STATUS
    "Maximum standard compliance: ${LUMEX_MAXIMUM_STANDARD_COMPLIANCE}")
message(STATUS
    "Build timing: ${LUMEX_BUILD_TIMING}")
message(STATUS
    "Generate build info: ${LUMEX_GENERATE_BUILD_INFO} "
    "(format=${LUMEX_BUILD_INFO_FORMAT})")
lumex_warn_sanitizers_without_debug_info()

# clang-cl + ASan rejects the debug CRT (invalid argument '-MDd' not allowed
# with '-fsanitize=address'). Must run before any add_subdirectory so every
# target inherits the forced release CRT; see cmake/LumexSanitizerBuildType.cmake.
lumex_apply_asan_runtime_library()

if(UNIX AND NOT APPLE)
    add_compile_definitions(_GNU_SOURCE)
elseif(APPLE)
    add_compile_options(-fno-common)
endif()

# ---------------------------------------------------------------------------
# Unified output directories - ensures *.dll sit next to the executables
# ---------------------------------------------------------------------------
set(LUMEX_BIN_DIR ${CMAKE_BINARY_DIR}/bin)
set(LUMEX_LIB_DIR ${CMAKE_BINARY_DIR}/lib)

if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONF ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONF} CONF_UPPER)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONF_UPPER} ${LUMEX_BIN_DIR}/${CONF})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONF_UPPER} ${LUMEX_BIN_DIR}/${CONF})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONF_UPPER} ${LUMEX_LIB_DIR}/${CONF})
    endforeach()
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${LUMEX_BIN_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LUMEX_BIN_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LUMEX_LIB_DIR})
endif()

include(cmake/LumexLibraryVersioning.cmake)

add_subdirectory(lumex)

# CMakeRoutines' symlink target is not ALL. clangd reads the repo-root copy.
# Visual Studio / Xcode never write compile_commands.json, so copy via a
# script that no-ops when the source is missing (copy_if_different would
# fail ALL and abort MSBuild).
# Embedded: CMAKE_EXPORT_COMPILE_COMMANDS is a cache entry of the whole
# tree (the routine FORCEs it), and the copy would drop the parent's
# database into this submodule's source tree; leave both to the parent.
if(LUMEX_IS_TOP_LEVEL)
    configure_compile_commands(ENABLE ON CREATE_SYMLINK OFF)
    add_custom_target(lumex_copy_compile_commands ALL
        COMMAND ${CMAKE_COMMAND}
                -DSOURCE=${CMAKE_BINARY_DIR}/compile_commands.json
                -DDEST=${PROJECT_SOURCE_DIR}/compile_commands.json
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyCompileCommandsIfPresent.cmake
        COMMENT "Copying compile_commands.json to the repo root for clangd (if present)"
        VERBATIM
    )
endif()

if(LUMEX_BUILD_XML)
    if(LUMEX_XML_WCHAR_MODE)
        add_compile_definitions(LUMEX_XML_WCHAR_MODE)
        message(STATUS "Building with Xml wchar_t mode enabled")
    endif()
    message(STATUS "Building with Xml enabled")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# LUMEX_INSTALL (cmake/LumexOptions.cmake) defaults to ON only when LumexLib
# is the top-level project; module CMakeLists.txt files use the same guard.
if(LUMEX_INSTALL)
    lumex_install_export_if_any(LumexCoreTargets LumexCoreTargets.cmake
        LumexCore_base64
        LumexCore_circular_buffer
        LumexCore_crc
        LumexCore_environment
        LumexCore_exceptions
        LumexCore_expected
        LumexCore_filesystem
        LumexCore_fmt
        LumexCore_number_generator
        LumexCore_math
        LumexCore_optional
        LumexCore_reflection
        LumexCore_string
        LumexCore_string_view
        LumexCore_temporary
        LumexCore_time
        LumexCore_utility
    )

    lumex_install_export_if_any(LumexAppliedTargets LumexAppliedTargets.cmake
        LumexApplied_hardware
        LumexApplied_json
        LumexApplied_logger
        LumexApplied_logging
        LumexApplied_resource_monitor
        LumexApplied_serial
        LumexApplied_settings
    )

    lumex_install_export_if_any(LumexXmlTargets LumexXmlTargets.cmake
        LumexXml
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    configure_package_config_file(
        "cmake/LumexLibConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LumexLib"
    )

    install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfigVersion.cmake"
        DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/LumexLib
    )
endif()

# ==============================================================================
# DOCUMENTATION
# ==============================================================================
if(LUMEX_BUILD_DOCUMENTATION)
    message(STATUS "Building with documentation enabled")
    find_program(DOXYGEN_EXECUTABLE NAMES doxygen)

    if(NOT DOXYGEN_EXECUTABLE)
        message(FATAL_ERROR "Doxygen executable not found! Please install Doxygen or ensure it's in your PATH.")
    endif()

    # Markdown pages (README.md next to a component) keep repository-relative
    # image links; this filter turns them into file names from IMAGE_PATH
    # while Doxygen reads them. Without Python the images are just missing.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(LUMEX_DOXYGEN_MARKDOWN_FILTER
            "${Python3_EXECUTABLE} ${LumexLib_SOURCE_DIR}/Scripts/Doxygen/markdown_image_filter.py")
    else()
        set(LUMEX_DOXYGEN_MARKDOWN_FILTER "")
        message(WARNING "Python 3 not found: Markdown images are left out of the documentation")
    endif()

    # Configure Doxyfile from template with project version
    set(DOXYFILE_IN "${LumexLib_SOURCE_DIR}/Doxyfile")
    set(DOXYFILE_OUT "${CMAKE_BINARY_DIR}/Doxyfile")

    configure_file(
        ${DOXYFILE_IN}
        ${DOXYFILE_OUT}
        @ONLY
    )

    set(DOXYFILE_PATH "${DOXYFILE_OUT}")
    set(DOXYGEN_OUTPUT_DIR "${LumexLib_SOURCE_DIR}/docs")

    add_custom_command(
        OUTPUT "${DOXYGEN_OUTPUT_DIR}/html/index.html"
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_PATH}
        WORKING_DIRECTORY "${LumexLib_SOURCE_DIR}"
        DEPENDS ${DOXYFILE_OUT}
        COMMENT "Generating Doxygen documentation (version: ${PROJECT_VERSION})"
        VERBATIM
    )

    add_custom_target(generate_documentation ALL
        DEPENDS "${DOXYGEN_OUTPUT_DIR}/html/index.html"
        COMMENT "Doxygen documentation generated"
    )

    if(LUMEX_INSTALL)
        install(DIRECTORY "${DOXYGEN_OUTPUT_DIR}/"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/docs"
            PATTERN "*.map" EXCLUDE
            PATTERN "*.md5" EXCLUDE
            PATTERN "*.tag" EXCLUDE
            PATTERN "*.bak" EXCLUDE
        )
    endif()
endif()

# ==============================================================================
# EXAMPLES / TESTING
# ==============================================================================
# enable_testing() must precede add_test() in lumex/examples. Tests ON also
# compiles and registers every example executable (exit 0) so the samples
# cannot drift from the public API.
if(LUMEX_BUILD_TESTS)
    enable_testing()
endif()

if(LUMEX_BUILD_EXAMPLES OR LUMEX_BUILD_TESTS)
    message(STATUS "Building examples (LUMEX_BUILD_EXAMPLES=${LUMEX_BUILD_EXAMPLES}, LUMEX_BUILD_TESTS=${LUMEX_BUILD_TESTS})")
    add_subdirectory(lumex/examples)
endif()

if(LUMEX_BUILD_TESTS)
    message(STATUS "Building with tests enabled")
    add_subdirectory(lumex/tests)
endif()

if(LUMEX_BUILD_BENCHMARKS AND LUMEX_BUILD_CORE AND LUMEX_BUILD_FMT)
    message(STATUS "Building benchmarks (benchmarks/fmt)")
    add_subdirectory(benchmarks/fmt)
endif()

# Shared libs (not test/example binaries) -> <platform>/Distr<Config>.
# Tests are registered with CTest; the programmer runs ctest explicitly.
# Embedded: Distr lives in this library's source tree, and the parent
# decides where Lumex runtime files go, so do not publish there.
if(LUMEX_IS_TOP_LEVEL)
    lumex_add_publish_distr_target()
endif()

# After every add_subdirectory: per-target CompilerFlags + OptimizationLevel
# (+ CMakeRoutines sanitizers when any LUMEX_USE_*SAN is ON, including tests
# and vendored gtest).
lumex_configure_all_compiled_targets()
print_build_info()
