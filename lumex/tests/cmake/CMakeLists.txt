# lumex/tests/cmake/CMakeLists.txt
#
# Tests the CMake option helpers and keeps subdirectory wiring from
# drifting off cmake/LumexOptions.cmake. These are cmake -P scripts,
# not GoogleTest binaries. They register as CTest names cmake.<case> (the
# prefix of this directory, lumex_test_name) and run with the rest of the
# suite when LUMEX_BUILD_TESTS=ON.

# This directory is added before lumex/tests includes LumexGoogleTest.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/LumexTestNames.cmake")

set(_LUMEX_CMAKE_RUN "${CMAKE_CURRENT_SOURCE_DIR}/run_one.cmake")

function(lumex_add_cmake_test name)
    lumex_test_name(_ctest_name ${name})
    add_test(NAME ${_ctest_name}
        COMMAND ${CMAKE_COMMAND}
            -DCASE=${name}
            -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
            -P ${_LUMEX_CMAKE_RUN}
    )
    set_tests_properties(${_ctest_name} PROPERTIES
        LABELS "cmake"
        TIMEOUT 30
    )
endfunction()

function(lumex_add_cmake_fail_test name pattern)
    lumex_test_name(_ctest_name ${name})
    add_test(NAME ${_ctest_name}
        COMMAND ${CMAKE_COMMAND}
            -DCASE=${name}
            -DCASE_EXPECT_FAIL=1
            -DCASE_FAIL_PATTERN=${pattern}
            -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
            -P ${_LUMEX_CMAKE_RUN}
    )
    set_tests_properties(${_ctest_name} PROPERTIES
        LABELS "cmake"
        TIMEOUT 30
    )
endfunction()

