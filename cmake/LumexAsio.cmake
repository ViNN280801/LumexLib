# cmake/LumexAsio.cmake
#
# Standalone Asio 1.36.0, vendored header-only under 3rdparty/asio/include
# (provenance: 3rdparty/asio/VENDORED.md). lumex_setup_asio() creates the
# in-tree INTERFACE target lumex_3rdparty_asio (alias lumex::3rdparty_asio,
# not exported). LUMEX_ASIO_DIR replaces the vendored include directory with
# the consumer's own; the version must still be 1.36.0 and the configuration
# macros are fixed (ASIO_STANDALONE, ASIO_NO_DEPRECATED): a process that
# also loads another library holds a second Asio copy, and both copies must
# be identical. ASIO_DISABLE_VISIBILITY is NOT defined, on purpose: the
# Asio headers push default visibility around themselves, so on ELF the
# statics of Asio (the per-thread call stacks above all) are one per process
# whatever -fvisibility says, and an io_context shared by an executable and a
# shared library works. With a copy of those per binary, the epoll reactor of
# one binary runs on the scheduler thread of the other and dereferences a
# null thread record (a segmentation fault in 3 of 4 runs of the
# transport_two_modules fixture). The transport's own symbols and hidapi stay
# hidden (see lumex/applied/transport/CMakeLists.txt).

# Resolved at include time: CMAKE_CURRENT_LIST_DIR inside a function is the
# caller's directory.
get_filename_component(_LUMEX_ASIO_DEFAULT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/asio/include" ABSOLUTE)
# A global property, not a variable: the setup function may be called from a
# consumer's directory, which does not see this file's directory scope.
set_property(GLOBAL PROPERTY LUMEX_ASIO_DEFAULT_DIR "${_LUMEX_ASIO_DEFAULT_DIR}")

function(lumex_setup_asio)
    set(LUMEX_ASIO_REQUIRED_VERSION 103600)
    if(TARGET lumex_3rdparty_asio)
        return()
    endif()

    if(LUMEX_ASIO_DIR)
        get_filename_component(_dir "${LUMEX_ASIO_DIR}" ABSOLUTE)
    else()
        get_property(_dir GLOBAL PROPERTY LUMEX_ASIO_DEFAULT_DIR)
    endif()

    lumex_third_party_read_define(_version "${_dir}/asio/version.hpp" ASIO_VERSION)
    if(NOT _version STREQUAL "${LUMEX_ASIO_REQUIRED_VERSION}")
        message(FATAL_ERROR
            "LumexLib needs Asio ${LUMEX_ASIO_REQUIRED_VERSION} (1.36.0), "
            "${_dir}/asio/version.hpp has ${_version}. The version is pinned "
            "because one process can hold a second copy of Asio.")
    endif()

    find_package(Threads REQUIRED)
    add_library(lumex_3rdparty_asio INTERFACE)
    add_library(lumex::3rdparty_asio ALIAS lumex_3rdparty_asio)
    target_include_directories(lumex_3rdparty_asio SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${_dir}>"
        "$<INSTALL_INTERFACE:include/lumex/3rdparty/asio>")
    # What lumex_third_party_install copies when a module that needs Asio is
    # installed (cmake/LumexThirdParty.cmake).
    get_filename_component(_root "${_dir}/.." ABSOLUTE)
    set_target_properties(lumex_3rdparty_asio PROPERTIES
        INTERFACE_LUMEX_3RDPARTY_INSTALL_DIRS "${_dir};include/lumex/3rdparty/asio")
    if(EXISTS "${_root}/LICENSE_1_0.txt")
        set_target_properties(lumex_3rdparty_asio PROPERTIES
            INTERFACE_LUMEX_3RDPARTY_LICENSE "${_root}/LICENSE_1_0.txt;include/lumex/3rdparty/asio")
    endif()
    target_compile_definitions(lumex_3rdparty_asio INTERFACE
        ASIO_STANDALONE
        ASIO_NO_DEPRECATED)
    target_link_libraries(lumex_3rdparty_asio INTERFACE Threads::Threads)
    lumex_third_party_quiet_includes(lumex_3rdparty_asio)
    if(WIN32)
        # cl.exe gets these through #pragma comment(lib); MinGW and clang
        # with the GNU driver do not.
        target_link_libraries(lumex_3rdparty_asio INTERFACE ws2_32 mswsock)
    endif()
endfunction()
