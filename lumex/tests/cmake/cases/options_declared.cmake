# Every group and module option() must exist in LumexOptions.cmake.

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)

foreach(_name
        LUMEX_BUILD_CORE
        LUMEX_BUILD_APPLIED
        LUMEX_BUILD_XML
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
        LUMEX_BUILD_HARDWARE
        LUMEX_BUILD_JSON
        LUMEX_BUILD_LOGGER
        LUMEX_BUILD_LOGGING
        LUMEX_BUILD_RESOURCE_MONITOR
        LUMEX_BUILD_SERIAL
        LUMEX_BUILD_SETTINGS
        LUMEX_WITH_FIELD_REFLECTION
        LUMEX_USE_ASAN
        LUMEX_USE_UBSAN
        LUMEX_USE_TSAN
        LUMEX_USE_CLANG_TIDY
        LUMEX_USE_CPPCHECK
        LUMEX_MAXIMUM_STANDARD_COMPLIANCE
        LUMEX_BUILD_TIMING
        LUMEX_GENERATE_BUILD_INFO)
    string(FIND "${_opts}" "option(${_name} " _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "cmake/LumexOptions.cmake has no option(${_name} ...)")
    endif()
endforeach()

string(FIND "${_opts}" "set(LUMEX_LOGGER_CONFIG_FORMAT " _logger_fmt_pos)
if(_logger_fmt_pos EQUAL -1)
    message(FATAL_ERROR
        "cmake/LumexOptions.cmake has no set(LUMEX_LOGGER_CONFIG_FORMAT ...)")
endif()