lumex_add_cmake_test(options_declared)
lumex_add_cmake_test(effective_flags)
lumex_add_cmake_test(effective_matrix)
lumex_add_cmake_test(effective_group_off_keeps_module_var)
lumex_add_cmake_test(double_include_modules)
lumex_add_cmake_test(require_graph_all_edges)
lumex_add_cmake_test(require_ok)
lumex_add_cmake_test(require_ok_applied_off)
lumex_add_cmake_test(require_ok_xml_off)
lumex_add_cmake_test(require_ok_xml_on_applied_off)
lumex_add_cmake_test(require_ok_all_groups_off)
lumex_add_cmake_test(require_ok_independents_off)
lumex_add_cmake_test(require_ok_math_off_utility_off)
lumex_add_cmake_test(require_ok_time_off_without_time_consumers)
lumex_add_cmake_test(require_ok_string_off_without_string_consumers)
lumex_add_cmake_test(require_ok_filesystem_off_without_fs_consumers)
lumex_add_cmake_test(require_ok_only_math)
lumex_add_cmake_test(require_ok_only_utility_and_math)
lumex_add_cmake_test(require_ok_logger_without_core)
lumex_add_cmake_test(require_ok_logger_and_circular_buffer_without_core)
lumex_add_cmake_test(require_ok_logger_ini)
lumex_add_cmake_test(require_ok_logger_xml)
lumex_add_cmake_test(wiring_subdirs)
lumex_add_cmake_test(wiring_distr)
lumex_add_cmake_test(wiring_install_umbrellas)
lumex_add_cmake_test(wiring_license_install)
lumex_add_cmake_test(publish_distr_filters)
lumex_add_cmake_test(cpack_nsis)
lumex_add_cmake_test(dev_artifacts)
lumex_add_cmake_test(install_bin_runtime)
lumex_add_cmake_test(release_tag_build)
lumex_add_cmake_test(wiring_field_reflection)
lumex_add_cmake_test(wiring_field_reflection_no_leak)
lumex_add_cmake_test(wiring_sanitizers)
lumex_add_cmake_test(options_field_reflection_and_nlohmann)
lumex_add_cmake_test(sanitizer_asan_release_warns)
lumex_add_cmake_test(sanitizer_ubsan_release_warns)
lumex_add_cmake_test(sanitizer_tsan_release_warns)
lumex_add_cmake_test(sanitizer_asan_minsizerel_warns)
lumex_add_cmake_test(sanitizer_ubsan_minsizerel_warns)
lumex_add_cmake_test(sanitizer_tsan_minsizerel_warns)
lumex_add_cmake_test(sanitizer_any_release_warns)
lumex_add_cmake_test(sanitizer_debug_silent)
lumex_add_cmake_test(sanitizer_relwithdebinfo_silent)
lumex_add_cmake_test(sanitizer_off_release_silent)
lumex_add_cmake_test(sanitizer_off_minsizerel_silent)
lumex_add_cmake_test(sanitizer_multiconfig_release_warns)
lumex_add_cmake_test(sanitizer_multiconfig_minsizerel_warns)
lumex_add_cmake_test(sanitizer_multiconfig_debug_only_silent)
lumex_add_cmake_test(wiring_build_info)
lumex_add_cmake_test(wiring_msvc_vcvars)
lumex_add_cmake_test(wiring_version_config)
lumex_add_cmake_test(wiring_static_analysis)
lumex_add_cmake_test(wiring_max_standard_compliance)
lumex_add_cmake_test(wiring_werror)
lumex_add_cmake_test(wiring_install_dirs_order)
lumex_add_cmake_test(lint_msvc_pragmas_script)
lumex_add_cmake_test(lint_headers_standalone_script)
lumex_add_cmake_test(release_diagnostics_script)
lumex_add_cmake_test(release_script_options)
lumex_add_cmake_test(release_script_dry_run)
lumex_add_cmake_test(wiring_build_timing)
lumex_add_cmake_test(wiring_generate_build_info)
lumex_add_cmake_test(wiring_warning_suppression)
lumex_add_cmake_test(wiring_recursive_source_collection)
lumex_add_cmake_test(wiring_compile_commands)
lumex_add_cmake_test(wiring_export_namespace)
lumex_add_cmake_test(wiring_rc_windows_only)
lumex_add_cmake_test(wiring_posix_system_libraries)
lumex_add_cmake_test(source_no_inline_variable_in_cpp)
lumex_add_cmake_test(source_no_public_api_inline_in_cpp)
lumex_add_cmake_test(source_no_public_api_inline_declaration)
lumex_add_cmake_test(source_seh_translator_branches)
lumex_add_cmake_test(source_function_name_by_compiler)
lumex_add_cmake_test(source_crc_spec_storage_complete)
lumex_add_cmake_test(source_enumeration_uses_bounded_open)
lumex_add_cmake_test(wiring_xml_no_keep_flags)
lumex_add_cmake_test(wiring_xml_no_wchar_mode)
lumex_add_cmake_test(source_std_array_included)
lumex_add_cmake_test(source_xml_examples_own_files)
lumex_add_cmake_test(wiring_gtest_cxx_stdlib)
lumex_add_cmake_test(wiring_embedding)
lumex_add_cmake_test(wiring_include_order_ctest)
lumex_add_cmake_test(wiring_headers_standalone_ctest)
lumex_add_cmake_test(wiring_clang_stdlib)
lumex_add_cmake_test(wiring_toolchain_rpath)
lumex_add_cmake_test(asan_clang_cl_runtime_library)
lumex_add_cmake_test(gtest_discovery_does_not_lock_exe)
lumex_add_cmake_test(wiring_atomic)
lumex_add_cmake_test(wiring_math_constants)
lumex_add_cmake_test(wiring_ordering)
lumex_add_cmake_test(wiring_span)
lumex_add_cmake_test(wiring_string_view)
lumex_add_cmake_test(wiring_string_view_export)
lumex_add_cmake_test(wiring_unicode_users)
lumex_add_cmake_test(wiring_hazard_pointer)
lumex_add_cmake_test(wiring_hazard_pointer_export)
lumex_add_cmake_test(wiring_optional)
lumex_add_cmake_test(wiring_test_names)
lumex_add_cmake_test(wiring_standard_suites)
lumex_add_cmake_test(wiring_exception_crash_lock)
lumex_add_cmake_test(wiring_expected_global_names)
lumex_add_cmake_test(wiring_path_compile_checks)

# configure_msvc_vcvars is a no-op on non-Windows hosts, so the guard against
# calling it after project() can only fire on Windows.
if(CMAKE_HOST_WIN32)
    lumex_add_cmake_fail_test(wiring_msvc_vcvars_after_project
        "must be called BEFORE project")
endif()

lumex_add_cmake_fail_test(wiring_test_names_outside_root
    "the directory is not under the root")
lumex_add_cmake_fail_test(require_fail_base64_without_utility
    "LUMEX_BUILD_BASE64 is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_crc_without_utility
    "LUMEX_BUILD_CRC is enabled but requires LUMEX_BUILD_UTILITY")
