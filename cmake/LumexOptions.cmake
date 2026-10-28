# cmake/LumexOptions.cmake
#
# All project option() declarations live here. Include this file from the
# root CMakeLists.txt after project() and before any branch that reads
# a LUMEX_BUILD_* (per-module or tests/examples/docs) / LUMEX_XML_* /
# LUMEX_WITH_* / LUMEX_OPTIMIZATION_* /
# LUMEX_ENABLE_LTO / LUMEX_DEBUG_SYMBOLS /
# LUMEX_USE_ASAN / LUMEX_USE_UBSAN / LUMEX_USE_TSAN /
# LUMEX_USE_CLANG_TIDY / LUMEX_USE_CPPCHECK /
# LUMEX_MAXIMUM_STANDARD_COMPLIANCE / LUMEX_WERROR /
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
option(LUMEX_BUILD_ATOMIC "Build core/atomic (header-only atomic_shared_ptr / atomic_weak_ptr)" ON)
option(LUMEX_BUILD_BASE64 "Build core/base64" ON)
option(LUMEX_BUILD_CIRCULAR_BUFFER "Build core/circular_buffer" ON)
option(LUMEX_BUILD_CONTRACTS "Build core/contracts (LUMEX_CONTRACT_ASSERT, the contract assertion of C++26, from C++11)" ON)
option(LUMEX_BUILD_CRC "Build core/crc" ON)
option(LUMEX_BUILD_ENVIRONMENT "Build core/environment" ON)
option(LUMEX_BUILD_EXCEPTIONS "Build core/exceptions" ON)
option(LUMEX_BUILD_EXPECTED "Build core/expected" ON)
option(LUMEX_BUILD_FILESYSTEM "Build core/filesystem" ON)
option(LUMEX_BUILD_FMT "Build core/fmt (std::format-style formatting)" ON)
option(LUMEX_BUILD_FRAMING "Build core/framing (frame codecs: SLIP, COBS, delimiter, length prefix, header, Modbus RTU, stages)" ON)
option(LUMEX_BUILD_GENERATORS "Build core/generators" ON)
option(LUMEX_BUILD_HAZARD_POINTER "Build core/hazard_pointer (hazard pointers from C++11, the interface of C++26 <hazard_pointer>)" ON)
option(LUMEX_BUILD_INTERPROCESS "Build core/interprocess (an inter-process file lock released by the system when its process ends)" ON)
option(LUMEX_BUILD_MATH "Build core/math" ON)
option(LUMEX_BUILD_OPTIONAL "Build core/optional" ON)
option(LUMEX_BUILD_PROTOCOL "Build core/protocol (exchange protocols described as tables: generated command types, names, run-time entries)" ON)
option(LUMEX_PROTOCOL_WITH_JSON "core/protocol: export_json (defines LUMEX_PROTOCOL_WITH_JSON; the consumer brings nlohmann/json 3.x, like applied/json)" ON)
option(LUMEX_BUILD_REFLECTION "Build core/reflection" ON)
option(LUMEX_BUILD_SMART_PTR "Build core/smart_ptr (header-only shared_ptr / weak_ptr family with the own split-count control block, from C++11)" ON)
option(LUMEX_BUILD_SPAN "Build core/span (header-only C++11 backport of std::span)" ON)
option(LUMEX_BUILD_STRING "Build core/string" ON)
option(LUMEX_BUILD_STRING_VIEW "Build core/string_view" ON)
option(LUMEX_BUILD_TEMPORARY "Build core/temporary" ON)
option(LUMEX_BUILD_TIME "Build core/time" ON)
option(LUMEX_BUILD_UNICODE "Build core/unicode (header-only UTF-8/16/32 and Latin-1 transcoding)" ON)
option(LUMEX_BUILD_UTILITY "Build core/utility" ON)

