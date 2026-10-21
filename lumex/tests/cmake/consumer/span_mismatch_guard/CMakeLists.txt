# Fixture for cmake.consumer_span_mismatch_guard.
#
# LumexSpanChecks.hpp records LUMEX_SPAN_CHECKS for the MSVC linker
# (detect_mismatch). Through LumexUtility every compiled module of the library
# includes it, so the build of the library defines
# LUMEX_SPAN_NO_MISMATCH_GUARD on its own targets (cmake/LumexBuild.cmake) and
# only the translation units of a consumer record the value. Checked while
# configuring, on every compiler (the macro LUMEX_SPAN_MISMATCH_GUARD is
# defined everywhere; only MSVC turns it into the pragma):
#   1. every compiled target of the embedded library has the definition, and
#      none of them hands it on to its consumers (it is PRIVATE);
#   2. a translation unit compiled with the definitions of a library target
#      has no guard, one compiled like a consumer's has it.

cmake_minimum_required(VERSION 3.16)
project(LumexSpanMismatchGuard LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

function(_collect_targets dir out_var)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_s IN LISTS _subs)
        _collect_targets("${_s}" _more)
        list(APPEND _targets ${_more})
    endforeach()
    set(${out_var} ${_targets} PARENT_SCOPE)
endfunction()

_collect_targets("${LUMEX_SOURCE_DIR}" _all_targets)

set(_compiled "")
foreach(_t IN LISTS _all_targets)
    get_target_property(_type "${_t}" TYPE)
    if(_type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "STATIC_LIBRARY"
       OR _type STREQUAL "MODULE_LIBRARY" OR _type STREQUAL "OBJECT_LIBRARY"
       OR _type STREQUAL "EXECUTABLE")
        list(APPEND _compiled "${_t}")
    endif()
endforeach()
list(LENGTH _compiled _count)
if(_count LESS 10)
    message(FATAL_ERROR "only ${_count} compiled targets found: ${_compiled}")
endif()

set(_library_definitions "")
foreach(_t IN LISTS _compiled)
    # GoogleTest and the vendored third-party code (Boost.Context, hidapi)
    # never include the span headers and get no Lumex compile settings.
    if(_t MATCHES "^lumex_gtest" OR _t MATCHES "^lumex_3rdparty_")
        continue()
    endif()
    get_target_property(_defs "${_t}" COMPILE_DEFINITIONS)
    if(NOT _defs OR NOT "LUMEX_SPAN_NO_MISMATCH_GUARD" IN_LIST _defs)
        message(FATAL_ERROR
            "${_t} is a compiled target of the library and lacks "
            "LUMEX_SPAN_NO_MISMATCH_GUARD (COMPILE_DEFINITIONS: ${_defs})")
    endif()
    get_target_property(_iface "${_t}" INTERFACE_COMPILE_DEFINITIONS)
    if(_iface AND "LUMEX_SPAN_NO_MISMATCH_GUARD" IN_LIST _iface)
        message(FATAL_ERROR
            "${_t} hands LUMEX_SPAN_NO_MISMATCH_GUARD on to its consumers")
    endif()
    if(NOT _library_definitions)
        set(_library_definitions ${_defs})
    endif()
endforeach()
message(STATUS "LUMEX_SPAN_NO_MISMATCH_GUARD on ${_count} compiled targets")

# A header-only module does not carry it either (a consumer's includes only).
get_target_property(_span_iface LumexCore_span INTERFACE_COMPILE_DEFINITIONS)
if(_span_iface AND "LUMEX_SPAN_NO_MISMATCH_GUARD" IN_LIST _span_iface)
    message(FATAL_ERROR "lumex::span hands the definition on to its consumers")
endif()

# Translation units: the definitions of a library target against a consumer's.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(_library_flags "")
foreach(_d IN LISTS _library_definitions)
    list(APPEND _library_flags "-D${_d}")
endforeach()
if(MSVC)
    set(_msvc_flags /Zc:__cplusplus)
else()
    set(_msvc_flags "")
endif()

foreach(_case IN ITEMS library consumer)
    if(_case STREQUAL "library")
        set(_expect 0)
        set(_flags ${_library_flags})
    else()
        set(_expect 1)
        set(_flags "")
    endif()
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${_case}"
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/probe.cpp"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
        COMPILE_DEFINITIONS ${_flags} ${_msvc_flags} -DEXPECT_GUARD=${_expect}
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE _output)
    if(NOT _ok)
        message(FATAL_ERROR "the ${_case} translation unit has the wrong "
                            "mismatch guard:\n${_output}")
    endif()
endforeach()
message(STATUS "library units have no mismatch guard, consumer units have it")
