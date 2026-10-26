# cmake/LumexThirdParty.cmake
#
# Common steps of the vendored third-party code of the transport layer
# (cmake/LumexAsio.cmake, cmake/LumexBoostContext.cmake,
# cmake/LumexHidapi.cmake). Their targets are named lumex_3rdparty_*;
# lumex_configure_target skips that prefix, so they never get WARNINGS HIGH.

# Reads `#define <macro> <number>` from <header> into <out_var>.
function(lumex_third_party_read_define out_var header macro)
    if(NOT EXISTS "${header}")
        message(FATAL_ERROR "LumexLib: ${header} does not exist")
    endif()
    # The first matching line; a trailing comment after the number is allowed
    # (asio/version.hpp: "#define ASIO_VERSION 103600 // 1.36.0").
    file(STRINGS "${header}" _lines REGEX "^[ \t]*#[ \t]*define[ \t]+${macro}[ \t]+[0-9]+")
    if(NOT _lines)
        message(FATAL_ERROR "LumexLib: ${header} has no #define ${macro} <number>")
    endif()
    list(GET _lines 0 _line)
    string(REGEX REPLACE "^[ \t]*#[ \t]*define[ \t]+${macro}[ \t]+([0-9]+).*$" "\\1" _value "${_line}")
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

# Third-party headers stay quiet in the consumer's translation units too. The
# include directories are SYSTEM (-isystem for GCC and Clang, -imsvc for
# clang-cl, /external:I for cl.exe from CMake 3.22); cl.exe additionally needs
# /external:W0 to drop the warnings of external headers, which this passes on
# to every consumer of <target> (the library's own cl.exe targets use the same
# switch, cmake/LumexBuild.cmake).
function(lumex_third_party_quiet_includes target)
    target_compile_options(${target} INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/external:W0>")
endfunction()

# Every warning off, position-independent code, the tree's sanitizer flags and,
# for C++ targets, the C++ standard library the tree chose (-stdlib=libc++ on
# Clang), so instrumented or libc++ objects never mix with plain ones.
function(lumex_third_party_finish target)
    if(MSVC AND CMAKE_GENERATOR MATCHES "^Visual Studio")
        # The ClCompile items of a Visual Studio project default to /W1, and
        # suppress_warnings adds /w next to it: cl.exe warns D9025. /W0 is
        # the same switch to cl.exe and becomes the project's WarningLevel.
        target_compile_options(${target} PRIVATE /W0)
    elseif(COMMAND suppress_warnings)
        suppress_warnings(${target})
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W0)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION OFF)
    if(COMMAND lumex_apply_sanitizers)
        lumex_apply_sanitizers(${target})
    endif()
    lumex_third_party_apply_stdlib(${target})
endfunction()

# Adds the recorded -stdlib= option to the C++ sources of <target> once. In the
# library's own tree the option is recorded only by
# lumex_configure_all_compiled_targets, which calls this again for every
# lumex_3rdparty_* target it finds.
function(lumex_third_party_apply_stdlib target)
    get_target_property(_done ${target} LUMEX_3RDPARTY_STDLIB_DONE)
    if(_done)
        return()
    endif()
    get_property(_stdlib GLOBAL PROPERTY LUMEX_CXX_STDLIB_OPTION)
    if(_stdlib)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${_stdlib}>")
        set_property(TARGET ${target} PROPERTY LUMEX_3RDPARTY_STDLIB_DONE TRUE)
    endif()
endfunction()

# Installs a third-party target that LumexLib created (not one the consumer
# defined first) with the export set of the module that needs it, so that an
# installed package that exports that module also provides the target:
# lumex_third_party_install(<target> <export set>). The headers go under
# include/lumex/3rdparty/<name> (the INSTALL_INTERFACE include directory the
# setup function gave the target), the license file next to them, a static
# library to the library directory. The setup functions mark their targets
# with INTERFACE_LUMEX_3RDPARTY_INSTALL_DIRS (pairs of source directory and
# destination) and INTERFACE_LUMEX_3RDPARTY_LICENSE (the license file and its
# destination); the INTERFACE_ prefix because CMake before 3.19 allows no
# other custom property on an INTERFACE library.
function(lumex_third_party_install target export_set)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(_dirs ${target} INTERFACE_LUMEX_3RDPARTY_INSTALL_DIRS)
    if(NOT _dirs)
        # Defined by the consumer: it installs its own copy.
        return()
    endif()
    get_property(_installed GLOBAL PROPERTY LUMEX_3RDPARTY_INSTALLED_${target})
    if(_installed)
        return()
    endif()
    set_property(GLOBAL PROPERTY LUMEX_3RDPARTY_INSTALLED_${target} TRUE)
    list(LENGTH _dirs _count)
    math(EXPR _last "${_count} - 1")
    foreach(_i RANGE 0 ${_last} 2)
        math(EXPR _j "${_i} + 1")
        list(GET _dirs ${_i} _from)
        list(GET _dirs ${_j} _to)
        install(DIRECTORY "${_from}/" DESTINATION "${_to}")
    endforeach()
    get_target_property(_license ${target} INTERFACE_LUMEX_3RDPARTY_LICENSE)
    if(_license)
        list(GET _license 0 _license_file)
        list(GET _license 1 _license_to)
        install(FILES "${_license_file}" DESTINATION "${_license_to}")
    endif()
    install(TARGETS ${target} EXPORT ${export_set}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()