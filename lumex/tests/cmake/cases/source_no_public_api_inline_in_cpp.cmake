# A function defined in a .cpp with LUMEX_PUBLIC_API is part of the exported
# API and is called from other translation units, so its definition must not
# be `inline`: an inline function is emitted only where it is odr-used, and
# a caller in another translation unit then finds no symbol. GCC hid this
# behind -fkeep-inline-functions and MSVC behind dllexport, while Clang on
# Linux failed to link LumexXml itself. This case keeps the pair out of
# every production .cpp file.

file(GLOB_RECURSE _sources RELATIVE "${LUMEX_SOURCE_DIR}"
    "${LUMEX_SOURCE_DIR}/lumex/*.cpp")
list(FILTER _sources EXCLUDE REGEX "^lumex/(tests|examples)/")

if(NOT _sources)
    message(FATAL_ERROR "no production .cpp files found under lumex/")
endif()

# The whole file is matched at once: source lines contain `;` and brackets,
# which a per-line CMake list would split. "\n" below is a real newline.
set(_pattern "LUMEX_PUBLIC_API[ \t\r]*(\n[ \t\r]*)?inline[ \t\r\n]")

set(_offenders "")
foreach(_source IN LISTS _sources)
    file(READ "${LUMEX_SOURCE_DIR}/${_source}" _text)
    string(REGEX MATCH "${_pattern}" _hit "${_text}")
    if(NOT _hit STREQUAL "")
        list(APPEND _offenders "${_source}")
    endif()
endforeach()

if(_offenders)
    string(REPLACE ";" "\n  " _list "${_offenders}")
    message(FATAL_ERROR
        "inline definition of a LUMEX_PUBLIC_API function in (drop `inline`):"
        "\n  ${_list}")
endif()
