# cmake/LumexOptions.cmake
#
# All project option() declarations live here. Include this file from the
# root CMakeLists.txt after project() and before any branch that reads
# a LUMEX_BUILD_* (per-module or tests/examples/docs) / LUMEX_XML_* /
# LUMEX_WITH_* / LUMEX_OPTIMIZATION_* /
# LUMEX_ENABLE_LTO / LUMEX_DEBUG_SYMBOLS /
# LUMEX_USE_ASAN / LUMEX_USE_UBSAN / LUMEX_USE_TSAN /
# LUMEX_USE_CLANG_TIDY / LUMEX_USE_CPPCHECK /
# LUMEX_MAXIMUM_STANDARD_COMPLIANCE /
# LUMEX_BUILD_TIMING / LUMEX_GENERATE_BUILD_INFO /
# LUMEX_BUILD_INFO_FORMAT / LUMEX_LOGGER_CONFIG_FORMAT cache variables.
#
# Defaults match the previous inline option() calls. Passing -D...=ON/OFF
# on the cmake command line still overrides these.

# ==============================================================================
# LIBRARY LINKAGE
# ==============================================================================

option(LUMEX_BUILD_SHARED_LIBS "Build shared libraries (.dll/.so)" ON)

# ==============================================================================
# OPTIONAL MODULES
# ==============================================================================
# Groups (same idea as LUMEX_BUILD_XML). A module compiles only when its
# group AND its per-module option are ON. Turning a group OFF does not
# rewrite the per-module cache entries.
option(LUMEX_BUILD_CORE "Build the core module group" ON)
option(LUMEX_BUILD_APPLIED "Build the applied module group" ON)
option(LUMEX_BUILD_XML "Build Xml" ON)

# Core
option(LUMEX_BUILD_BASE64 "Build core/base64" ON)
option(LUMEX_BUILD_CIRCULAR_BUFFER "Build core/circular_buffer" ON)
option(LUMEX_BUILD_CRC "Build core/crc" ON)
option(LUMEX_BUILD_ENVIRONMENT "Build core/environment" ON)
option(LUMEX_BUILD_EXCEPTIONS "Build core/exceptions" ON)
option(LUMEX_BUILD_EXPECTED "Build core/expected" ON)
option(LUMEX_BUILD_FILESYSTEM "Build core/filesystem" ON)
option(LUMEX_BUILD_GENERATORS "Build core/generators" ON)
option(LUMEX_BUILD_MATH "Build core/math" ON)
option(LUMEX_BUILD_OPTIONAL "Build core/optional" ON)
option(LUMEX_BUILD_REFLECTION "Build core/reflection" ON)
option(LUMEX_BUILD_STRING "Build core/string" ON)
option(LUMEX_BUILD_STRING_VIEW "Build core/string_view" ON)
option(LUMEX_BUILD_TEMPORARY "Build core/temporary" ON)
option(LUMEX_BUILD_TIME "Build core/time" ON)
option(LUMEX_BUILD_UTILITY "Build core/utility" ON)

# Applied
option(LUMEX_BUILD_HARDWARE "Build applied/hardware" ON)
option(LUMEX_BUILD_JSON "Build applied/json (header-only; needs the consumer's nlohmann/json)" ON)
option(LUMEX_BUILD_LOGGER "Build applied/logger" ON)
option(LUMEX_BUILD_LOGGING "Build applied/logging" ON)
option(LUMEX_BUILD_RESOURCE_MONITOR "Build applied/resource_monitor" ON)
option(LUMEX_BUILD_SERIAL "Build applied/serial" ON)
option(LUMEX_BUILD_SETTINGS "Build applied/settings" ON)

# The logger compiles exactly one config reader. Changing the format requires
# reconfiguring and rebuilding the logger target. PLAIN_TEXT has no additional
# dependency; INI uses lumex::settings; JSON uses vendored nlohmann/json; YAML
# throws until LumexSettingsYAML exists; XML uses lumex::xml.
set(LUMEX_LOGGER_CONFIG_FORMAT "PLAIN_TEXT" CACHE STRING
    "Logger config format: PLAIN_TEXT, INI, JSON, YAML, or XML")
set_property(CACHE LUMEX_LOGGER_CONFIG_FORMAT PROPERTY STRINGS
    PLAIN_TEXT INI JSON YAML XML)

option(LUMEX_XML_WCHAR_MODE "Build Xml with wchar_t mode" OFF)
option(LUMEX_WITH_FIELD_REFLECTION "Enable field reflection to_json via LumexAggregateFields + vendored nlohmann/json" ON)

# ==============================================================================
# DOCUMENTATION / EXAMPLES / TESTS
# ==============================================================================
option(LUMEX_BUILD_DOCUMENTATION "Build Doxygen documentation" OFF)
option(LUMEX_BUILD_EXAMPLES "Build the examples for the LumexLib (also implied by LUMEX_BUILD_TESTS)" OFF)
option(LUMEX_BUILD_TESTS "Build the tests for LumexCore (also compiles and runs lumex/examples)" OFF)