# base64, crc, exceptions and xml link the string_view module (the views their
# string overloads take below C++17). Each case switches the modules checked
# before it off, so that the module under test is the one that fails.
lumex_add_cmake_fail_test(require_fail_base64_without_string_view
    "LUMEX_BUILD_BASE64 is enabled but requires LUMEX_BUILD_STRING_VIEW")
lumex_add_cmake_fail_test(require_fail_crc_without_string_view
    "LUMEX_BUILD_CRC is enabled but requires LUMEX_BUILD_STRING_VIEW")
lumex_add_cmake_fail_test(require_fail_xml_without_string_view
    "LUMEX_BUILD_XML is enabled but requires LUMEX_BUILD_STRING_VIEW")
lumex_add_cmake_fail_test(require_fail_exceptions_without_string_view
    "LUMEX_BUILD_EXCEPTIONS is enabled but requires LUMEX_BUILD_STRING_VIEW")
# base64 and crc link the span module too, but utility requires it as well and
# is checked first, so only that edge can be tested alone
# (cmake.wiring_span pins the other two).
lumex_add_cmake_fail_test(require_fail_utility_without_span
    "LUMEX_BUILD_UTILITY is enabled but requires LUMEX_BUILD_SPAN")
lumex_add_cmake_fail_test(require_fail_utility_without_optional
    "LUMEX_BUILD_UTILITY is enabled but requires LUMEX_BUILD_OPTIONAL")
lumex_add_cmake_fail_test(require_fail_environment_without_utility
    "LUMEX_BUILD_ENVIRONMENT is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_filesystem_without_utility
    "LUMEX_BUILD_FILESYSTEM is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_fmt_without_utility
    "LUMEX_BUILD_FMT is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_string_view_without_utility
    "LUMEX_BUILD_STRING_VIEW is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_reflection_without_utility
    "LUMEX_BUILD_REFLECTION is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_utility_without_math
    "LUMEX_BUILD_UTILITY is enabled but requires LUMEX_BUILD_MATH")
# hazard_pointer links utility and span; utility requires span itself and is
# checked first, so only the utility edge can fail alone (cmake.wiring_hazard_pointer
# pins the other).
lumex_add_cmake_fail_test(require_fail_hazard_pointer_without_utility
    "LUMEX_BUILD_HAZARD_POINTER is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_serial_without_core
    "LUMEX_BUILD_SERIAL is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_xml_without_core
    "LUMEX_BUILD_XML is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_xml_without_utility
    "LUMEX_BUILD_XML is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_xml_without_unicode
    "LUMEX_BUILD_XML is enabled but requires LUMEX_BUILD_UNICODE")
lumex_add_cmake_fail_test(require_fail_unicode_without_utility
    "LUMEX_BUILD_UNICODE is enabled but requires LUMEX_BUILD_UTILITY")
# filesystem, serial, hardware and resource_monitor link unicode (their sources
# convert wchar_t text with to_utf8 / to_wide). Each case switches off the
# modules checked before it (xml, then the other three in the order of
# cmake/LumexModules.cmake), so that the module under test is the one that fails.
lumex_add_cmake_fail_test(require_fail_hardware_without_unicode
    "LUMEX_BUILD_HARDWARE is enabled but requires LUMEX_BUILD_UNICODE")
lumex_add_cmake_fail_test(require_fail_resource_monitor_without_unicode
    "LUMEX_BUILD_RESOURCE_MONITOR is enabled but requires LUMEX_BUILD_UNICODE")
lumex_add_cmake_fail_test(require_fail_serial_without_unicode
    "LUMEX_BUILD_SERIAL is enabled but requires LUMEX_BUILD_UNICODE")
lumex_add_cmake_fail_test(require_fail_filesystem_without_unicode
    "LUMEX_BUILD_FILESYSTEM is enabled but requires LUMEX_BUILD_UNICODE")
lumex_add_cmake_fail_test(require_fail_core_off_xml_applied_off
    "LUMEX_BUILD_XML is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_time_without_environment
    "LUMEX_BUILD_TIME is enabled but requires LUMEX_BUILD_ENVIRONMENT")
lumex_add_cmake_fail_test(require_fail_temporary_without_environment
    "LUMEX_BUILD_TEMPORARY is enabled but requires LUMEX_BUILD_ENVIRONMENT")
lumex_add_cmake_fail_test(require_fail_temporary_without_filesystem
    "LUMEX_BUILD_TEMPORARY is enabled but requires LUMEX_BUILD_FILESYSTEM")
