# cmake/LumexBoostContext.cmake
#
# Boost.Context 1.88.0 for asio::spawn, vendored under 3rdparty/boost_context
# (the include closure of the fiber headers, provenance:
# 3rdparty/boost_context/VENDORED.md). lumex_setup_boost_context() creates the
# in-tree STATIC target lumex_3rdparty_boost_context (alias
# lumex::3rdparty_boost_context, not exported) with the backends that need no
# assembly: Windows fibers (BOOST_USE_WINFIB) on Windows, ucontext
# (BOOST_USE_UCONTEXT) elsewhere. LUMEX_BOOST_CONTEXT_DIR replaces the vendored
# tree with another Boost 1.88.0 source tree; a prebuilt Boost::context is not
# reused, because it may have been built with the fcontext backend and mixing
# backends is an ODR violation.

get_filename_component(_LUMEX_BOOST_CONTEXT_DEFAULT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/boost_context" ABSOLUTE)
# A global property, not a variable: the setup function may be called from a
# consumer's directory, which does not see this file's directory scope.
set_property(GLOBAL PROPERTY LUMEX_BOOST_CONTEXT_DEFAULT_DIR "${_LUMEX_BOOST_CONTEXT_DEFAULT_DIR}")

function(lumex_setup_boost_context)
    set(LUMEX_BOOST_REQUIRED_VERSION 108800)
    if(TARGET lumex_3rdparty_boost_context)
        return()
    endif()

    if(LUMEX_BOOST_CONTEXT_DIR)
        get_filename_component(_dir "${LUMEX_BOOST_CONTEXT_DIR}" ABSOLUTE)
    else()
        get_property(_dir GLOBAL PROPERTY LUMEX_BOOST_CONTEXT_DEFAULT_DIR)
    endif()

    lumex_third_party_read_define(_version "${_dir}/boost/version.hpp" BOOST_VERSION)
    if(NOT _version STREQUAL "${LUMEX_BOOST_REQUIRED_VERSION}")
        message(FATAL_ERROR
            "LumexLib needs Boost ${LUMEX_BOOST_REQUIRED_VERSION} (1.88.0) for "
            "Boost.Context, ${_dir}/boost/version.hpp has ${_version}.")
    endif()

    set(_src "${_dir}/libs/context/src")
    if(WIN32)
        set(_backend BOOST_USE_WINFIB)
        set(_traits "${_src}/windows/stack_traits.cpp")
    else()
        set(_backend BOOST_USE_UCONTEXT)
        set(_traits "${_src}/posix/stack_traits.cpp")
    endif()
    foreach(_file IN ITEMS "${_src}/fiber.cpp" "${_traits}")
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR "LumexLib: Boost.Context source ${_file} is missing")
        endif()
    endforeach()

    add_library(lumex_3rdparty_boost_context STATIC "${_src}/fiber.cpp" "${_traits}")
    add_library(lumex::3rdparty_boost_context ALIAS lumex_3rdparty_boost_context)
    set_target_properties(lumex_3rdparty_boost_context PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    target_include_directories(lumex_3rdparty_boost_context SYSTEM PUBLIC
        "$<BUILD_INTERFACE:${_dir}>"
        "$<INSTALL_INTERFACE:include/lumex/3rdparty/boost_context>")
    # What lumex_third_party_install copies: the headers of the closure.
    set_target_properties(lumex_3rdparty_boost_context PROPERTIES
        INTERFACE_LUMEX_3RDPARTY_INSTALL_DIRS
            "${_dir}/boost;include/lumex/3rdparty/boost_context/boost")
    if(EXISTS "${_dir}/LICENSE_1_0.txt")
        set_target_properties(lumex_3rdparty_boost_context PROPERTIES
            INTERFACE_LUMEX_3RDPARTY_LICENSE "${_dir}/LICENSE_1_0.txt;include/lumex/3rdparty/boost_context")
    endif()
    # BOOST_ALL_NO_LIB: no MSVC auto-link to a prebuilt boost_context library.
    target_compile_definitions(lumex_3rdparty_boost_context PUBLIC
        BOOST_ALL_NO_LIB
        ${_backend})
    if(LUMEX_USE_ASAN AND _backend STREQUAL "BOOST_USE_UCONTEXT")
        target_compile_definitions(lumex_3rdparty_boost_context PUBLIC BOOST_USE_ASAN)
    endif()
    lumex_third_party_quiet_includes(lumex_3rdparty_boost_context)
    lumex_third_party_finish(lumex_3rdparty_boost_context)
endfunction()