# ==============================================================================
# INSTALL
# ==============================================================================
# ON when LumexLib is the top-level project, OFF when a parent embeds it with
# add_subdirectory() (the root CMakeLists.txt sets LUMEX_IS_TOP_LEVEL before
# project()). A parent that wants Lumex headers, libraries and the package
# config in its own `cmake --install` passes -DLUMEX_INSTALL=ON.
if(LUMEX_IS_TOP_LEVEL)
    set(_lumex_install_default ON)
else()
    set(_lumex_install_default OFF)
endif()
option(LUMEX_INSTALL "Generate LumexLib install() rules (default: ON only when LumexLib is the top-level project)" ${_lumex_install_default})
unset(_lumex_install_default)

# ==============================================================================
# OPTIMIZATION (CMakeRoutines configure_optimization_level)
# ==============================================================================
# Standard:   O1, x86-64 baseline                 - quick build
# Portable:   O2, x86-64, generic                 - portable Release (DEFAULT)
# Aggressive: O3, x86-64-v2, optional LTO         - portable speed
# Maximum:    O3, native, fast-math, LTO          - forbidden here (IEEE APIs)
# MinSize:    Os, no-unroll                       - minimum binary size
# Debug / RelWithDebInfo / MinSizeRel stay on the module's fixed per-config map.
set(LUMEX_OPTIMIZATION_LEVEL "Portable" CACHE STRING
    "Release optimization level: Standard, Portable (default), Aggressive, Maximum (rejected), MinSize")
set_property(CACHE LUMEX_OPTIMIZATION_LEVEL PROPERTY STRINGS
    Portable Standard Aggressive Maximum MinSize)

option(LUMEX_ENABLE_LTO
    "Link-time optimization (ignored for Portable; do not enable CMAKE_INTERPROCEDURAL_OPTIMIZATION)"
    OFF)
option(LUMEX_DEBUG_SYMBOLS "Emit debug symbols on compiled Lumex targets" ON)

# ==============================================================================
# SANITIZERS (CMakeRoutines testing/SanitizersConfig.cmake)
# ==============================================================================
# Default OFF. When any of these is ON, lumex_configure_all_compiled_targets
# applies the same flags to libraries, examples, test executables, and the
# vendored gtest static libs. ASan+TSan is a configure FATAL_ERROR (shadow
# memory). TSan is Linux/macOS only (FATAL_ERROR on WIN32).
option(LUMEX_USE_ASAN "Enable AddressSanitizer (CMakeRoutines configure_sanitizers)" OFF)
option(LUMEX_USE_UBSAN "Enable UndefinedBehaviorSanitizer (CMakeRoutines configure_sanitizers)" OFF)
option(LUMEX_USE_TSAN "Enable ThreadSanitizer (Linux/macOS; CMakeRoutines configure_sanitizers)" OFF)

# ==============================================================================
# STATIC ANALYSIS / ISO DIAGNOSTICS (CMakeRoutines analysis/)
# ==============================================================================
# Default OFF. clang-tidy / cppcheck FATAL_ERROR if the tool is missing
# when the matching option is ON. MaximumStandardCompliance is GNU/Clang
# only and is applied only to targets that already have CXX_STANDARD
# (do not pin a project-wide standard).
option(LUMEX_USE_CLANG_TIDY "Run clang-tidy on compiled Lumex targets (CMakeRoutines configure_static_analysis)" OFF)
option(LUMEX_USE_CPPCHECK "Run cppcheck on compiled Lumex targets (CMakeRoutines configure_static_analysis)" OFF)
set(LUMEX_STATIC_ANALYSIS_PROFILE "default" CACHE STRING
    "StaticAnalysisConfig PROFILE: memory, undefined, thread, security, full, default")
set_property(CACHE LUMEX_STATIC_ANALYSIS_PROFILE PROPERTY STRINGS
    default memory undefined thread security full)
option(LUMEX_MAXIMUM_STANDARD_COMPLIANCE "Apply CMakeRoutines maximum_standard_compliance_configure to targets that already pin CXX_STANDARD" ON)

# ==============================================================================
# BUILD UTILS (CMakeRoutines utils/)
# ==============================================================================
# Default OFF. BuildTiming needs Ninja/Makefiles for per-rule times (VS/Xcode
# ignore RULE_LAUNCH_*). GenerateBuildInfo writes Lumex*-build-info.<fmt>
# next to each library binary when ON; PublishDistr only stages *.json.
# WarningSuppression / RecursiveSourceCollection are included from LumexBuild
# so their commands exist; vendored gtest uses suppress_warnings when present.
option(LUMEX_BUILD_TIMING "Prefix compile/link with cmake -E time (CMakeRoutines configure_build_timing)" OFF)
option(LUMEX_GENERATE_BUILD_INFO "Write per-library build-info via CMakeRoutines generate_build_info_file + POST_BUILD" OFF)
set(LUMEX_BUILD_INFO_FORMAT "json" CACHE STRING
    "Build-info format when LUMEX_GENERATE_BUILD_INFO=ON: txt, json, yaml, ini")
set_property(CACHE LUMEX_BUILD_INFO_FORMAT PROPERTY STRINGS txt json yaml ini)