lumex_add_cmake_fail_test(require_fail_exceptions_without_time
    "LUMEX_BUILD_EXCEPTIONS is enabled but requires LUMEX_BUILD_TIME")
lumex_add_cmake_fail_test(require_fail_exceptions_without_string
    "LUMEX_BUILD_EXCEPTIONS is enabled but requires LUMEX_BUILD_STRING")
lumex_add_cmake_fail_test(require_fail_logging_without_filesystem
    "LUMEX_BUILD_LOGGING is enabled but requires LUMEX_BUILD_FILESYSTEM")
lumex_add_cmake_fail_test(require_fail_logging_without_string
    "LUMEX_BUILD_LOGGING is enabled but requires LUMEX_BUILD_STRING")
lumex_add_cmake_fail_test(require_fail_hardware_without_logging
    "LUMEX_BUILD_HARDWARE is enabled but requires LUMEX_BUILD_LOGGING")
lumex_add_cmake_fail_test(require_fail_resource_monitor_without_expected
    "LUMEX_BUILD_RESOURCE_MONITOR is enabled but requires LUMEX_BUILD_EXPECTED")
lumex_add_cmake_fail_test(require_fail_resource_monitor_without_logging
    "LUMEX_BUILD_RESOURCE_MONITOR is enabled but requires LUMEX_BUILD_LOGGING")
lumex_add_cmake_fail_test(require_fail_settings_without_logging
    "LUMEX_BUILD_SETTINGS is enabled but requires LUMEX_BUILD_LOGGING")
lumex_add_cmake_fail_test(require_fail_json_without_reflection
    "LUMEX_BUILD_JSON is enabled but requires LUMEX_BUILD_REFLECTION")
lumex_add_cmake_fail_test(require_fail_json_without_string_view
    "LUMEX_BUILD_JSON is enabled but requires LUMEX_BUILD_STRING_VIEW")
lumex_add_cmake_fail_test(require_fail_logger_ini_without_settings
    "LUMEX_LOGGER_CONFIG_FORMAT=INI requires LUMEX_BUILD_SETTINGS")
lumex_add_cmake_fail_test(require_fail_logger_xml_without_xml
    "LUMEX_LOGGER_CONFIG_FORMAT=XML requires LUMEX_BUILD_XML")
lumex_add_cmake_fail_test(require_fail_logger_invalid_format
    "Invalid LUMEX_LOGGER_CONFIG_FORMAT='TOML'")

# Consumer smoke: configure, build and run a tiny project that embeds
# LumexLib and includes the reflection umbrella with lumex::reflection alone
# (no nlohmann), once with LUMEX_WITH_FIELD_REFLECTION OFF and once ON. The
# nested build reuses this tree's generator and compiler and the calling
# shell's environment (MSVC: run ctest from a developer shell, otherwise the
# case is reported as skipped).
set(_LUMEX_CONSUMER_RUN "${CMAKE_CURRENT_SOURCE_DIR}/consumer/run_consumer.cmake")

# A compiler outside the system paths keeps its C++ runtime (libstdc++ and
# libgcc_s, or libc++, libc++abi and libunwind) outside the loader's
# directories. The fixture's own executable is not configured by LumexBuild, so
# the nested build gets the directories as CMAKE_BUILD_RPATH; both libraries,
# because a fixture that links a Lumex library takes libc++ from it and one that
# links header-only modules keeps the compiler's default.
get_toolchain_runtime_directories(_LUMEX_CONSUMER_RPATH STDLIB libstdc++ libc++)

function(lumex_add_consumer_test name fixture)
    # The nested build uses this tree's C++ flags (a --gcc-toolchain, for
    # example) and its Clang standard library choice, so it builds like the
    # tree that runs it, and runs with the runtime of this tree's compiler.
    lumex_test_name(_ctest_name ${name})
    add_test(NAME ${_ctest_name}
        COMMAND ${CMAKE_COMMAND}
            -DCASE=${fixture}
            -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/${name}
            -DGENERATOR=${CMAKE_GENERATOR}
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
            -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
            "-DBUILD_RPATH=${_LUMEX_CONSUMER_RPATH}"
            -DBUILD_TYPE=Release
            "-DEXTRA_ARGS=${ARGN}"
            -P ${_LUMEX_CONSUMER_RUN}
    )
    set_tests_properties(${_ctest_name} PROPERTIES
        LABELS "cmake;consumer"
        TIMEOUT 900
        SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
        RUN_SERIAL TRUE
    )
