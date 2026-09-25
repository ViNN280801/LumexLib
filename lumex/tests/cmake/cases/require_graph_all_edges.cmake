# Every PUBLIC/INTERFACE Lumex-module edge must stay in
# lumex_check_module_dependencies. A dropped line is a silent hole.

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake" _mod)

foreach(_edge
        "lumex_require_module(LUMEX_BUILD_UTILITY LUMEX_BUILD_MATH)"
        "lumex_require_module(LUMEX_BUILD_CIRCULAR_BUFFER LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_EXPECTED LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_BASE64 LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_CRC LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_ENVIRONMENT LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_TIME LUMEX_BUILD_ENVIRONMENT)"
        "lumex_require_module(LUMEX_BUILD_FILESYSTEM LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_STRING_VIEW LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_FMT LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_REFLECTION LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_SERIAL LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_XML LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_TEMPORARY LUMEX_BUILD_ENVIRONMENT)"
        "lumex_require_module(LUMEX_BUILD_TEMPORARY LUMEX_BUILD_FILESYSTEM)"
        "lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_ENVIRONMENT)"
        "lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_FILESYSTEM)"
        "lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_STRING)"
        "lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_TIME)"
        "lumex_require_module(LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_ENVIRONMENT)"
        "lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_FILESYSTEM)"
        "lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_STRING)"
        "lumex_require_module(LUMEX_BUILD_LOGGING LUMEX_BUILD_TIME)"
        "lumex_require_module(LUMEX_BUILD_HARDWARE LUMEX_BUILD_LOGGING)"
        "lumex_require_module(LUMEX_BUILD_HARDWARE LUMEX_BUILD_UTILITY)"
        "lumex_require_module(LUMEX_BUILD_RESOURCE_MONITOR LUMEX_BUILD_LOGGING)"
        "lumex_require_module(LUMEX_BUILD_RESOURCE_MONITOR LUMEX_BUILD_TIME)"
        "lumex_require_module(LUMEX_BUILD_SETTINGS LUMEX_BUILD_FILESYSTEM)"
        "lumex_require_module(LUMEX_BUILD_SETTINGS LUMEX_BUILD_LOGGING)"
        "lumex_require_module(LUMEX_BUILD_SETTINGS LUMEX_BUILD_TIME)"
        "lumex_require_module(LUMEX_BUILD_JSON LUMEX_BUILD_REFLECTION)"
        "lumex_require_module(LUMEX_BUILD_JSON LUMEX_BUILD_STRING_VIEW)"
        "lumex_require_module(LUMEX_BUILD_JSON LUMEX_BUILD_UTILITY)")
    string(FIND "${_mod}" "${_edge}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "missing dependency edge: ${_edge}")
    endif()
endforeach()
