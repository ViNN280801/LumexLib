# reflection CMakeLists and sources must use LumexAggregateFields +
# vendored nlohmann, never Boost.PFR.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_forbid_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} still mentions ${needle}")
    endif()
endfunction()

set(_ref_cmake "lumex/core/reflection/CMakeLists.txt")
set(_ref_hdr "lumex/core/reflection/field_reflection/LumexFieldReflection.hpp")
set(_agg_hdr "lumex/core/reflection/field_reflection/LumexAggregateFields.hpp")

_require_text("${_ref_cmake}" "lumex_setup_nlohmann_json")
_require_text("${_ref_cmake}" "LUMEX_WITH_FIELD_REFLECTION")
_require_text("${_ref_cmake}" "3rdparty")
_require_text("${_ref_cmake}" "nlohmann")
_forbid_text("${_ref_cmake}" "boost_pfr")
_forbid_text("${_ref_cmake}" "boost/pfr")

_require_text("${_ref_hdr}" "LumexAggregateFields.hpp")
_require_text("${_ref_hdr}" "<nlohmann/json.hpp>")
_forbid_text("${_ref_hdr}" "boost/pfr")
_forbid_text("${_ref_hdr}" "boost::pfr")

_require_text("${_agg_hdr}" "tuple_size_v")
_require_text("${_agg_hdr}" "names_as_array")
_require_text("${_agg_hdr}" "k_max_aggregate_fields")
_forbid_text("${_agg_hdr}" "boost/pfr")
_forbid_text("${_agg_hdr}" "boost::pfr")

file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/core/reflection/CMakeLists.txt" _tcmake)
foreach(_suite LumexFieldReflectionTests LumexFieldReflectionGetTests
               LumexFieldReflectionNamesTests)
    string(FIND "${_tcmake}" "${_suite}" _tp)
    if(_tp EQUAL -1)
        message(FATAL_ERROR "reflection tests omit ${_suite}")
    endif()
endforeach()
