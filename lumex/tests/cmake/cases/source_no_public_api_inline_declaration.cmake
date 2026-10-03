# A header that declares an exported function `inline` without defining it
# promises a definition in every translation unit that uses the function, and
# the only definition then sits in one .cpp. A caller in another translation
# unit (a consumer taking the address of seh_translator through
# SET_SEH_TRANSLATOR) finds no symbol. source_no_public_api_inline_in_cpp
# catches the pair in a .cpp; this case catches the declaration in a header,
# which the .cpp check misses when the definition carries no export macro.

file(GLOB_RECURSE _headers RELATIVE "${LUMEX_SOURCE_DIR}"
    "${LUMEX_SOURCE_DIR}/lumex/*.hpp")
list(FILTER _headers EXCLUDE REGEX "^lumex/(tests|examples)/")

if(NOT _headers)
    message(FATAL_ERROR "no production headers found under lumex/")
endif()

# An export macro, then `inline`, then a declaration that ends with `;` before
# any `{`: a definition in the header has a body and stays allowed. The whole
# file is matched at once because source lines contain `;`.
set(_pattern
    "(LUMEX_PUBLIC_API|LUMEX_API|LUMEX_UTILITY_API)[ \t\r\n]+inline[^;{]*;")

set(_offenders "")
foreach(_header IN LISTS _headers)
    file(READ "${LUMEX_SOURCE_DIR}/${_header}" _text)
    string(REGEX MATCH "${_pattern}" _hit "${_text}")
    if(NOT _hit STREQUAL "")
        list(APPEND _offenders "${_header}")
    endif()
endforeach()

if(_offenders)
    string(REPLACE ";" "\n  " _list "${_offenders}")
    message(FATAL_ERROR
        "exported function declared `inline` without a body in (drop `inline`):"
        "\n  ${_list}")
endif()