# Applied
option(LUMEX_BUILD_HARDWARE "Build applied/hardware" ON)
option(LUMEX_BUILD_JSON "Build applied/json (header-only; needs the consumer's nlohmann/json)" ON)
option(LUMEX_BUILD_LOGGER "Build applied/logger" ON)
option(LUMEX_BUILD_LOGGING "Build applied/logging" ON)
option(LUMEX_BUILD_RESOURCE_MONITOR "Build applied/resource_monitor" ON)
option(LUMEX_BUILD_SERIAL "Build applied/serial" ON)
option(LUMEX_BUILD_TRANSPORT "Build applied/transport (byte streams and message transports over the vendored Asio: serial, TCP, UDP; always a static library)" ON)
option(LUMEX_TRANSPORT_WITH_SPAWN "applied/transport: the asio::yield_context completion token (builds the vendored Boost.Context)" ON)
set(LUMEX_TRANSPORT_LOG_MIN_LEVEL "TRACE" CACHE STRING
    "applied/transport: the lowest log level compiled in (TRACE, DEBUG, INFO, WARNING, ERROR)")
set_property(CACHE LUMEX_TRANSPORT_LOG_MIN_LEVEL PROPERTY STRINGS
    TRACE DEBUG INFO WARNING ERROR)
option(LUMEX_TRANSPORT_WITH_TESTING "applied/transport: the public test doubles (mock streams and datagrams)" ON)
# No option of its own: with LUMEX_BUILD_LOGGER the transport also compiles the
# adapter to the logger (log/adapter: install_lumex_logger_sink,
# lumex_logger_watch_timer), links lumex::logger and defines
# LUMEX_TRANSPORT_WITH_LOGGER=1 for itself and its consumers.
# The framed adapters (framed/: framed_stream, framed_datagram) are wired the
# same way: no option of their own, no required edge. With LUMEX_BUILD_FRAMING
# the transport compiles them, links lumex::framing and defines
# LUMEX_TRANSPORT_WITH_FRAMING=1 for itself and its consumers.
option(LUMEX_BUILD_SETTINGS "Build applied/settings" ON)

# The logger compiles exactly one config reader. Changing the format requires
# reconfiguring and rebuilding the logger target. PLAIN_TEXT has no additional
# dependency; INI uses lumex::settings; JSON uses vendored nlohmann/json; YAML
# throws until LumexSettingsYAML exists; XML uses lumex::xml.
set(LUMEX_LOGGER_CONFIG_FORMAT "PLAIN_TEXT" CACHE STRING
    "Logger config format: PLAIN_TEXT, INI, JSON, YAML, or XML")
set_property(CACHE LUMEX_LOGGER_CONFIG_FORMAT PROPERTY STRINGS
    PLAIN_TEXT INI JSON YAML XML)

# The evaluation semantic of LUMEX_CONTRACT_ASSERT for the whole build (target
# lumex::contracts passes it to every translation unit that links the target;
# a translation unit may override it with the macro LUMEX_CONTRACTS_SEMANTIC).
# IGNORE never evaluates the predicate; OBSERVE reports a false predicate to the
# violation handler and continues; ENFORCE (default) reports and aborts;
# QUICK_ENFORCE stops at once without a handler; P2900 uses the compiler's own
# contract_assert where the compiler has one (enforce otherwise).
set(LUMEX_CONTRACTS_SEMANTIC "ENFORCE" CACHE STRING
    "Evaluation semantic of LUMEX_CONTRACT_ASSERT: IGNORE, OBSERVE, ENFORCE, QUICK_ENFORCE, or P2900")
set_property(CACHE LUMEX_CONTRACTS_SEMANTIC PROPERTY STRINGS
    IGNORE OBSERVE ENFORCE QUICK_ENFORCE P2900)

option(LUMEX_WITH_FIELD_REFLECTION "Enable field reflection to_json via LumexAggregateFields + vendored nlohmann/json" ON)