endfunction()

lumex_add_consumer_test(consumer_reflection_umbrella_field_reflection_off
    reflection_umbrella -DLUMEX_WITH_FIELD_REFLECTION=OFF)
lumex_add_consumer_test(consumer_reflection_umbrella_field_reflection_on
    reflection_umbrella -DLUMEX_WITH_FIELD_REFLECTION=ON)
# MSVC consumers that set no /Zc:__cplusplus must still see the real
# __cplusplus in Lumex headers (the INTERFACE flag of every module target).
lumex_add_consumer_test(consumer_cplusplus_macro cplusplus_macro)
# Exported functions do not depend on the C++ standard: the library at one
# standard and the consumer at another link, both ways (base64, XML and
# exceptions, through their inline wrappers).
lumex_add_consumer_test(consumer_standard_mismatch_lib11_consumer20
    standard_mismatch -DLIB_STD=11 -DCONSUMER_STD=20)
lumex_add_consumer_test(consumer_standard_mismatch_lib20_consumer11
    standard_mismatch -DLIB_STD=20 -DCONSUMER_STD=11)
# Before C++20 the lock-based atomic smart pointers sleep on a table in a
# function-local static; two shared libraries built with hidden visibility
# must share it (skipped on Windows, where DLLs cannot).
lumex_add_consumer_test(consumer_atomic_cross_module atomic_cross_module)
# core/hazard_pointer: the deterministic tests of the Dekker race (the engine is
# built with test hooks and threads are stalled at named points), two shared
# libraries that share one domain, hazard pointers in static constructors and
# destructors and in thread-local destructors.
lumex_add_consumer_test(consumer_hazard_pointer_hooks hazard_pointer_hooks)
lumex_add_consumer_test(consumer_hazard_pointer_two_modules
    hazard_pointer_two_modules)
lumex_add_consumer_test(consumer_hazard_pointer_static_destruction
    hazard_pointer_static_destruction)
# The header-only modules (string, math, generators, optional, atomic, span) compile
# from an install prefix built with LUMEX_BUILD_UTILITY OFF: the core/utility
# headers they include are installed whatever that option says.
lumex_add_consumer_test(install_header_only_without_utility
    install_header_only_without_utility)

