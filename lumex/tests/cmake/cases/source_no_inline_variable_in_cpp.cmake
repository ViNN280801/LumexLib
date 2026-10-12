# Static data members declared in a header without `inline` are defined once,
# in a .cpp, as ordinary definitions. LUMEX_INLINE_VARIABLE expands to
# `inline` from C++17, and an inline variable defined in that one translation
# unit only is emitted there only when the unit odr-uses it: ELF shared
# libraries then export no symbol for the constant-initialized members, and
# LumexXml (linked with -Wl,--no-undefined) fails with undefined references.
# MSVC hides the problem because dllexport forces the definitions out, so
# only a Linux build would notice; this case catches it on every platform.

file(GLOB_RECURSE _sources RELATIVE "${LUMEX_SOURCE_DIR}"
    "${LUMEX_SOURCE_DIR}/lumex/*.cpp")
list(FILTER _sources EXCLUDE REGEX "^lumex/(tests|examples)/")

if(NOT _sources)
    message(FATAL_ERROR "no production .cpp files found under lumex/")
endif()

set(_offenders "")
foreach(_source IN LISTS _sources)
    # Code lines only: a comment line (`//`, ` * `) may name the macro.
    file(STRINGS "${LUMEX_SOURCE_DIR}/${_source}" _hits
        REGEX "^[^/*]*LUMEX_INLINE_VARIABLE")
    if(_hits)
        list(APPEND _offenders "${_source}")
    endif()
endforeach()

if(_offenders)
    string(REPLACE ";" "\n  " _list "${_offenders}")
    message(FATAL_ERROR
        "LUMEX_INLINE_VARIABLE in production .cpp files (use an ordinary "
        "definition):\n  ${_list}")
endif()
