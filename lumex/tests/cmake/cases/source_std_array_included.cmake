# A file that uses std::array includes <array> itself. The library used to get
# it through lumex/core/utility/traits/LumexTypeTraits.hpp, which then dropped
# the include and broke every header that had relied on it. Comments are not
# code: lines that start with `*`, `/*`, `//` or `///` are skipped.

file(GLOB_RECURSE _files
    "${LUMEX_SOURCE_DIR}/lumex/*.hpp"
    "${LUMEX_SOURCE_DIR}/lumex/*.cpp")

set(_missing "")
foreach(_file IN LISTS _files)
    # Tests and examples are compiled one translation unit at a time and may
    # keep their own policy; the library headers are what consumers include.
    if(_file MATCHES "/lumex/(tests|examples)/")
        continue()
    endif()
    file(STRINGS "${_file}" _lines)
    set(_uses FALSE)
    set(_includes FALSE)
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^[ \t]*#[ \t]*include[ \t]*<array>")
            set(_includes TRUE)
        elseif(NOT _line MATCHES "^[ \t]*(\\*|/\\*|//)" AND _line MATCHES "std::array[ \t]*<")
            set(_uses TRUE)
        endif()
    endforeach()
    if(_uses AND NOT _includes)
        list(APPEND _missing "${_file}")
    endif()
endforeach()

if(_missing)
    string(REPLACE ";" "\n  " _list "${_missing}")
    message(FATAL_ERROR
        "These files use std::array without #include <array>:\n  ${_list}")
endif()