# LumexFormat compile-time checks: bad literal format strings must not
# compile at C++20 (consteval), good ones must. Header-only, so the fixture
# only includes the source tree and runs try_compile while configuring.
lumex_test_name(_ctest_name format_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=format_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/format_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# LUMEX_MATH_CONSTANTS_* compile-time checks: the constants are double
# literals, so the uses a literal rejects (an address, a non-const reference,
# an assignment, a modulo, a narrowing in braces, a value in an #if), a suffix
# that is not a floating one and a float or long double literal compared with
# the double macro must not compile with warnings as errors; valid use must.
lumex_test_name(_ctest_name math_constants_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=math_constants_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/math_constants_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# Ordering classes (LumexOrdering.hpp) and the three-way part of the safe
# comparator compile-time checks: the uses the standard orderings reject (a
# default constructor, a relational operator between two orderings or against
# a number other than the literal 0, a conversion to a stronger category) and
# the static assertions of the comparator must not compile with warnings as
# errors; valid use must.
lumex_test_name(_ctest_name ordering_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=ordering_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/ordering_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# atomic_shared_ptr / atomic_weak_ptr compile-time checks: discarded load ()
# and is_lock_free () results (nodiscard) and, with the diagnose_if attribute,
# constant memory orders the standard forbids must not compile with warnings
# as errors; valid use must.
lumex_test_name(_ctest_name atomic_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=atomic_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/atomic_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# LUMEX_DEFINE_FIELD_NAMES compile-time checks: a registration that does not
# match the aggregate (a wrong number of names, a name that is no member, an
# array member, a second registration) and a use of names, get or to_json
# without the registration where the standard gives no automatic source must
# be refused with the message of their own check; valid use must compile.
lumex_test_name(_ctest_name field_names_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=field_names_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/field_names_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# span compile-time checks: the uses std::span rejects (a discarded result,
# a constructor its constraints remove, an out-of-range static subview, an
# implicit conversion the standard makes explicit) must not compile with
# warnings as errors; valid use must.
lumex_test_name(_ctest_name span_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=span_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/span_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# lumex::path compile-time checks: a lumex path converts to std::string only
# explicitly (the implicit conversion made `std_path = lumex_path;` ambiguous on
# POSIX), so every implicit use must be refused at every standard, while the
# explicit forms and the three ways to make a std::filesystem::path from a
# lumex path (C++17) compile.
lumex_test_name(_ctest_name path_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=path_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/path_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# The long runs of the hazard pointer stress tests are outside the default run:
# CTest label soak (the ABA structures for LUMEX_HP_SOAK_SECONDS seconds) and
# label tsan (the same under ThreadSanitizer: Clang when there is one, GCC
# through `setarch x86_64 -R` otherwise, since GCC's runtime cannot map its
# shadow with ASLR on). Select them with `ctest -L soak` / `ctest -L tsan`.
if(LUMEX_BUILD_SOAK_TESTS)
    function(lumex_add_hazard_pointer_soak_test name label seconds sanitize compiler)
        lumex_test_name(_ctest_name ${name})
        add_test(NAME ${_ctest_name}
            COMMAND ${CMAKE_COMMAND}
                -DCASE=hazard_pointer_soak
                -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/${name}
                -DGENERATOR=${CMAKE_GENERATOR}
                -DCXX_COMPILER=${compiler}
                "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
                -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
                "-DBUILD_RPATH=${_LUMEX_CONSUMER_RPATH}"
                -DBUILD_TYPE=Release
                "-DEXTRA_ARGS=-DSANITIZE=${sanitize}"
                "-DRUN_PREFIX=${ARGN}"
                -P ${_LUMEX_CONSUMER_RUN}
        )
        set_tests_properties(${_ctest_name} PROPERTIES
            LABELS "cmake;consumer;${label}"
            ENVIRONMENT "LUMEX_HP_SOAK_SECONDS=${seconds}"
            TIMEOUT 3600
            SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
            RUN_SERIAL TRUE
        )
    endfunction()
    lumex_add_hazard_pointer_soak_test(hazard_pointer_soak soak 60 none
        ${CMAKE_CXX_COMPILER})
    # ThreadSanitizer: Clang if the build's compiler is one or one is on PATH.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(_tsan_compiler ${CMAKE_CXX_COMPILER})
        set(_tsan_prefix "")
    else()
        find_program(_tsan_clang NAMES clang++)
        if(_tsan_clang)
            set(_tsan_compiler ${_tsan_clang})
            set(_tsan_prefix "")
        else()
            set(_tsan_compiler ${CMAKE_CXX_COMPILER})
            set(_tsan_prefix setarch x86_64 -R)
        endif()
    endif()
    lumex_add_hazard_pointer_soak_test(hazard_pointer_tsan tsan 20 thread
        ${_tsan_compiler} ${_tsan_prefix})
endif()

# hazard_pointer compile-time checks: the uses [saferecl.hp] rejects (a type
# that is not hazard-protectable, a copy of a holder, a discarded result, the
# protected members of the base, a wrong deleter, a batch over the wrong span)
# must not compile with warnings as errors; valid use must.
lumex_test_name(_ctest_name hazard_pointer_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=hazard_pointer_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/hazard_pointer_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# Header hygiene at every standard (C++11 to C++20): a header does not depend
# on the Lumex header before it (LumexTime before LumexGenerators and back) and
# compiles without a warning under the strict flags of a consumer. Header-only
# translation units, built with try_compile while configuring.
lumex_test_name(_ctest_name hygiene_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=hygiene_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/hygiene_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)

# expected umbrella global-name checks: the umbrella has no global
# `unexpected` (the MinGW runtime declares a function of that name in <eh.h>)
# and no global `in_place`, so a program's own `unexpected` next to it compiles
# and the removed names do not; the names that stay global are the names of
# the module.
lumex_test_name(_ctest_name expected_compile_checks)
add_test(NAME ${_ctest_name}
    COMMAND ${CMAKE_COMMAND}
        -DCASE=expected_compile_checks
        -DLUMEX_SOURCE_DIR=${LumexLib_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/expected_compile_checks
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
        -DCLANG_STDLIB=${LUMEX_CLANG_STDLIB}
        -DBUILD_TYPE=Release
        -DCONFIGURE_ONLY=ON
        -P ${_LUMEX_CONSUMER_RUN}
)
set_tests_properties(${_ctest_name} PROPERTIES
    LABELS "cmake;consumer"
    TIMEOUT 900
    SKIP_REGULAR_EXPRESSION "LUMEX_CONSUMER_SKIP"
)
