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
