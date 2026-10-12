# LumexXml must not pass -fkeep-inline-functions or -fkeep-static-functions.
# They once kept `inline` definitions of exported functions in the .cpp files
# alive on GCC; those definitions are gone (source_no_public_api_inline_in_cpp
# keeps them out). The flags also emit every inline function of every header
# into the library, including GCC 8's inline __gnu_cxx::recursive_init_error
# constructor from <cxxabi.h>, whose vtable libstdc++ does not export, so
# LumexXml failed to link with GCC 8.

file(READ "${LUMEX_SOURCE_DIR}/lumex/xml/CMakeLists.txt" _xml)

# Count code only: a comment may name the options.
string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" _code "${_xml}")
foreach(_flag IN ITEMS -fkeep-inline-functions -fkeep-static-functions)
    string(FIND "${_code}" "${_flag}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR
            "lumex/xml/CMakeLists.txt: ${_flag} is back; it breaks the GCC 8 "
            "link of LumexXml and exports every inline function")
    endif()
endforeach()
