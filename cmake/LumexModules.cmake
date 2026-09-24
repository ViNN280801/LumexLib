# cmake/LumexModules.cmake
#
# Helpers for LUMEX_BUILD_* group and module options. Include after
# cmake/LumexOptions.cmake. Call lumex_check_module_dependencies()
# before any add_subdirectory that creates a lumex:: alias.
#
# A module is enabled only when its group AND its per-module option
# are ON. Groups: LUMEX_BUILD_CORE, LUMEX_BUILD_APPLIED, LUMEX_BUILD_XML.

include_guard(GLOBAL)
# Script-mode cases (`cmake -P`) do not inherit the project's policy
# stack. IN_LIST needs CMP0057 NEW.
cmake_policy(SET CMP0057 NEW)

set(LUMEX_CORE_MODULE_OPTIONS
    LUMEX_BUILD_BASE64
    LUMEX_BUILD_CIRCULAR_BUFFER
    LUMEX_BUILD_CRC
    LUMEX_BUILD_ENVIRONMENT
    LUMEX_BUILD_EXCEPTIONS
    LUMEX_BUILD_EXPECTED
    LUMEX_BUILD_FILESYSTEM
    LUMEX_BUILD_GENERATORS
    LUMEX_BUILD_MATH
    LUMEX_BUILD_OPTIONAL
    LUMEX_BUILD_REFLECTION
    LUMEX_BUILD_STRING
    LUMEX_BUILD_STRING_VIEW
    LUMEX_BUILD_TEMPORARY
    LUMEX_BUILD_TIME
    LUMEX_BUILD_UTILITY
)

set(LUMEX_APPLIED_MODULE_OPTIONS
    LUMEX_BUILD_HARDWARE
    LUMEX_BUILD_JSON
    LUMEX_BUILD_LOGGER
    LUMEX_BUILD_LOGGING
    LUMEX_BUILD_RESOURCE_MONITOR
    LUMEX_BUILD_SERIAL
    LUMEX_BUILD_SETTINGS
)

function(lumex_module_group out_var opt_var)
    if("${opt_var}" IN_LIST LUMEX_CORE_MODULE_OPTIONS)
        set(${out_var} LUMEX_BUILD_CORE PARENT_SCOPE)
    elseif("${opt_var}" IN_LIST LUMEX_APPLIED_MODULE_OPTIONS)
        set(${out_var} LUMEX_BUILD_APPLIED PARENT_SCOPE)
    elseif(opt_var STREQUAL "LUMEX_BUILD_XML")
        set(${out_var} LUMEX_BUILD_XML PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

function(lumex_effective_on out_var opt_var)
    if(NOT ${opt_var})
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()
    lumex_module_group(_group "${opt_var}")
    if(_group AND NOT ${_group})
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()
    set(${out_var} TRUE PARENT_SCOPE)
endfunction()

function(lumex_require_module consumer_opt dep_opt)
    lumex_effective_on(_consumer_on "${consumer_opt}")
    lumex_effective_on(_dep_on "${dep_opt}")
    if(_consumer_on AND NOT _dep_on)
        lumex_module_group(_consumer_group "${consumer_opt}")
        lumex_module_group(_dep_group "${dep_opt}")
        message(FATAL_ERROR
            "${consumer_opt} is enabled but requires ${dep_opt} to be enabled "
            "(group flags: ${_consumer_group} / ${_dep_group})")
    endif()
endfunction()

function(lumex_add_subdirectory_if opt_var source_dir)
    lumex_effective_on(_on "${opt_var}")
    if(_on)
        add_subdirectory(${source_dir})
    else()
        lumex_module_group(_group "${opt_var}")
        if(_group AND NOT ${_group})
            message(STATUS "Skipping ${source_dir} (${_group}=OFF)")
        else()
            message(STATUS "Skipping ${source_dir} (${opt_var}=OFF)")
        endif()
    endif()
endfunction()

function(lumex_check_module_dependencies)
    # Edges match PUBLIC/INTERFACE target_link_libraries between Lumex modules.
    lumex_require_module(LUMEX_BUILD_UTILITY LUMEX_BUILD_MATH)
    lumex_require_module(LUMEX_BUILD_CIRCULAR_BUFFER LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_EXPECTED LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_BASE64 LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_CRC LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_ENVIRONMENT LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_TIME LUMEX_BUILD_ENVIRONMENT)
    lumex_require_module(LUMEX_BUILD_FILESYSTEM LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_STRING_VIEW LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_REFLECTION LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_SERIAL LUMEX_BUILD_UTILITY)
    lumex_require_module(LUMEX_BUILD_XML LUMEX_BUILD_UTILITY)

    lumex_require_module(LUMEX_BUILD_TEMPORARY LUMEX_BUILD_ENVIRONMENT)
    lumex_require_module(LUMEX_BUILD_TEMPORARY LUMEX_BUILD_FILESYSTEM)

    lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_ENVIRONMENT)
    lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_FILESYSTEM)
    lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_STRING)
    lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_TIME)
    lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_UTILITY)

    lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_ENVIRONMENT)
    lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_FILESYSTEM)
    lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_STRING)
    lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_TIME)

    lumex_require_module(LUMEX_BUILD_HARDWARE LUMEX_BUILD_LOGGING)
    lumex_require_module(LUMEX_BUILD_HARDWARE LUMEX_BUILD_UTILITY)

    lumex_require_module(LUMEX_BUILD_RESOURCE_MONITOR LUMEX_BUILD_LOGGING)
    lumex_require_module(LUMEX_BUILD_RESOURCE_MONITOR LUMEX_BUILD_TIME)

    lumex_require_module(LUMEX_BUILD_SETTINGS LUMEX_BUILD_FILESYSTEM)
    lumex_require_module(LUMEX_BUILD_SETTINGS LUMEX_BUILD_LOGGING)
    lumex_require_module(LUMEX_BUILD_SETTINGS LUMEX_BUILD_TIME)

    lumex_require_module(LUMEX_BUILD_JSON LUMEX_BUILD_REFLECTION)
    lumex_require_module(LUMEX_BUILD_JSON LUMEX_BUILD_STRING_VIEW)
    lumex_require_module(LUMEX_BUILD_JSON LUMEX_BUILD_UTILITY)

    string(TOUPPER "${LUMEX_LOGGER_CONFIG_FORMAT}" _logger_config_format)
    set(_logger_config_formats PLAIN_TEXT INI JSON YAML XML)
    if(NOT _logger_config_format IN_LIST _logger_config_formats)
        message(FATAL_ERROR
            "Invalid LUMEX_LOGGER_CONFIG_FORMAT='${LUMEX_LOGGER_CONFIG_FORMAT}'. "
            "Expected one of: PLAIN_TEXT, INI, JSON, YAML, XML")
    endif()
    set(LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED
        "${_logger_config_format}" PARENT_SCOPE)

    lumex_effective_on(_logger_on LUMEX_BUILD_LOGGER)
    if(_logger_on AND _logger_config_format STREQUAL "INI")
        lumex_effective_on(_settings_on LUMEX_BUILD_SETTINGS)
        if(NOT _settings_on)
            message(FATAL_ERROR
                "LUMEX_LOGGER_CONFIG_FORMAT=INI requires LUMEX_BUILD_SETTINGS "
                "to be effective (group LUMEX_BUILD_APPLIED and the module "
                "option both ON)")
        endif()
    endif()
    if(_logger_on AND _logger_config_format STREQUAL "XML")
        lumex_effective_on(_xml_on LUMEX_BUILD_XML)
        if(NOT _xml_on)
            message(FATAL_ERROR
                "LUMEX_LOGGER_CONFIG_FORMAT=XML requires LUMEX_BUILD_XML "
                "to be effective")
        endif()
    endif()
