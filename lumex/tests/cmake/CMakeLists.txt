# lumex/tests/cmake/CMakeLists.txt
#
# Tests the CMake option helpers and keeps subdirectory wiring from
# drifting off cmake/LumexOptions.cmake. These are cmake -P scripts,
# not GoogleTest binaries. They register as CTest names LumexCMake.*
# and run with the rest of the suite when LUMEX_BUILD_TESTS=ON.

set(_LUMEX_CMAKE_RUN "${CMAKE_CURRENT_SOURCE_DIR}/run_one.cmake")

function(lumex_add_cmake_test name)
    add_test(NAME LumexCMake.${name}
        COMMAND ${CMAKE_COMMAND}
            -DCASE=${name}
            -DLUMEX_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${_LUMEX_CMAKE_RUN}
    )
    set_tests_properties(LumexCMake.${name} PROPERTIES
        LABELS "cmake"
        TIMEOUT 30
    )
endfunction()

function(lumex_add_cmake_fail_test name pattern)
    add_test(NAME LumexCMake.${name}
        COMMAND ${CMAKE_COMMAND}
            -DCASE=${name}
            -DCASE_EXPECT_FAIL=1
            -DCASE_FAIL_PATTERN=${pattern}
            -DLUMEX_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${_LUMEX_CMAKE_RUN}
    )
    set_tests_properties(LumexCMake.${name} PROPERTIES
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
lumex_add_cmake_test(publish_distr_filters)
lumex_add_cmake_test(wiring_field_reflection)
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
lumex_add_cmake_test(wiring_build_timing)
lumex_add_cmake_test(wiring_generate_build_info)
lumex_add_cmake_test(wiring_warning_suppression)
lumex_add_cmake_test(wiring_recursive_source_collection)
lumex_add_cmake_test(wiring_compile_commands)
lumex_add_cmake_test(wiring_export_namespace)
lumex_add_cmake_test(asan_clang_cl_runtime_library)

lumex_add_cmake_fail_test(wiring_msvc_vcvars_after_project
    "must be called BEFORE project")

lumex_add_cmake_fail_test(require_fail_base64_without_utility
    "LUMEX_BUILD_BASE64 is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_crc_without_utility
    "LUMEX_BUILD_CRC is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_environment_without_utility
    "LUMEX_BUILD_ENVIRONMENT is enabled but requires LUMEX_BUILD_UTILITY")
lumex_add_cmake_fail_test(require_fail_filesystem_without_utility
    "LUMEX_BUILD_FILESYSTEM is enabled but requires LUMEX_BUILD_UTILITY")
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
lumex_add_cmake_fail_test(require_fail_resource_monitor_without_logging
    "LUMEX_BUILD_RESOURCE_MONITOR is enabled but requires LUMEX_BUILD_LOGGING")
lumex_add_cmake_fail_test(require_fail_settings_without_logging
    "LUMEX_BUILD_SETTINGS is enabled but requires LUMEX_BUILD_LOGGING")
lumex_add_cmake_fail_test(require_fail_logger_ini_without_settings
    "LUMEX_LOGGER_CONFIG_FORMAT=INI requires LUMEX_BUILD_SETTINGS")
lumex_add_cmake_fail_test(require_fail_logger_xml_without_xml
    "LUMEX_LOGGER_CONFIG_FORMAT=XML requires LUMEX_BUILD_XML")
lumex_add_cmake_fail_test(require_fail_logger_invalid_format
    "Invalid LUMEX_LOGGER_CONFIG_FORMAT='TOML'")
