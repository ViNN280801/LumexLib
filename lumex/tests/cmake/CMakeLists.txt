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
lumex_add_cmake_test(wiring_gtest_cxx_stdlib)
lumex_add_cmake_test(wiring_embedding)
lumex_add_cmake_test(wiring_include_order_ctest)
lumex_add_cmake_test(wiring_clang_stdlib)
lumex_add_cmake_test(asan_clang_cl_runtime_library)
lumex_add_cmake_test(gtest_discovery_does_not_lock_exe)
lumex_add_cmake_test(wiring_atomic)
lumex_add_cmake_test(wiring_test_names)
lumex_add_cmake_test(wiring_standard_suites)
lumex_add_cmake_test(wiring_exception_crash_lock)

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
lumex_add_cmake_fail_test(require_fail_serial_without_core
    "LUMEX_BUILD_SERIAL is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_xml_without_core
    "LUMEX_BUILD_XML is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_xml_without_utility
    "LUMEX_BUILD_XML is enabled but requires LUMEX_BUILD_UTILITY")
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

function(lumex_add_consumer_test name fixture)
    # The nested build uses this tree's C++ flags (a --gcc-toolchain, for
    # example) and its Clang standard library choice, so it builds like the
    # tree that runs it.
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
# The header-only modules (string, math, generators, optional, atomic) compile
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