# ==============================================================================
# DOCUMENTATION / EXAMPLES / TESTS
# ==============================================================================
option(LUMEX_BUILD_DOCUMENTATION "Build Doxygen documentation" OFF)
option(LUMEX_BUILD_EXAMPLES "Build the examples for the LumexLib (also implied by LUMEX_BUILD_TESTS)" OFF)
option(LUMEX_BUILD_TESTS "Build the tests for LumexCore (also compiles and runs lumex/examples)" OFF)
option(LUMEX_BUILD_RELACY "Build the Relacy memory-order models of the split-count engine (needs LUMEX_BUILD_TESTS; uses 3rdparty/relacy, or LUMEX_RELACY_DIR when it is given)" OFF)
option(LUMEX_BUILD_BENCHMARKS "Build the benchmarks under benchmarks/ (Release builds only make sense)" OFF)
option(LUMEX_BUILD_FUZZERS "Build the libFuzzer targets under lumex/tests/fuzz (Clang only, needs LUMEX_BUILD_TESTS; CTest label fuzz: a short bounded run per target, not in the default run)" OFF)
option(LUMEX_BUILD_SOAK_TESTS "Register the long soak and ThreadSanitizer runs (CTest labels soak and tsan: core/hazard_pointer and the concurrency tests of core/atomic; not in the default run; needs LUMEX_BUILD_TESTS; the soak registration of the atomic tests needs CMake 3.22 or later; LUMEX_TEST_SOAK_SECONDS sets their length)" OFF)

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

# C++ standard library under Clang (CMakeRoutines configure_optimization_level
# CXX_STDLIB). The top-level project chooses: AUTO (libc++ when a program
# built with it compiles and links, else the compiler default) when LumexLib
# is the top-level project, DEFAULT (no -stdlib, the parent's library) when a
# parent embeds it. LIBCXX requires libc++. A library built with libc++ hands
# -stdlib=libc++ to its consumers. GCC and MSVC ignore the option.
if(LUMEX_IS_TOP_LEVEL)
    set(_lumex_clang_stdlib_default AUTO)
else()
    set(_lumex_clang_stdlib_default DEFAULT)
endif()
set(LUMEX_CLANG_STDLIB "${_lumex_clang_stdlib_default}" CACHE STRING
    "C++ standard library under Clang: AUTO, LIBCXX or DEFAULT (default: AUTO at top level, DEFAULT when embedded)")
set_property(CACHE LUMEX_CLANG_STDLIB PROPERTY STRINGS AUTO LIBCXX DEFAULT)
unset(_lumex_clang_stdlib_default)
string(TOUPPER "${LUMEX_CLANG_STDLIB}" _lumex_clang_stdlib_upper)
if(NOT _lumex_clang_stdlib_upper MATCHES "^(AUTO|LIBCXX|DEFAULT)$")
    message(FATAL_ERROR
        "Invalid LUMEX_CLANG_STDLIB='${LUMEX_CLANG_STDLIB}': use AUTO, LIBCXX or DEFAULT")
endif()
unset(_lumex_clang_stdlib_upper)

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
# Default OFF. ON adds -Werror (/WX on MSVC and clang-cl) to the library
# targets, so a new warning fails the build. Tests and examples have their
# warnings switched off and are not affected. create_release.sh turns it ON.
option(LUMEX_WERROR "Treat compiler warnings of the Lumex library targets as errors" OFF)

# ==============================================================================
# BUILD UTILS (CMakeRoutines utils/)
# ==============================================================================
# Default OFF. BuildTiming needs Ninja/Makefiles for per-rule times (VS/Xcode
# ignore RULE_LAUNCH_*). GenerateBuildInfo writes Lumex*-build-info.<fmt>
# next to each library binary when ON; PublishDistr only stages *.json.
# WarningSuppression / RecursiveSourceCollection are included from LumexBuild
# so their commands exist; vendored gtest uses suppress_warnings when present.
option(LUMEX_BUILD_TIMING "Prefix compile/link with cmake -E time (CMakeRoutines configure_build_timing)" OFF)
option(LUMEX_GENERATE_BUILD_INFO "Write LumexLib-build-info.json, one CMakeRoutines build report for all the libraries (Distr and the package root)" ON)
set(LUMEX_BUILD_INFO_FORMAT "json" CACHE STRING
    "Format of the CMake-native per-target report CMakeRoutines writes into the build tree when no Python 3.8+ is found (LumexLib-build-info.json is always JSON): txt, json, yaml, ini")
set_property(CACHE LUMEX_BUILD_INFO_FORMAT PROPERTY STRINGS txt json yaml ini)