endfunction()

function(lumex_install_export_if_any export_name dest_file)
    set(_has FALSE)
    foreach(_t ${ARGN})
        if(TARGET ${_t})
            set(_has TRUE)
            break()
        endif()
    endforeach()
    if(_has)
        install(EXPORT ${export_name}
            FILE ${dest_file}
            NAMESPACE lumex::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
        )
    endif()
endfunction()

# Shared-library names that can appear as real SHARED targets. INTERFACE
# modules are skipped at collect time. Keep in lockstep with the
# install(EXPORT) candidate lists in the root CMakeLists.txt.
set(LUMEX_SHARED_LIBRARY_CANDIDATES
    LumexCore_base64
    LumexCore_circular_buffer
    LumexCore_crc
    LumexCore_environment
    LumexCore_exceptions
    LumexCore_expected
    LumexCore_filesystem
    LumexCore_number_generator
    LumexCore_math
    LumexCore_optional
    LumexCore_reflection
    LumexCore_string
    LumexCore_string_view
    LumexCore_temporary
    LumexCore_time
    LumexCore_utility
    LumexApplied_hardware
    LumexApplied_json
    LumexApplied_logger
    LumexApplied_logging
    LumexApplied_resource_monitor
    LumexApplied_serial
    LumexApplied_settings
    LumexXml
)

# POST_BUILD-style Distr folder, same shape as PeakExpertCE:
#   <LumexLib>/<platform>/Distr<Config>
# Copies only Lumex shared libraries (and CRT next to them). Test and
# example binaries stay in the build tree. Programmer runs ctest separately.
function(lumex_add_publish_distr_target)
    set(_deps)
    foreach(_t ${LUMEX_SHARED_LIBRARY_CANDIDATES})
        if(TARGET ${_t})
            get_target_property(_type ${_t} TYPE)
            if(_type STREQUAL "SHARED_LIBRARY")
                list(APPEND _deps ${_t})
            endif()
        endif()
    endforeach()

    if(CMAKE_VS_PLATFORM_NAME)
        set(_plat "${CMAKE_VS_PLATFORM_NAME}")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_plat "x64")
    elseif(WIN32)
        set(_plat "Win32")
    else()
        set(_plat "${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    if(CMAKE_CONFIGURATION_TYPES)
        set(_distr_dir "${PROJECT_SOURCE_DIR}/${_plat}/Distr$<CONFIG>")
        set(_bin_dir "${LUMEX_BIN_DIR}/$<CONFIG>")
    else()
        if(CMAKE_BUILD_TYPE)
            set(_cfg "${CMAKE_BUILD_TYPE}")
        else()
            set(_cfg "Release")
        endif()
        set(_distr_dir "${PROJECT_SOURCE_DIR}/${_plat}/Distr${_cfg}")
        set(_bin_dir "${LUMEX_BIN_DIR}")
    endif()

    add_custom_target(publish_distr ALL
        COMMAND ${CMAKE_COMMAND}
            -Dbin_dir=${_bin_dir}
            -Ddistr_dir=${_distr_dir}
            -Dcopy_runtime_script=${PROJECT_SOURCE_DIR}/CMakeRoutines/deployment/CopyRuntimeDependencies.cmake
            -P ${PROJECT_SOURCE_DIR}/cmake/PublishDistr.cmake
        DEPENDS ${_deps}
        COMMENT "Publishing Lumex shared libraries to Distr"
        VERBATIM
    )
endfunction()
