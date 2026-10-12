# Every CRC spec keeps its constants as in-class `static const` members. Before
# C++17 a member that is odr-used (bound to a reference, as EXPECT_EQ does)
# needs one namespace-scope definition, which LumexCrcCatalog.cpp provides for
# the specs named in LUMEX_CRC_SPEC_LIST. A spec added to the header but not to
# the list links in C++17 and fails with undefined references in C++11/14.

file(READ "${LUMEX_SOURCE_DIR}/lumex/core/crc/parametric/LumexCrcParametric.hpp"
    _header)
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/crc/catalog/LumexCrcCatalog.cpp"
    _catalog)

# A spec definition starts a line and opens its body on the next one.
string(REGEX MATCHALL "\nstruct crc[A-Za-z0-9_]+_spec_t\n{" _definitions
    "${_header}")
list(LENGTH _definitions _count)
if(_count LESS 100)
    message(FATAL_ERROR
        "expected over 100 CRC specs in LumexCrcParametric.hpp, found "
        "${_count}: the pattern no longer matches the header")
endif()

string(FIND "${_catalog}" "#define LUMEX_CRC_SPEC_LIST(X)" _list_pos)
if(_list_pos EQUAL -1)
    message(FATAL_ERROR
        "LumexCrcCatalog.cpp does not define LUMEX_CRC_SPEC_LIST")
endif()

set(_missing "")
foreach(_definition IN LISTS _definitions)
    string(REGEX REPLACE "\nstruct (crc[A-Za-z0-9_]+_spec_t)\n{" "\\1" _name
        "${_definition}")
    string(FIND "${_catalog}" "X (${_name})" _pos)
    if(_pos EQUAL -1)
        list(APPEND _missing "${_name}")
    endif()
endforeach()

if(_missing)
    string(REPLACE ";" "\n  " _list "${_missing}")
    message(FATAL_ERROR
        "CRC specs missing from LUMEX_CRC_SPEC_LIST in LumexCrcCatalog.cpp:"
        "\n  ${_list}")
endif()