# ==============================================================================
# THIRD-PARTY CODE OF THE TRANSPORT LAYER (cmake/LumexAsio.cmake,
# cmake/LumexBoostContext.cmake, cmake/LumexHidapi.cmake)
# ==============================================================================
# Vendored under 3rdparty/ (Asio 1.36.0, Boost.Context 1.88.0, hidapi 0.15.0).
# An empty directory option means the vendored copy. A consumer that already
# ships one of these libraries points the option at its own source tree, which
# must have the same version (checked at configure time): a process that loads
# another library holds a second copy of Asio and hidapi, and both copies
# must be identical. The vendored targets build with every warning off.
set(LUMEX_ASIO_DIR "" CACHE PATH
    "Asio include directory (the one that contains asio.hpp); empty = 3rdparty/asio/include")
set(LUMEX_BOOST_CONTEXT_DIR "" CACHE PATH
    "Boost source root with boost/context and libs/context/src; empty = 3rdparty/boost_context")
set(LUMEX_HIDAPI_DIR "" CACHE PATH
    "hidapi source root (hidapi/hidapi.h, windows/, linux/, libusb/, mac/); empty = 3rdparty/hidapi")
set(LUMEX_HIDAPI_BACKEND "hidraw" CACHE STRING
    "hidapi backend on Linux and other non-Windows, non-Apple systems: hidraw (needs libudev) or libusb (needs libusb-1.0)")
set_property(CACHE LUMEX_HIDAPI_BACKEND PROPERTY STRINGS hidraw libusb)

# The HID transport (applied/transport/hid: hid_device over the vendored
# hidapi). On by default where hidapi builds: always on Windows and Apple
# systems; elsewhere when pkg-config finds the library of the chosen backend
# (libudev for hidraw, libusb-1.0 for libusb). When it does not, the default
# is OFF and the configure says why (a warning for libusb, which only an
# explicit LUMEX_HIDAPI_BACKEND=libusb selects; a status line for the default
# hidraw). Set LUMEX_TRANSPORT_WITH_HID=ON explicitly to get the backend's
# error instead. lumex_hid_default_decision (cmake/LumexHidDefault.cmake) is
# the pure decision, so a test walks every combination without the libraries.
include("${CMAKE_CURRENT_LIST_DIR}/LumexHidDefault.cmake")

if(NOT DEFINED LUMEX_TRANSPORT_WITH_HID)
    set(_lumex_hid_own_backend OFF)
    if(WIN32 OR APPLE)
        set(_lumex_hid_own_backend ON)
    endif()
    set(_lumex_hid_pkg_config OFF)
    set(_lumex_hid_library OFF)
    if(NOT _lumex_hid_own_backend)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            set(_lumex_hid_pkg_config ON)
            if(LUMEX_HIDAPI_BACKEND STREQUAL "libusb")
                pkg_check_modules(_LUMEX_HID_PROBE QUIET libusb-1.0)
            else()
                pkg_check_modules(_LUMEX_HID_PROBE QUIET libudev)
            endif()
            if(_LUMEX_HID_PROBE_FOUND)
                set(_lumex_hid_library ON)
            endif()
        endif()
    endif()
    lumex_hid_default_decision(_lumex_hid_default _lumex_hid_message
        "${LUMEX_HIDAPI_BACKEND}" ${_lumex_hid_own_backend}
        ${_lumex_hid_pkg_config} ${_lumex_hid_library})
    if(_lumex_hid_message)
        if(LUMEX_HIDAPI_BACKEND STREQUAL "libusb")
            message(WARNING "${_lumex_hid_message}")
        else()
            message(STATUS "${_lumex_hid_message}")
        endif()
    endif()
    unset(_lumex_hid_own_backend)
    unset(_lumex_hid_pkg_config)
    unset(_lumex_hid_library)
    unset(_lumex_hid_message)
endif()
option(LUMEX_TRANSPORT_WITH_HID "applied/transport: hid_device over the vendored hidapi (default ON where hidapi builds)" ${_lumex_hid_default})
unset(_lumex_hid_default)
